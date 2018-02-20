// Copyright 2016 Richard R. Goodwin / Audio Morphology.
//
// Author: Richard R. Goodwin (richard.goodwin@morphology.co.uk)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// 19-Feb-2018
#line 1 "waft.ino"

#include "Arduino.h"
void setup();
void loop();
#line 4
void printStatus();
int quantize_val(int, int);

#include <Wire.h>
#include "mcp4728.h"
#include "Adafruit_VL6180X.h"
#define SMOOTHING 4
#define MOVEMENT_HYSTERESIS 100

Adafruit_VL6180X vl = Adafruit_VL6180X();
mcp4728 dac = mcp4728(0); // instantiate mcp4728 object, Device ID = 0
uint16_t buff[50];
uint8_t write_ptr = 0;
int p_gate1 = 2;     // Gate 1 is triggered when ANY range signal is detected
int p_gate2 = 3;     // gate 2 is triggered by MOVEMENT
int p_invert = 6;     // Internal Pullup, pulled low by external jumper
int p_D0 = 7;
int p_D1 = 8;
int p_D2 = 9;
int p_D3 = 10;
int quantize = 0;
float prev_av = 0;

void setup()
{
  pinMode(p_gate1, OUTPUT);
  pinMode(p_gate2, OUTPUT);
  pinMode(p_invert, INPUT_PULLUP);
  pinMode(p_D0, INPUT_PULLUP);
  pinMode(p_D1, INPUT_PULLUP);
  pinMode(p_D2, INPUT_PULLUP);
  pinMode(p_D3, INPUT_PULLUP);
  Serial.begin(9600);  // initialize serial interface for print()
  Serial.println("Waft by Audio Morphology");
  Serial.println("Copyright 2016 Richard R. Goodwin / Audio Morphology.");
  Serial.println(" ");
  dac.begin();  // initialize i2c interface
  dac.vdd(5000); // set VDD(mV) of MCP4728 for correct conversion between LSB and Vout
  if (! vl.begin()) {
    Serial.println("Failed to find VL6180X Sensor\n");
    while (1);
  }
  Serial.println("VL6180x Sensor found!\n");
  dac.setVref(1,1,1,1); // set to use internal voltage reference (2.048V)
  dac.setGain(0, 0); // set the gain of internal voltage reference ( 2.048V x 2 = 4.096V )
  dac.setGain(1, 1); // set the gain of internal voltage reference ( 2.048V x 2 = 4.096V )
  printStatus(); // Print all internal value and setting of input register and EEPROM. 
}


void loop()
{
  float av_val;
  float no_quant;
  int invert = digitalRead(p_invert);
  int D0 = digitalRead(p_D0);
  int D1 = digitalRead(p_D1);
  int D2 = digitalRead(p_D2);
  int D3 = digitalRead(p_D3);
  quantize = 0;
  if(D0 == 0) quantize += 1;
  if(D1 == 0) quantize += 2;
  if(D2 == 0) quantize += 4;
  if(D3 == 0) quantize += 8;
  if(quantize > 9) quantize = 0;
  uint8_t range = vl.readRange();
  uint8_t status = vl.readRangeStatus();
  if (status == VL6180X_ERROR_NONE) {
    // Gate On
    digitalWrite(p_gate1, HIGH);
    // Range values are likely to be approx 50 - 200
    if(range >= 200){
      range = 200;
    }
    if(invert == LOW){
      buff[write_ptr] = (200-range)*20;
    }
    else {
      buff[write_ptr] = (range)*20;
    }
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
    no_quant = av_val;
    //Serial.print("Prev Av: ");
    //Serial.print(prev_av);
    //Serial.print(" Average: ");
    //Serial.println(av_val);
    
    av_val = quantize_val(av_val, quantize);

    // DAC Channel 1 = 2 Octave range, quantised where necessary
    // DAC Channel 2 = 4v Range, never quantised
    dac.analogWrite(av_val,no_quant,av_val,av_val);
    // Gate2 On if the value has changed
    if((av_val <= prev_av - MOVEMENT_HYSTERESIS) || (av_val >= prev_av + MOVEMENT_HYSTERESIS)) {
      prev_av = av_val;
      digitalWrite(p_gate2, HIGH); 
    }
    else {
      digitalWrite(p_gate2, LOW); 
    }
  }
  else if (status == VL6180X_ERROR_NOCONVERGE) {
     range = 200;
     digitalWrite(p_gate1, LOW); 
     digitalWrite(p_gate2, LOW);
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

// Qantizes the passed input value
// to different scales depending on
// the chosen scale
int quantize_val(int in_val, int scale){
  int retval;
  switch(scale){
    case 0:
        retval = in_val;  // scale == 0 don't quantize
      break;
    case 1:
      // Semitones
      switch(in_val){
        case 0 ... 84:
            retval = 0;
            break;
        case 85 ... 251:
            retval = 168;
            break;
        case 252 ... 417:
            retval = 333;
            break;
        case 418 ... 584:
            retval = 500;
            break;
        case 585 ... 750:
            retval = 667;
            break;
        case 751 ... 917:
            retval = 833;
            break;
        case 918 ... 1084:
            retval = 1000;
            break;
        case 1085 ... 1250:
            retval = 1167;
            break;
        case 1251 ... 1417:
            retval = 1333;
            break;
        case 1418 ... 1584:
            retval = 1500;
            break;
        case 1585 ... 1750:
            retval = 1667;
            break;
        case 1751 ... 1917:
            retval = 1833;
            break;
        case 1918 ... 2084:
            retval = 2000;
            break;
        case 2085 ... 2250:
            retval = 2167;
            break;
        case 2251 ... 2417:
            retval = 2333;
            break;
        case 2418 ... 2584:
            retval = 2500;
            break;
        case 2585 ... 2750:
            retval = 2667;
            break;
        case 2751 ... 2917:
            retval = 2833;
            break;
        case 2918 ... 3084:
            retval = 3000;
            break;
        case 3085 ... 3250:
            retval = 3167;
            break;
        case 3251 ... 3417:
            retval = 3333;
            break;
        case 3418 ... 3584:
            retval = 3500;
            break;
        case 3585 ... 3750:
            retval = 3667;
            break;
        case 3751 ... 3917:
            retval = 3833;
            break;
        case 3918 ... 4096:
            retval = 4000;
            break;
      }
      break;
    case 2:
      // Major
      switch(in_val){
        case 0 ... 272:
            retval = 0;
            break;
        case 273 ... 545:
            retval = 333;
            break;
        case 546 ... 818:
            retval = 667;
            break;
        case 819 ... 1091:
            retval = 833;
            break;
        case 1092 ... 1364:
            retval = 1167;
            break;
        case 1365 ... 1637:
            retval = 1500;
            break;
        case 1638 ... 1910:
            retval = 1833;
            break;
        case 1911 ... 2183:
            retval = 2000;
            break;
        case 2184 ... 2456:
            retval = 2333;
            break;
        case 2457 ... 2729:
            retval = 2667;
            break;
        case 2730 ... 3002:
            retval = 2833;
            break;
        case 3003 ... 3275:
            retval = 3167;
            break;
        case 3276 ... 3548:
            retval = 3500;
            break;
        case 3549 ... 3821:
            retval = 3833;
            break;
        case 3822 ... 4096:
            retval = 4000;
            break;
      }

      break;
    case 3:

      break;
    case 4:

      break;
    case 5:

      break;

    case 6:

      break;
    case 7:

      break;
    case 8:

      break;
    case 9:

      break;
  }
  return retval;
}

