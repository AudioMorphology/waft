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
#include "vl6180x_api.h"
void setup();
void loop();
#line 4
void printStatus();
int quantize_val(int, int, int);
void log_msg(const char*, ...);

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
int p_debug = 4;      // Pin indicates whether to transmit debug messages
int p_D0 = 7;
int p_D1 = 8;
int p_D2 = 9;
int p_D3 = 10;
int quantize = 0;
int debug = 1;
float prev_av = 0;

void setup()
{
  pinMode(p_gate1, OUTPUT);
  pinMode(p_gate2, OUTPUT);
  pinMode(p_invert, INPUT_PULLUP);
  pinMode(p_intonation, INPUT_PULLUP);
  pinMode(p_debug, INPUT_PULLUP);
  pinMode(p_D0, INPUT_PULLUP);
  pinMode(p_D1, INPUT_PULLUP);
  pinMode(p_D2, INPUT_PULLUP);
  pinMode(p_D3, INPUT_PULLUP);
  Serial.begin(9600);  // initialize serial interface for print()
  //NOTE: Always print copyright to the Serial port - don't use the log_msg() wrapper
  Serial.println("Waft by Audio Morphology");
  Serial.println("Copyright 2016 Richard R. Goodwin / Audio Morphology.");
  Serial.println(" ");
  dac.begin();  // initialize i2c interface
  dac.vdd(5000); // set VDD(mV) of MCP4728 for correct conversion between LSB and Vout
  if (! vl.begin()) {
    log_msg("Failed to find VL6180X Sensor\n\r");
    while (1);
  }
  log_msg("VL6180x Sensor found!\n\r");
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
  debug = digitalRead(p_debug);
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
  if(range != 255) log_msg("Range: %d\n\r",range);
  uint8_t status = vl.readRangeStatus();
  if (status == VL6180X_ERROR_NONE) {
    // Gate On
    digitalWrite(p_gate1, HIGH);
    // Range values are likely to be approx 15 - 190
    // but treat the useable range as 15 - 175
    // so scale these to a 0 - 4000 value
    if(range > 15){range -= 15;} else {range = 0;}
    if(range > 160){range = 160;}
    //log_msg("Range: %d\n\r",range);
    if(invert == LOW){
      buff[write_ptr] = (160-range)*25;
    }
    else {
      buff[write_ptr] = (range)*25;
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
    
    // Quantise appropriately
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
  else {
     // basically treat all returned values other 
     // than VL6180X_ERROR_NONE as no signal, so gate off
     digitalWrite(p_gate1, LOW); 
     digitalWrite(p_gate2, LOW);
  }
//#define VL6180X_ERROR_NONE         0   ///< Success!
//#define VL6180X_ERROR_SYSERR_1     1   ///< System error
//#define VL6180X_ERROR_SYSERR_5     5   ///< Sysem error
//#define VL6180X_ERROR_ECEFAIL      6   ///< Early convergence estimate fail  
//#define VL6180X_ERROR_NOCONVERGE   7   ///< No target detected
//#define VL6180X_ERROR_RANGEIGNORE  8   ///< Ignore threshold check failed
//#define VL6180X_ERROR_SNR          11  ///< Ambient conditions too high
//#define VL6180X_ERROR_RAWUFLOW     12  ///< Raw range algo underflow
//#define VL6180X_ERROR_RAWOFLOW     13  ///< Raw range algo overflow
//#define VL6180X_ERROR_RANGEUFLOW   14  ///< Raw range algo underflow
//#define VL6180X_ERROR_RANGEOFLOW   15  ///< Raw range algo overflow
}

void printStatus()
{
  log_msg("NAME     Vref  Gain  PowerDown  Value\n");
  for (int channel=0; channel <= 3; channel++)
  {  
    log_msg("DAC%d       %d     %d       %d       %d\n\r",channel,dac.getVref(channel),dac.getGain(channel),dac.getPowerDown(channel),dac.getValue(channel));
    log_msg("EEPROM%d    %d     %d       %d       %d\n\r",channel,dac.getVrefEp(channel),dac.getGainEp(channel),dac.getPowerDownEp(channel),dac.getValueEp(channel));
  }
  log_msg(" \n\r");
}

// Wrapper for the Serial.print function that
// enables us to turn off debug messages. This
// is simply because the overhead of writing
// debug messages to the serial port slows down
// the main program loop
void log_msg(const char* format, ...)
{
   char buf[128];
  if (debug == 1){
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  Serial.print(buf);
  va_end(args);
  }
}

// Qantizes the passed input value
// to different scales depending on
// the chosen scale, and intonaztion
// (Equal temperament or Just Intonation)
int quantize_val(int in_val, int scale, int intonation){
  int retval;
  //log_msg("In val: %d\n\r",in_val);
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
            case 0 ... 159:
                retval = 0;
                break;
            case 160 ... 319:
                retval = 150;
                break;
            case 320 ... 479:
                retval = 340;
                break;
            case 480 ... 639:
                retval = 490;
                break;
            case 640 ... 799:
                retval = 680;
                break;
            case 800 ... 959:
                retval = 830;
                break;
            case 960 ... 1119:
                retval = 980;
                break;
            case 1120 ... 1279:
                retval = 1170;
                break;
            case 1280 ... 1439:
                retval = 1320;
                break;
            case 1440 ... 1599:
                retval = 1510;
                break;
            case 1600 ... 1759:
                retval = 1660;
                break;
            case 1760 ... 1919:
                retval = 1850;
                break;
            case 1920 ... 2079:
                retval = 2000;
                break;
            case 2080 ... 2239:
                retval = 2150;
                break;
            case 2240 ... 2399:
                retval = 2340;
                break;
            case 2400 ... 2559:
                retval = 2490;
                break;
            case 2560 ... 2719:
                retval = 2680;
                break;
            case 2720 ... 2879:
                retval = 2830;
                break;
            case 2880 ... 3039:
                retval = 2980;
                break;
            case 3040 ... 3199:
                retval = 3170;
                break;
            case 3200 ... 3359:
                retval = 3320;
                break;
            case 3360 ... 3519:
                retval = 3510;
                break;
            case 3520 ... 3679:
                retval = 3660;
                break;
            case 3680 ... 3839:
                retval = 3850;
                break;
            case 3840 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 2:
          // Major (Just Intonation)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 340;
                break;
            case 534 ... 800:
                retval = 680;
                break;
            case 801 ... 1067:
                retval = 830;
                break;
            case 1068 ... 1334:
                retval = 1170;
                break;
            case 1335 ... 1601:
                retval = 1510;
                break;
            case 1602 ... 1868:
                retval = 1850;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2340;
                break;
            case 2403 ... 2669:
                retval = 2680;
                break;
            case 2670 ... 2936:
                retval = 2830;
                break;
            case 2937 ... 3203:
                retval = 3170;
                break;
            case 3204 ... 3470:
                retval = 3510;
                break;
            case 3471 ... 3737:
                retval = 3850;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 3:
          // Major Pentatonic (Just Intonation)
          switch(in_val){
            case 0 ... 363:
                retval = 0;
                break;
            case 364 ... 727:
                retval = 340;
                break;
            case 728 ... 1091:
                retval = 680;
                break;
            case 1092 ... 1455:
                retval = 1170;
                break;
            case 1456 ... 1819:
                retval = 1510;
                break;
            case 1820 ... 2183:
                retval = 2000;
                break;
            case 2184 ... 2547:
                retval = 2340;
                break;
            case 2548 ... 2911:
                retval = 2680;
                break;
            case 2912 ... 3275:
                retval = 3170;
                break;
            case 3276 ... 3639:
                retval = 3510;
                break;
            case 3640 ... 4000:
                retval = 4000;
                break;
          }
        
          break;
        case 4:
          // Minor Pentatonic (Just Intonation)
          switch(in_val){
            case 0 ... 363:
                retval = 0;
                break;
            case 364 ... 727:
                retval = 370;
                break;
            case 728 ... 1091:
                retval = 667;
                break;
            case 1092 ... 1455:
                retval = 1000;
                break;
            case 1456 ... 1819:
                retval = 1555;
                break;
            case 1820 ... 2183:
                retval = 2000;
                break;
            case 2184 ... 2547:
                retval = 2370;
                break;
            case 2548 ... 2911:
                retval = 2667;
                break;
            case 2912 ... 3275:
                retval = 3000;
                break;
            case 3276 ... 3639:
                retval = 3555;
                break;
            case 3640 ... 4000:
                retval = 4000;
                break;
          }
        
          break;
        case 5:
          // Harmonic Minor (Just Intonation)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 340;
                break;
            case 534 ... 800:
                retval = 490;
                break;
            case 801 ... 1067:
                retval = 830;
                break;
            case 1068 ... 1334:
                retval = 1170;
                break;
            case 1335 ... 1601:
                retval = 1320;
                break;
            case 1602 ... 1868:
                retval = 1850;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2340;
                break;
            case 2403 ... 2669:
                retval = 2490;
                break;
            case 2670 ... 2936:
                retval = 2830;
                break;
            case 2937 ... 3203:
                retval = 3170;
                break;
            case 3204 ... 3470:
                retval = 3320;
                break;
            case 3471 ... 3737:
                retval = 3850;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 6:
          // Dorian (Just Intonation)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 340;
                break;
            case 534 ... 800:
                retval = 490;
                break;
            case 801 ... 1067:
                retval = 830;
                break;
            case 1068 ... 1334:
                retval = 1170;
                break;
            case 1335 ... 1601:
                retval = 1510;
                break;
            case 1602 ... 1868:
                retval = 1660;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2340;
                break;
            case 2403 ... 2669:
                retval = 2490;
                break;
            case 2670 ... 2936:
                retval = 2830;
                break;
            case 2937 ... 3203:
                retval = 3170;
                break;
            case 3204 ... 3470:
                retval = 3510;
                break;
            case 3471 ... 3737:
                retval = 3660;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
    
          break;
        case 7:
          // Phrygian (Just Intonation)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 150;
                break;
            case 534 ... 800:
                retval = 490;
                break;
            case 801 ... 1067:
                retval = 830;
                break;
            case 1068 ... 1334:
                retval = 1170;
                break;
            case 1335 ... 1601:
                retval = 1320;
                break;
            case 1602 ... 1868:
                retval = 1660;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2150;
                break;
            case 2403 ... 2669:
                retval = 2490;
                break;
            case 2670 ... 2936:
                retval = 2830;
                break;
            case 2937 ... 3203:
                retval = 3170;
                break;
            case 3204 ... 3470:
                retval = 3320;
                break;
            case 3471 ... 3737:
                retval = 3660;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }

          break;
        case 8:
          // Symmetric Octatonic Half Whole (Just Intonation)
          switch(in_val){
            case 0 ... 234:
                retval = 0;
                break;
            case 235 ... 469:
                retval = 150;
                break;
            case 470 ... 704:
                retval = 490;
                break;
            case 705 ... 939:
                retval = 680;
                break;
            case 940 ... 1174:
                retval = 980;
                break;
            case 1175 ... 1409:
                retval = 1170;
                break;
            case 1410 ... 1644:
                retval = 1510;
                break;
            case 1645 ... 1879:
                retval = 1660;
                break;
            case 1880 ... 2114:
                retval = 2000;
                break;
            case 2115 ... 2349:
                retval = 2150;
                break;
            case 2350 ... 2584:
                retval = 2490;
                break;
            case 2585 ... 2819:
                retval = 2680;
                break;
            case 2820 ... 3054:
                retval = 2980;
                break;
            case 3055 ... 3289:
                retval = 3170;
                break;
            case 3290 ... 3524:
                retval = 3510;
                break;
            case 3525 ... 3759:
                retval = 3660;
                break;
            case 3760 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 9:
          // Symmetric Octatonic Whole half (Just Intonation)
          switch(in_val){
            case 0 ... 234:
                retval = 0;
                break;
            case 235 ... 469:
                retval = 340;
                break;
            case 470 ... 704:
                retval = 490;
                break;
            case 705 ... 939:
                retval = 830;
                break;
            case 940 ... 1174:
                retval = 980;
                break;
            case 1175 ... 1409:
                retval = 1320;
                break;
            case 1410 ... 1644:
                retval = 1510;
                break;
            case 1645 ... 1879:
                retval = 1850;
                break;
            case 1880 ... 2114:
                retval = 2000;
                break;
            case 2115 ... 2349:
                retval = 2340;
                break;
            case 2350 ... 2584:
                retval = 2490;
                break;
            case 2585 ... 2819:
                retval = 2830;
                break;
            case 2820 ... 3054:
                retval = 2980;
                break;
            case 3055 ... 3289:
                retval = 3320;
                break;
            case 3290 ... 3524:
                retval = 3510;
                break;
            case 3525 ... 3759:
                retval = 3850;
                break;
            case 3760 ... 4000:
                retval = 4000;
                break;
          }
    
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
            case 0 ... 159:
                retval = 0;
                break;
            case 160 ... 319:
                retval = 168;
                break;
            case 320 ... 479:
                retval = 333;
                break;
            case 480 ... 639:
                retval = 500;
                break;
            case 640 ... 799:
                retval = 667;
                break;
            case 800 ... 959:
                retval = 833;
                break;
            case 960 ... 1119:
                retval = 980;
                break;
            case 1120 ... 1279:
                retval = 1167;
                break;
            case 1280 ... 1439:
                retval = 1333;
                break;
            case 1440 ... 1599:
                retval = 1500;
                break;
            case 1600 ... 1759:
                retval = 1667;
                break;
            case 1760 ... 1919:
                retval = 1833;
                break;
            case 1920 ... 2079:
                retval = 2000;
                break;
            case 2080 ... 2239:
                retval = 2167;
                break;
            case 2240 ... 2399:
                retval = 2333;
                break;
            case 2400 ... 2559:
                retval = 2500;
                break;
            case 2560 ... 2719:
                retval = 2667;
                break;
            case 2720 ... 2879:
                retval = 2833;
                break;
            case 2880 ... 3039:
                retval = 3000;
                break;
            case 3040 ... 3199:
                retval = 3167;
                break;
            case 3200 ... 3359:
                retval = 3333;
                break;
            case 3360 ... 3519:
                retval = 3500;
                break;
            case 3520 ... 3679:
                retval = 3677;
                break;
            case 3680 ... 3839:
                retval = 3833;
                break;
            case 3840 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 2:
          // Major (Equal Temperament)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 333;
                break;
            case 534 ... 800:
                retval = 667;
                break;
            case 801 ... 1067:
                retval = 833;
                break;
            case 1068 ... 1334:
                retval = 1167;
                break;
            case 1335 ... 1601:
                retval = 1500;
                break;
            case 1602 ... 1868:
                retval = 1833;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2333;
                break;
            case 2403 ... 2669:
                retval = 2667;
                break;
            case 2670 ... 2936:
                retval = 2833;
                break;
            case 2937 ... 3203:
                retval = 3167;
                break;
            case 3204 ... 3470:
                retval = 3500;
                break;
            case 3471 ... 3737:
                retval = 3833;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
    
          break;
        case 3:
          // Major Pentatonic (Equal Temperament)
          switch(in_val){
            case 0 ... 363:
                retval = 0;
                break;
            case 364 ... 727:
                retval = 333;
                break;
            case 728 ... 1091:
                retval = 667;
                break;
            case 1092 ... 1455:
                retval = 1167;
                break;
            case 1456 ... 1819:
                retval = 1500;
                break;
            case 1820 ... 2183:
                retval = 2000;
                break;
            case 2184 ... 2547:
                retval = 2333;
                break;
            case 2548 ... 2911:
                retval = 2667;
                break;
            case 2912 ... 3275:
                retval = 3167;
                break;
            case 3276 ... 3639:
                retval = 3500;
                break;
            case 3640 ... 4000:
                retval = 4000;
                break;
          }

          break;
        case 4:
          // Minor Pentatonic (Equal Temperament)
          switch(in_val){
            case 0 ... 363:
                retval = 0;
                break;
            case 364 ... 727:
                retval = 500;
                break;
            case 728 ... 1091:
                retval = 833;
                break;
            case 1092 ... 1455:
                retval = 1167;
                break;
            case 1456 ... 1819:
                retval = 1667;
                break;
            case 1820 ... 2183:
                retval = 2000;
                break;
            case 2184 ... 2547:
                retval = 2500;
                break;
            case 2548 ... 2911:
                retval = 2833;
                break;
            case 2912 ... 3275:
                retval = 3167;
                break;
            case 3276 ... 3639:
                retval = 3667;
                break;
            case 3640 ... 4000:
                retval = 4000;
                break;
          }
        
          break;
        case 5:
          // Harmonic Minor (Equal Temperament)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 333;
                break;
            case 534 ... 800:
                retval = 500;
                break;
            case 801 ... 1067:
                retval = 833;
                break;
            case 1068 ... 1334:
                retval = 1167;
                break;
            case 1335 ... 1601:
                retval = 1333;
                break;
            case 1602 ... 1868:
                retval = 1833;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2333;
                break;
            case 2403 ... 2669:
                retval = 2500;
                break;
            case 2670 ... 2936:
                retval = 2833;
                break;
            case 2937 ... 3203:
                retval = 3167;
                break;
            case 3204 ... 3470:
                retval = 3333;
                break;
            case 3471 ... 3737:
                retval = 3833;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 6:
          // Dorian (Equal Temperament)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 333;
                break;
            case 534 ... 800:
                retval = 500;
                break;
            case 801 ... 1067:
                retval = 833;
                break;
            case 1068 ... 1334:
                retval = 1167;
                break;
            case 1335 ... 1601:
                retval = 1500;
                break;
            case 1602 ... 1868:
                retval = 1667;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2333;
                break;
            case 2403 ... 2669:
                retval = 2500;
                break;
            case 2670 ... 2936:
                retval = 2833;
                break;
            case 2937 ... 3203:
                retval = 3167;
                break;
            case 3204 ... 3470:
                retval = 3400;
                break;
            case 3471 ... 3737:
                retval = 3667;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }
    
          break;
        case 7:
          // Phrygian (Equal Temperament)
          switch(in_val){
            case 0 ... 266:
                retval = 0;
                break;
            case 267 ... 533:
                retval = 168;
                break;
            case 534 ... 800:
                retval = 500;
                break;
            case 801 ... 1067:
                retval = 833;
                break;
            case 1068 ... 1334:
                retval = 1167;
                break;
            case 1335 ... 1601:
                retval = 1333;
                break;
            case 1602 ... 1868:
                retval = 1667;
                break;
            case 1869 ... 2135:
                retval = 2000;
                break;
            case 2136 ... 2402:
                retval = 2168;
                break;
            case 2403 ... 2669:
                retval = 2500;
                break;
            case 2670 ... 2936:
                retval = 2833;
                break;
            case 2937 ... 3203:
                retval = 3167;
                break;
            case 3204 ... 3470:
                retval = 3333;
                break;
            case 3471 ... 3737:
                retval = 3667;
                break;
            case 3738 ... 4000:
                retval = 4000;
                break;
          }

          break;
        case 8:
          // Symmetric Octatonic Half Whole (Equal Temperament)
          switch(in_val){
            case 0 ... 234:
                retval = 0;
                break;
            case 235 ... 469:
                retval = 168;
                break;
            case 470 ... 704:
                retval = 500;
                break;
            case 705 ... 939:
                retval = 667;
                break;
            case 940 ... 1174:
                retval = 1000;
                break;
            case 1175 ... 1409:
                retval = 1167;
                break;
            case 1410 ... 1644:
                retval = 1500;
                break;
            case 1645 ... 1879:
                retval = 1667;
                break;
            case 1880 ... 2114:
                retval = 2000;
                break;
            case 2115 ... 2349:
                retval = 2168;
                break;
            case 2350 ... 2584:
                retval = 2500;
                break;
            case 2585 ... 2819:
                retval = 2667;
                break;
            case 2820 ... 3054:
                retval = 3000;
                break;
            case 3055 ... 3289:
                retval = 3167;
                break;
            case 3290 ... 3524:
                retval = 3400;
                break;
            case 3525 ... 3759:
                retval = 3667;
                break;
            case 3760 ... 4000:
                retval = 4000;
                break;
          }
          break;
        case 9:
          // Symmetric Octatonic Whole half (Equal Temperament)
          switch(in_val){
            case 0 ... 234:
                retval = 0;
                break;
            case 235 ... 469:
                retval = 333;
                break;
            case 470 ... 704:
                retval = 500;
                break;
            case 705 ... 939:
                retval = 833;
                break;
            case 940 ... 1174:
                retval = 1000;
                break;
            case 1175 ... 1409:
                retval = 1333;
                break;
            case 1410 ... 1644:
                retval = 1500;
                break;
            case 1645 ... 1879:
                retval = 1833;
                break;
            case 1880 ... 2114:
                retval = 2000;
                break;
            case 2115 ... 2349:
                retval = 2333;
                break;
            case 2350 ... 2584:
                retval = 2500;
                break;
            case 2585 ... 2819:
                retval = 2833;
                break;
            case 2820 ... 3054:
                retval = 3000;
                break;
            case 3055 ... 3289:
                retval = 3333;
                break;
            case 3290 ... 3524:
                retval = 3500;
                break;
            case 3525 ... 3759:
                retval = 3833;
                break;
            case 3760 ... 4000:
                retval = 4000;
                break;
          }
          break;
        default:
            retval = in_val;  // default = don't quantize
          break;
      }
    
      break;
  }
  log_msg("Inval: %d, retval = %d\n\r",in_val, retval);
  return retval;
}

