# Racing Wheel Game Controller

Suhail Chakravarty, Andrew Lu.

ECE 253, Fall 2020, Professor Brewer. 

---

## Overview

The goal of this project is to utilize the accelerometer on the Nexys A7 to emulate a steering wheel that can act as a controller and be used to play many PC racing games. When playing the game, users can turn the board like they would with a steering wheel, and the Nexys A7 board will emulate the necessary controller signals to be picked up by the game.

## Peripherals

For the base idea, no additional peripherals are needed.

However, as a stretch goal, we may consider adding additional features that would require external peripherals, such as an external display for the GUI.

## Software / Real-Time Architecture

The system will consistently read values from the onboard 3-axis accelerometer. When significant changes to the acceleration are detected (denoting that the board has been turned), the system will emulate the necessary HID signals to indicate the change, and send them out of the USB Host port using the onboard HID controller.

## Testing

In the early stages of this project, we plan to test our functionality by outputting accelerometer data using the eight 7-segment displays and the various LEDs onboard. When we gain more familiarity working with this board, we can expand further by incorporating the use of an external display (either the TFT LED display or a VGA-connected display). Final testing will be done by connecting this to a computer, opening a compatible racing game, and attempting to play it using the board.

## Further Ideas / Improvements

Once the base project is done (the controller works for one specific racing game), we would want to set up different modes as well, so we can integrate our new controller into multiple games. We may also want to attach a screen, which can be used to display a graphical user interface.

Additional ideas include adding an external foot pedal using the various connectors on the board.