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
int quantize_val(int, int, int);

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
int p_intonation = 5; // Internal pullup, pulled low by external jumper (sets intonation to Just or Equal)
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
  pinMode(p_intonation, INPUT_PULLUP);
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
  int intonation = digitalRead(p_intonation);
  int D0 = digitalRead(p_D0);
  int D1 = digitalRead(p_D1);
  int D2 = digitalRead(p_D2);
  int D3 = digitalRead(p_D3);
  quantize = 0;
  if(D0 == 0) quantize += 1;
  if(D1 == 0) quantize += 2;
  if(D2 == 0) quantize += 4;
  if(D3 == 0) quantize += 8;
  if((quantize < 0) || (quantize > 9)) quantize = 0;
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
    
    av_val = quantize_val(av_val, quantize, intonation);

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
// the chosen scale, and intonaztion
// (Equal temperament or Just Intonation)
int quantize_val(int in_val, int scale, int intonation){
  int retval;
  switch(intonation){
    // Just Intonation
    case 0:
      //Serial.println("Just Intonation");
      switch(scale){
        case 0:
            retval = in_val;  // scale == 0 don't quantize
          break;
        case 1:
          // Chromatic (Just Intonation)
          switch(in_val){
            case 0 ... 162:
                retval = 0;
                break;
            case 163 ... 325:
                retval = 150;
                break;
            case 326 ... 488:
                retval = 340;
                break;
            case 489 ... 651:
                retval = 490;
                break;
            case 652 ... 814:
                retval = 680;
                break;
            case 815 ... 977:
                retval = 830;
                break;
            case 978 ... 1140:
                retval = 980;
                break;
            case 1141 ... 1303:
                retval = 1170;
                break;
            case 1304 ... 1466:
                retval = 1320;
                break;
            case 1467 ... 1629:
                retval = 1510;
                break;
            case 1630 ... 1792:
                retval = 1660;
                break;
            case 1793 ... 1955:
                retval = 1850;
                break;
            case 1956 ... 2118:
                retval = 2000;
                break;
            case 2119 ... 2281:
                retval = 2150;
                break;
            case 2282 ... 2444:
                retval = 2340;
                break;
            case 2445 ... 2607:
                retval = 2490;
                break;
            case 2608 ... 2770:
                retval = 2680;
                break;
            case 2771 ... 2933:
                retval = 2830;
                break;
            case 2934 ... 3096:
                retval = 2980;
                break;
            case 3097 ... 3259:
                retval = 3170;
                break;
            case 3260 ... 3422:
                retval = 3320;
                break;
            case 3423 ... 3585:
                retval = 3510;
                break;
            case 3586 ... 3748:
                retval = 3660;
                break;
            case 3749 ... 3911:
                retval = 3850;
                break;
            case 3912 ... 4096:
                retval = 4000;
                break;
          }
          break;
        case 2:
          // Major (Just Intonation)
          switch(in_val){
            case 0 ... 272:
                retval = 0;
                break;
            case 273 ... 545:
                retval = 340;
                break;
            case 546 ... 818:
                retval = 680;
                break;
            case 819 ... 1091:
                retval = 830;
                break;
            case 1092 ... 1364:
                retval = 1170;
                break;
            case 1365 ... 1637:
                retval = 1510;
                break;
            case 1638 ... 1910:
                retval = 1850;
                break;
            case 1911 ... 2183:
                retval = 2000;
                break;
            case 2184 ... 2456:
                retval = 2340;
                break;
            case 2457 ... 2729:
                retval = 2680;
                break;
            case 2730 ... 3002:
                retval = 2830;
                break;
            case 3003 ... 3275:
                retval = 3170;
                break;
            case 3276 ... 3548:
                retval = 3510;
                break;
            case 3549 ... 3821:
                retval = 3850;
                break;
            case 3822 ... 4096:
                retval = 4000;
                break;
          }
          break;
        case 3:
          // Major Pentatonic (Just Intonation)
          switch(in_val){
            case 0 ... 371:
                retval = 0;
                break;
            case 372 ... 743:
                retval = 340;
                break;
            case 744 ... 1115:
                retval = 680;
                break;
            case 1116 ... 1487:
                retval = 1170;
                break;
            case 1488 ... 1859:
                retval = 1510;
                break;
            case 1860 ... 2231:
                retval = 2000;
                break;
            case 2232 ... 2603:
                retval = 2340;
                break;
            case 2604 ... 2975:
                retval = 2680;
                break;
            case 2976 ... 3347:
                retval = 3170;
                break;
            case 3348 ... 3719:
                retval = 3510;
                break;
            case 3720 ... 4096:
                retval = 4000;
                break;
          }
        
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
        default:
            retval = in_val;  // default = don't quantize
          break;
      }
    
      break;
    // Equal Temperament
    case 1:
      //Serial.println("Equal Temperament");
      switch(scale){
        case 0:
            retval = in_val;  // scale == 0 don't quantize
          break;
        case 1:
          // Chromatic (Equal Temperament)
          switch(in_val){
            case 0 ... 162:
                retval = 0;
                break;
            case 163 ... 325:
                retval = 168;
                break;
            case 326 ... 488:
                retval = 333;
                break;
            case 489 ... 651:
                retval = 500;
                break;
            case 652 ... 814:
                retval = 667;
                break;
            case 815 ... 977:
                retval = 833;
                break;
            case 978 ... 1140:
                retval = 1000;
                break;
            case 1141 ... 1303:
                retval = 1167;
                break;
            case 1304 ... 1466:
                retval = 1333;
                break;
            case 1467 ... 1629:
                retval = 1500;
                break;
            case 1630 ... 1792:
                retval = 1667;
                break;
            case 1793 ... 1955:
                retval = 1833;
                break;
            case 1956 ... 2118:
                retval = 2000;
                break;
            case 2119 ... 2281:
                retval = 2167;
                break;
            case 2282 ... 2444:
                retval = 2333;
                break;
            case 2445 ... 2607:
                retval = 2500;
                break;
            case 2608 ... 2770:
                retval = 2667;
                break;
            case 2771 ... 2933:
                retval = 2833;
                break;
            case 2934 ... 3096:
                retval = 3000;
                break;
            case 3097 ... 3259:
                retval = 3167;
                break;
            case 3260 ... 3422:
                retval = 3333;
                break;
            case 3423 ... 3585:
                retval = 3500;
                break;
            case 3586 ... 3748:
                retval = 3667;
                break;
            case 3749 ... 3911:
                retval = 3833;
                break;
            case 3912 ... 4096:
                retval = 4000;
                break;
          }
          break;
        case 2:
          // Major (Equal Temperament)
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
          // Major Pentatonic (Equal Temperament)
          switch(in_val){
            case 0 ... 371:
                retval = 0;
                break;
            case 372 ... 743:
                retval = 333;
                break;
            case 744 ... 1115:
                retval = 667;
                break;
            case 1116 ... 1487:
                retval = 1167;
                break;
            case 1488 ... 1859:
                retval = 1500;
                break;
            case 1860 ... 2231:
                retval = 2000;
                break;
            case 2232 ... 2603:
                retval = 2333;
                break;
            case 2604 ... 2975:
                retval = 2667;
                break;
            case 2976 ... 3347:
                retval = 3167;
                break;
            case 3348 ... 3719:
                retval = 3500;
                break;
            case 3720 ... 4096:
                retval = 4000;
                break;
          }

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
        default:
            retval = in_val;  // default = don't quantize
          break;
      }
    
      break;
  }
  return retval;
}

