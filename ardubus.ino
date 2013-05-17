/*
    ArduBus is an Arduino program that eases the task of interfacing
    a new I2C device by using custom commands, so no programming is
    required.
    Copyright (C) 2010 Santiago Reig
    English Translation by Geeky Star 2013

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ArduBus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ArduBus.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Wire.h>
#define BUFFSIZ 50                     //Port buffer size large enough number
#define BAUD 115200                    //Serial port speed
#define MCU_FREQ 16000000L             //Operating frequency of the microcontroller
boolean error;
boolean echo = true;                   //To echo the serial input
boolean debug = false;                 //Enable debug messages
char buffer[BUFFSIZ];                  //Warehouse chain for serial input
char *parseptr;                        //Pointer to traverse the vector

void setup()
{
  Serial.begin(BAUD);                                            //We started the serial port
  Wire.begin();                                                  //We started the I2C bus

  Serial.println("");                                            //Welcome Message
  Serial.println("ArduBus v0.3");
  Serial.println("KungFu Labs - http://www.kungfulabs.com");
  Serial.println("ArduBus Copyright (C) 2010  Santiago Reig");
  Serial.println("");
  selectSpeed();
  Serial.println("202 I2C READY");
  Serial.println("205 I2C PULLUP ON");
}

void loop()
{
  error = false;                                                 //Limpimos error flag
  Serial.print("I2C>");                                          //We write the 'prompt'
  readString();                                                  //We read the typed command through the serial port
  startWork();                                                   //Execute the actions listed
}

void readString() {
  char c;
  int buffSize = 0;

  parseptr = buffer;
  Serial.flush();
  while (true) {
      while (Serial.available() == 0);                           //We wait for another character
      c=Serial.read();
      if (c == 127){
        if (buffSize != 0){                                      //If you delete a character, go back one position
          buffSize--;                                            //We avoid creating negative values
          if (echo){
            Serial.print(c);
          }
        }
        continue;
      }
      if (echo){                                                 //We echo
        if (c == '\r') Serial.println();                         //We started a new line
        else Serial.print(c);
      }
      if ((buffSize == BUFFSIZ-1) || (c == '\r')) {              //We read to detect the intro
        buffer[buffSize] = 0;                                    //In the last position we write a 0 to define the end of the chain
        return;
      }
      buffer[buffSize++] = c;                                    //Save the received character
  }
}

void selectSpeed(){
  byte i2cSpeed;
  char option[] = "Select I2C bus speed:\r\n1. 50 kHz\r\n2. 100 kHz\r\n3. 400 kHz";
  i2cSpeed = selectMenu(option,3);
  switch (i2cSpeed){
    case 1:
      Serial.println("50 kHz selected");
      TWBR = ((MCU_FREQ/50000)-16)/2;
      break;
    case 2:
      Serial.println("100 kHz selected");
      TWBR = ((MCU_FREQ/100000)-16)/2;
      break;
    case 3:
      Serial.println("400 kHz selected");
      TWBR = ((MCU_FREQ/400000)-16)/2;
      break;
  }
}

byte selectMenu(char* options,int len){				 //Show menu and return the value chosen
  Serial.println(options);
  do{
    Serial.print("I2C>");
    readString();
    if (parseptr[0] < '0' || parseptr[0] > '9' || parseptr[0]-'0' > len) Serial.println("ERROR: Option not recognized");
  } while (parseptr[0] < '0' || parseptr[0] > '9' || parseptr[0]-'0' > len);  //Limit of 10 options to select
  return parseptr[0]-'0';
}

void startWork(){
  byte address,data,nReads;

  if (parseptr[0] == 'E'){                                       //ECHO Command
    echo = !echo;
    if (echo) Serial.println("Echo activated");
    else Serial.println("Echo disabled");
    return;
  }
  else if (parseptr[0] == 'D'){                                  //DEBUG Command
    debug = !debug;
    if (debug) Serial.println("Debug activated");
    else Serial.println("Debug disabled");
    return;
  }
  if (debug){
      Serial.print("Processing chain: ");
      Serial.println(buffer);
  }
  while (parseptr[0] == '{'){                                    //While a new command ...
    parseptr++;                                                  //We advance to discuss the next character
    Serial.println("210 I2C START CONDITION");
    address = parseArgument();                                   //The first argument is the address
    if (error){
          Serial.println("ERROR: Syntax not recognized");
          return;
    }
    if (parseptr[1] != 'r' && parseptr[2] != 'r'){               //If the next (r) or the second character (0r), counting the space, is an 'r', so reading
      Wire.beginTransmission(address);                           //Open communication with the slave
      Serial.print("220 I2C WRITE: 0x");
      Serial.println(address,HEX);
      while (parseptr[0] != '}' && parseptr[0] != 0){            //We're writing data to find the key to the end of command or vector runs out
        data = parseArgument();                                  //Cojemos the value of the data by analyzing the text string
        if (error){
          Wire.endTransmission();
          Serial.println("ERROR: Syntax not recognized");
          return;
        }
        Wire.write(data);                                         // We send data
        Serial.print("220 I2C WRITE: 0x");
        Serial.println(data,HEX);
      }
      Wire.endTransmission();                                    //Once already written everything, we end the connection
      Serial.println("240 I2C STOP CONDITION");
      parseptr++;
    }
    else {
      nReads = parseArgument();                                  //We read the number of bytes that you want to read
      if (error){
        Serial.println("ERROR: Syntax not recognized");
        return;
      }
      Wire.requestFrom(address,nReads);                          //We ask the slave data
      while(Wire.available()){                                   //According to we receive them go writing them
        Serial.print("230 I2C READ: 0x");                        //POSSIBLE BUG: read more fast that is received and exits the loop, do loop with nReads
        Serial.println(Wire.read(),HEX);
      }
      Serial.println("240 I2C STOP CONDITION");
    }
  }
}

byte parseArgument(){
  byte argument;

  if (parseptr[0] == ' '){                                       //If we detect a space we skip it
    if (debug) Serial.println("Detected space");
    parseptr++;
  }
  if (parseptr[0] >= '1' && parseptr[0] <= '9'){                 //Detection for decimal type: '15'
    if (debug) Serial.print("Detected #dec");
    argument = parseDec();
  }
  else if (parseptr[0] == '0'){
    parseptr++;
    if (parseptr[0] == ' ' || parseptr[0] == '}' || parseptr[0] == 0) argument = 0; //Si hay un 0 seguido de uno de esos caracteres, es que es un 0 decimal, no 0xYY,0bYYYYY,...
    else if (parseptr[0] == 'x' || parseptr[0] == 'h'){
      parseptr++;
      if (debug) Serial.print("Detected #hex");                 //Detection for hexadecimal type: '0x4F'
      argument = parseHex();
    }
    else if (parseptr[0] == 'b'){
      parseptr++;
      if (debug) Serial.print("Detected #bin");                 //Detection for binary type: '0b110101'
      argument = parseBin();
    }
    else if (parseptr[0] == 'd'){
      parseptr++;
      if (debug) Serial.print("Detected #dec");                 //Detection for decimal type: '0d15'
      argument = parseDec();
    }
    else if (parseptr[0] == 'r'){
      parseptr++;
      if (debug) Serial.print("Detected #read");
      argument = parseDec();                                     //We use parseDec since 0rXX is a decimal number
    }
  }
  else if (parseptr[0] == 'r'){                                  //Posting 'r' type chain: 'rrrrrrr'
    if (debug) Serial.print("Detected #read");
    argument = parseRead();
  }
  else{
    error=true;
    parseptr++;
  }
  return argument;
}

byte parseRead(){
  byte result = 0;

  while (parseptr[0] == 'r'){
    result++;
    parseptr++;
  }
  if (debug){
    Serial.print(" - 0r");                                       //We show the converted value
    Serial.println(result,HEX);
  }
  return result;
}

byte parseHex(){                                                 //Convert text to hexadecimal number
  byte result = 0;

  while (parseptr[0] != ' ' && parseptr[0] != '}' && parseptr[0] != 0){
    if (parseptr[0] >= '0' && parseptr[0] <= '9'){
      result *= 16;
      result += parseptr[0]-'0';
    }
    else if (parseptr[0] >= 'a' && parseptr[0] <= 'f'){
      result *= 16;
      result += parseptr[0]-'a'+10;
    }
    else if (parseptr[0] >= 'A' && parseptr[0] <= 'F'){
      result *= 16;
      result += parseptr[0]-'A'+10;
    }
    else return result;
    parseptr++;
  }
  if (debug){
    Serial.print(" - 0x");                                       //We show the converted value
    Serial.println(result,HEX);
  }
  return result;
}

byte parseDec(){
  byte result = 0;

  while (parseptr[0] != ' ' && parseptr[0] != '}' && parseptr[0] != 0){
    if ((parseptr[0] < '0') || (parseptr[0] > '9'))
      return result;
    result *= 10;
    result += parseptr[0]-'0';
    parseptr++;
  }
  if (debug){
    Serial.print(" - 0x");                                       //We show the converted value
    Serial.println(result,HEX);
  }
  return result;
}

byte parseBin(){
  byte result = 0;

  while (parseptr[0] != ' ' && parseptr[0] != '}' && parseptr[0] != 0){
    if ((parseptr[0] < '0') || (parseptr[0] > '1'))
      return result;
    result *= 2;
    result += parseptr[0]-'0';
    parseptr++;
  }
  if (debug){
    Serial.print(" - 0x");                                       //We show the converted value
    Serial.println(result,HEX);
  }
  return result;
}
