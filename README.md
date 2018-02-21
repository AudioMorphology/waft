# waft
Eurorack Gesture Controller

## Overview

Waft is a LIDAR-based gesture controller in Eurorack format.

It is based around the VL6180x Time Of Flight controller, which measures distances by firing out a low-power laser, and recording the round-trip (Time Of Flight) delay. 

Waft utilises the widely-available Arduino Nano V3 board as its processing platform, and mcp4728 12-Bit DACs for the CV Outputs.

Waft generates two CV and two Gate outputs:

* CV1 - a 4v Range, not quantised
* CV2 - a 2v Range, quantised to a series of selectable scales
* Gate 1 - On if any object detected
* gate 2 - On triggered by movement only

In addition, a CV Input can be mixed with a variable amount of CV1, thus the Waft controller can be used to modulate a CV input.

The hardware design (Schematics & Gerbers) are released under the Opensource Hardware, and the Software is available under a Creative Commons / MIT license.

All documentation is held in the [Wiki](wiki)
