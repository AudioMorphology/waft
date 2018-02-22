# waft
Eurorack Gesture Controller

## Overview

Waft is a LIDAR-based gesture controller in Eurorack format.

It is based around the VL6180x Time Of Flight controller, which measures distances using a low-power Infra-red LASER emitter but, unlike most IR distance measuring devices, which estimate distance based on reflected signals, the VL6180x measures the actual round-trip (Time Of Flight) delay. So, maybe 'gesture' perhaps overstates it slightly - it doesn't recognise gestures such as swipe up, swipe down etc., though it _**does**_ measure distance with mm accuracy over a distance of 20cm or thereabouts.... 

The VL6180X is the latest product based on ST Microelectronics'patented FlightSense™ technology. It uses a SPAD (Single Photon Avalanche Diode) array coupled with a VCSEL (Vertical Cavity Surface-Emitting Laser). The VL6180x also incorporates an Ambient Light Sensor, though we're not making use of that here.

Waft utilises the widely-available Arduino Nano V3 board as its processing core, and an mcp4728 12-Bit I2C quad-channel DAC for the CV Outputs.

Waft generates two CV and two Gate outputs:

* CV1 - 0-8v Range, not quantised
* CV2 - 0-2v Range, quantised to a series of selectable scales
* Gate 1 - On if any object detected
* gate 2 - On triggered by movement only

In addition, a CV Input can be mixed with a variable amount of CV1, thus the Waft controller can be used to modulate a CV input.

The hardware design (Schematics & Gerbers) are released under the Opensource Hardware, and the Software is available under a Creative Commons / MIT license.

All documentation is held in the Wiki
