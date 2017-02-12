//#include <Firmata.h>
//#include <Boards.h>

/* 
Basic use of Arduino library for MicroChip MCP4728 I2C D/A converter
For discussion and feedback, please go to http://arduino.cc/forum/index.php/topic,51842.0.html
*/

#include <Wire.h>
#include "mcp4728.h"
#include "Adafruit_VL6180X.h"
#define SMOOTHING 4

Adafruit_VL6180X vl = Adafruit_VL6180X();
mcp4728 dac = mcp4728(0); // instantiate mcp4728 object, Device ID = 0
uint16_t buff[50];
uint8_t write_ptr = 0;
int gate = 13;


void setup()
{
  pinMode(gate, OUTPUT);
  Serial.begin(9600);  // initialize serial interface for print()
  dac.begin();  // initialize i2c interface
  dac.vdd(5000); // set VDD(mV) of MCP4728 for correct conversion between LSB and Vout
  if (! vl.begin()) {
    Serial.println("Failed to find VL6180X Sensor");
    while (1);
  }
  Serial.println("VL6180x Sensor found!");
  dac.setVref(1,1,1,1); // set to use internal voltage reference (2.048V)
  dac.setGain(0, 1); // set the gain of internal voltage reference ( 2.048V x 2 = 4.096V )
  dac.setGain(1, 1); // set the gain of internal voltage reference ( 2.048V x 2 = 4.096V )
  
/* If LDAC pin is not grounded, need to be pull down for normal operation.

  int LDACpin = 8;
  pinMode(LDACpin, OUTPUT); 
  digitalWrite(LDACpin, LOW);
*/
  printStatus(); // Print all internal value and setting of input register and EEPROM. 
}


void loop()
{
  float av_val;

  uint8_t range = vl.readRange();
  uint8_t status = vl.readRangeStatus();
  if (status == VL6180X_ERROR_NONE) {
    // Gate On
    digitalWrite(gate, HIGH);
    // Range values are likely to be approx 50 - 200
    if(range >= 200){
      range = 200;
    }
    buff[write_ptr] = (200-range)*20;
    if(write_ptr++ >= SMOOTHING) { 
      write_ptr = 0;
    }
    // Average this with previous
    av_val = 0;
    int i = 0;
    for(i = 0; i < SMOOTHING; i++) {
       av_val += buff[i];
    }
    av_val = av_val/SMOOTHING;
/*    Serial.print(" Write Ptr: ");
    Serial.print(write_ptr);
    Serial.print(" Average: ");
    Serial.println(av_val);*/
    dac.analogWrite(av_val,av_val,av_val,av_val);
  }
  else if (status == VL6180X_ERROR_NOCONVERGE) {
     range = 200;
     digitalWrite(gate, LOW); 
  }
}

void printStatus()
{
  Serial.println("NAME     Vref  Gain  PowerDown  Value");
  for (int channel=0; channel <= 3; channel++)
  { 
    Serial.print("DAC");
    Serial.print(channel,DEC);
    Serial.print("   ");
    Serial.print("    "); 
    Serial.print(dac.getVref(channel),BIN);
    Serial.print("     ");
    Serial.print(dac.getGain(channel),BIN);
    Serial.print("       ");
    Serial.print(dac.getPowerDown(channel),BIN);
    Serial.print("       ");
    Serial.println(dac.getValue(channel),DEC);

    Serial.print("EEPROM");
    Serial.print(channel,DEC);
    Serial.print("    "); 
    Serial.print(dac.getVrefEp(channel),BIN);
    Serial.print("     ");
    Serial.print(dac.getGainEp(channel),BIN);
    Serial.print("       ");
    Serial.print(dac.getPowerDownEp(channel),BIN);
    Serial.print("       ");
    Serial.println(dac.getValueEp(channel),DEC);
  }
  Serial.println(" ");
}
