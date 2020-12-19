#define AO_RACINGWHEEL

#include <stdio.h>
#include "bsp.h"
#include "lcd.h"
#include "PmodACL2.h"
#include "qpn_port.h"
#include "racing_wheel.h"
#include "xgpio.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xspi.h"
#include "xspi_l.h"
#include "xuartlite.h"
#include "xuartlite_l.h"

#define LED_CHANNEL 1

// Drivers from bsp.c
extern PmodACL2 accelerometer;
extern XGpio led_gpio;
extern XGpio rgbled_gpio;
extern XGpio switch_gpio;
extern XUartLite usbUart;

#ifdef XPAR_BLUETOOTH_UART_DEVICE_ID
extern XUartLite bluetoothUart;
#endif

// Forward declarations
void sendCommand(u8 command);
void drawLCDBackground();

typedef struct RacingWheelTag {
    QActive super;

    // Active UART - 1 for USB, 2 for Bluetooth
    unsigned char activeUart;
    XUartLite* activeUartPtr;
    u8 prevAccelValue;
    float sensitivity;
    int speedometer;
    unsigned char racingModeActive;
} RacingWheel;

/* Setup state machines */
/**********************************************************************/
static QState RacingWheel_initial(RacingWheel *me);
static QState RacingWheel_on(RacingWheel *me);
static QState RacingWheel_options(RacingWheel *me);
static QState RacingWheel_options_sensitivity(RacingWheel *me);
static QState RacingWheel_options_uart(RacingWheel *me);
static QState RacingWheel_options_game(RacingWheel *me);
static QState RacingWheel_options_speedometer(RacingWheel *me);
static QState RacingWheel_options_default(RacingWheel *me);
static QState RacingWheel_asphalt(RacingWheel *me);
static QState RacingWheel_asphalt_racing(RacingWheel *me);
static QState RacingWheel_asphalt_dpad(RacingWheel *me);
/**********************************************************************/

RacingWheel AO_RacingWheel;

void RacingWheel_ctor() {
    RacingWheel *me = &AO_RacingWheel;
    QActive_ctor(&me->super, (QStateHandler)&RacingWheel_initial);
    AO_RacingWheel.activeUart = 1;
    AO_RacingWheel.activeUartPtr = &usbUart;
    AO_RacingWheel.prevAccelValue = 7;
    AO_RacingWheel.sensitivity = 8;
    AO_RacingWheel.speedometer = 1;
    AO_RacingWheel.racingModeActive = 0;
}

QState RacingWheel_initial(RacingWheel *me) {
    //xil_printf("\r\nRacing Wheel Initialization\r\n");

    return Q_TRAN(&RacingWheel_on);
}

QState RacingWheel_on(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            //xil_printf("\n\rOn");
        }

        case Q_INIT_SIG: {
            unsigned int initialSwitch = XGpio_DiscreteRead(&switch_gpio, 1);

            if (initialSwitch & 0x8000) {
                return Q_TRAN(&RacingWheel_options);
            } else if (initialSwitch & 0x0001) {
                return Q_TRAN(&RacingWheel_asphalt_racing);
            } else {
                return Q_TRAN(&RacingWheel_asphalt_dpad); 
            }
        }

        case DPAD_UP: {
            return Q_HANDLED();
        }

        case DPAD_RELEASE_UP: {
            return Q_HANDLED();
        }

        case DPAD_LEFT: {
            return Q_HANDLED();
        }

        case DPAD_RELEASE_LEFT: {
            return Q_HANDLED();
        }

        case DPAD_RIGHT: {
            return Q_HANDLED();
        }

        case DPAD_RELEASE_RIGHT: {
            return Q_HANDLED();
        }

        case DPAD_DOWN: {
            return Q_HANDLED();
        }

        case DPAD_RELEASE_DOWN: {
            return Q_HANDLED();
        }

        case DPAD_CENTER: {
            return Q_HANDLED();
        }

        case DPAD_RELEASE_CENTER: {
            return Q_HANDLED();
        }

        case SWITCH_0_OFF: {
            AO_RacingWheel.racingModeActive = 0;
            return Q_HANDLED();
        }

        case SWITCH_0_ON: {
            AO_RacingWheel.racingModeActive = 1;
            return Q_HANDLED();
        }

        case SWITCH_15_OFF: {
            if (AO_RacingWheel.racingModeActive) {
                return Q_TRAN(&RacingWheel_asphalt_racing);
            } else {
                return Q_TRAN(&RacingWheel_asphalt_dpad);
            }
        }

        case SWITCH_15_ON: {
            return Q_TRAN(&RacingWheel_options_sensitivity);
        }
        
        case TICK_SIG: {
            return Q_HANDLED();
        }
    }
    
    return Q_SUPER(&QHsm_top);
}

QState RacingWheel_options(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            //xil_printf("Entering Options State\r\n");
            drawLCDBackground();

            // Display Options Header
            setColor(255, 255, 255);
            setColorBg(0, 0, 0);
            setFont(BigFont);
            lcdPrint("OPTIONS", 64, 20);

            // Display Option Titles
            setFont(SmallFont);
            lcdPrint("Sensitivity", 40, 60);
            lcdPrint("UART Mode", 40, 100);
            lcdPrint("Game Select", 40, 140);
            lcdPrint("Speedometer", 40, 180);
            lcdPrint("Reset to Default", 40, 240);

            // Display Current Option Values
            char sensitivityBuffer[2];
            sprintf(sensitivityBuffer, "%2.0f", AO_RacingWheel.sensitivity);
            lcdPrint(sensitivityBuffer, 180, 60);

            char uartBuffer[9];
            sprintf(uartBuffer, "%s", AO_RacingWheel.activeUart == 2 ? "Bluetooth" : "   USB   ");
            lcdPrint(uartBuffer, 152, 100);

            lcdPrint("ASPHALT", 160, 140);

            if (AO_RacingWheel.speedometer == 1) {
                lcdPrint(" ON", 176, 180);
            } else {
                lcdPrint("OFF", 176, 180);
            }

            // Set RGB LED Indicator (Left RGB, Right RGB)
            // Left indicates active UART, right indicates racing mode
            // Note that this will be very bright
            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 4;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);

            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            //xil_printf("Exiting Options State\r\n");

            // Clear row of LEDs
            XGpio_DiscreteWrite(&led_gpio, LED_CHANNEL, 0);

            return Q_HANDLED();
        }

        case TICK_SIG: {
            u8 value;

            // Capture accelerometer data only when there is data available
            if ((ACL2_STATUS_DATA_READY_MASK & ACL2_getStatus(&accelerometer)) != 0) {
                ACL2_SamplePacket data = ACL2_getSample(&accelerometer);

                int accelVal = (int)(ACL2_DataToG(&accelerometer, data.YData) * AO_RacingWheel.sensitivity) + 8;

                if (accelVal < 0) {
                    accelVal = 0;
                } else if (accelVal > 15) {
                    accelVal = 15;
                }

                value = accelVal;

                XGpio_DiscreteWrite(&led_gpio, LED_CHANNEL, 1 << value);
            }

            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_on);
}

QState RacingWheel_options_sensitivity(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            setColor(57, 255, 20);
            fillRect(14, 60, 26, 72);
            lcdPrint("< ", 164, 60);
            lcdPrint(" >", 196, 60);

            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            setColor(0, 54, 96);
            fillRect(14, 60, 26, 72);
            fillRect(164, 60, 179, 71);
            fillRect(196, 60, 211, 71);
            return Q_HANDLED();
        }

        case DPAD_UP: {
            return Q_TRAN(&RacingWheel_options_default);
        }

        case DPAD_DOWN: {
            return Q_TRAN(&RacingWheel_options_uart);
        }

        case DPAD_LEFT: {
            if (AO_RacingWheel.sensitivity > 1) {
                AO_RacingWheel.sensitivity--;
            }

            if (AO_RacingWheel.sensitivity == 1) {
                setColor(128, 128, 128);
                lcdPrint("< ", 164, 60);
            } else if (AO_RacingWheel.sensitivity == 14) {
                setColor(57, 255, 20);
                lcdPrint(" >", 196, 60);
            }

            char buffer[2];
            sprintf(buffer, "%2.0f", AO_RacingWheel.sensitivity);
            setColor(255, 255, 255);
            lcdPrint(buffer, 180, 60);

            return Q_HANDLED();
        }

        case DPAD_RIGHT: {
            if (AO_RacingWheel.sensitivity < 15) {
                AO_RacingWheel.sensitivity++;
            }

            if (AO_RacingWheel.sensitivity == 2) {
                setColor(57, 255, 20);
                lcdPrint("< ", 164, 60);
            } else if (AO_RacingWheel.sensitivity == 15) {
                setColor(128, 128, 128);
                lcdPrint(" >", 196, 60);
            }

            char buffer[2];
            sprintf(buffer, "%2.0f", AO_RacingWheel.sensitivity);
            setColor(255, 255, 255);
            lcdPrint(buffer, 180, 60);

            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_options);
}

QState RacingWheel_options_uart(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            setColor(57, 255, 20);
            fillRect(14, 100, 26, 112);

            #ifndef XPAR_BLUETOOTH_UART_DEVICE_ID
            setColor(128, 128, 128);
            #endif

            lcdPrint("< ", 136, 100);
            lcdPrint(" >", 224, 100);
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            setColor(0, 54, 96);
            fillRect(14, 100, 26, 112);
            fillRect(136, 100, 151, 111);
            fillRect(224, 100, 239, 111);
            return Q_HANDLED();
        }

        case DPAD_UP: {
            return Q_TRAN(&RacingWheel_options_sensitivity);
        }

        case DPAD_DOWN: {
            return Q_TRAN(&RacingWheel_options_game);
        }

        case DPAD_LEFT: {
            #ifdef XPAR_BLUETOOTH_UART_DEVICE_ID
            AO_RacingWheel.activeUart = AO_RacingWheel.activeUart == 2 ? 1 : 2;
            AO_RacingWheel.activeUartPtr = AO_RacingWheel.activeUart == 1 ? &usbUart : &bluetoothUart;

            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 4;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);

            char uartBuffer[9];
            sprintf(uartBuffer, "%s", AO_RacingWheel.activeUart == 2 ? "Bluetooth" : "   USB   ");
            setColor(255, 255, 255);
            lcdPrint(uartBuffer, 152, 100);
            #endif
            return Q_HANDLED();
        }

        case DPAD_RIGHT: {
            #ifdef XPAR_BLUETOOTH_UART_DEVICE_ID
            AO_RacingWheel.activeUart = AO_RacingWheel.activeUart == 2 ? 1 : 2;
            AO_RacingWheel.activeUartPtr = AO_RacingWheel.activeUart == 1 ? &usbUart : &bluetoothUart;

            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 4;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);

            char uartBuffer[9];
            sprintf(uartBuffer, "%s", AO_RacingWheel.activeUart == 2 ? "Bluetooth" : "   USB   ");
            setColor(255, 255, 255);
            lcdPrint(uartBuffer, 152, 100);
            #endif
            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_options);
}

QState RacingWheel_options_game(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            setColor(57, 255, 20);
            fillRect(14, 140, 26, 152);
            setColor(128, 128, 128);
            lcdPrint("< ", 144, 140);
            lcdPrint(" >", 216, 140);
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            setColor(0, 54, 96);
            fillRect(14, 140, 26, 152);
            fillRect(144, 140, 159, 151);
            fillRect(216, 140, 231, 151);
            return Q_HANDLED();
        }

        case DPAD_UP: {
            return Q_TRAN(&RacingWheel_options_uart);
        }

        case DPAD_DOWN: {
            return Q_TRAN(&RacingWheel_options_speedometer);
        }
    }
    return Q_SUPER(&RacingWheel_options);
}

QState RacingWheel_options_speedometer(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            setColor(57, 255, 20);
            fillRect(14, 180, 26, 192);
            lcdPrint("< ", 160, 180);
            lcdPrint(" >", 200, 180);
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            setColor(0, 54, 96);
            fillRect(14, 180, 26, 192);
            fillRect(160, 180, 175, 191);
            fillRect(200, 180, 215, 191);
            return Q_HANDLED();
        }

        case DPAD_UP: {
            return Q_TRAN(&RacingWheel_options_game);
        }

        case DPAD_DOWN: {
            return Q_TRAN(&RacingWheel_options_default);
        }

        case DPAD_LEFT: {
            setColor(255, 255, 255);
            if (AO_RacingWheel.speedometer == 1) {
                AO_RacingWheel.speedometer = 0;
                lcdPrint("OFF", 176, 180);
            } else {
                AO_RacingWheel.speedometer = 1;
                lcdPrint(" ON", 176, 180);
            }
            return Q_HANDLED();
        }

        case DPAD_RIGHT: {
            setColor(255, 255, 255);
            if (AO_RacingWheel.speedometer == 1) {
                AO_RacingWheel.speedometer = 0;
                lcdPrint("OFF", 176, 180);
            } else {
                AO_RacingWheel.speedometer = 1;
                lcdPrint(" ON", 176, 180);
            }
            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_options);
}

QState RacingWheel_options_default(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            setColor(57, 255, 20);
            fillRect(14, 240, 26, 252);
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            setColor(0, 54, 96);
            fillRect(14, 240, 26, 252);
            return Q_HANDLED();
        }

        case DPAD_UP: {
            return Q_TRAN(&RacingWheel_options_speedometer);
        }

        case DPAD_DOWN: {
            return Q_TRAN(&RacingWheel_options_sensitivity);
        }

        case DPAD_CENTER: {
            //Reset Constructor to default values
            AO_RacingWheel.activeUart = 1;
            AO_RacingWheel.activeUartPtr = &usbUart;
            AO_RacingWheel.prevAccelValue = 7;
            AO_RacingWheel.sensitivity = 8;
            AO_RacingWheel.speedometer = 1;

            //Redraw all parameters
            setColor(255, 255, 255);
            char sensitivityBuffer[2];
            sprintf(sensitivityBuffer, "%2.0f", AO_RacingWheel.sensitivity);
            lcdPrint(sensitivityBuffer, 180, 60);

            char uartBuffer[9];
            sprintf(uartBuffer, "%s", AO_RacingWheel.activeUart == 2 ? "Bluetooth" : "   USB   ");
            lcdPrint(uartBuffer, 152, 100);

            lcdPrint("ASPHALT", 160, 140);
            lcdPrint(" ON", 176, 180);

            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 4;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);

            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_options);
}

QState RacingWheel_asphalt(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            //xil_printf("Entering Asphalt State\r\n");
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            //xil_printf("Exiting Asphalt State\r\n");
            return Q_HANDLED();
        }

        case SWITCH_0_OFF: {
            AO_RacingWheel.racingModeActive = 0;
            return Q_TRAN(&RacingWheel_asphalt_dpad);
        }

        case SWITCH_0_ON: {
            AO_RacingWheel.racingModeActive = 1;
            return Q_TRAN(&RacingWheel_asphalt_racing);
        }
    }
    return Q_SUPER(&RacingWheel_on);
}

QState RacingWheel_asphalt_racing(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            //xil_printf("\r\nEntering Asphalt Racing Wheel State\n");
            drawLCDBackground();

            // Set RGB LED Indicator (Left RGB, Right RGB)
            // Left indicates active UART, right indicates racing mode
            // Note that this will be very bright
            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 2;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);
            setFont(BigFont);
            setColor(255, 255, 255);
            lcdPrint("RACING", 72, 20);
            
            if (AO_RacingWheel.speedometer) {
                // Enable speedometer in Python
                sendCommand(29);

                // Display 0 on speedometer
                char secStr[4] = { '0', '0', '0', '\0' };
                setFont(SevenSegNumFont);
                setColor(238, 64, 0);
                setColorBg(0, 54, 96);
                lcdPrint(secStr, 70, 190);
                setFont(BigFont);
                setColor(255, 255, 255);
                lcdPrint("MPH", 70, 190);
            }

            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            //xil_printf("Exiting Asphalt Racing Wheel State\r\n");

            // Disable speedometer in Python
            sendCommand(30);

            // Reset accelerometer value
            AO_RacingWheel.prevAccelValue = 7;
            sendCommand(7);

            // Clear row of LEDs
            XGpio_DiscreteWrite(&led_gpio, LED_CHANNEL, 0);

            // Clear all pressed keys
            sendCommand(28);

            return Q_HANDLED();
        }

        case DPAD_DOWN: {
            // Send command to activate Drift mode
            sendCommand(22);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_DOWN: {
            // Send command to deactivate Drift mode
            sendCommand(23);
            return Q_HANDLED();
        }

        case DPAD_CENTER: {
            // Send command to activate Nitro mode
            sendCommand(24);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_CENTER: {
            // Send command to deactivate Nitro mode
            sendCommand(25);
            return Q_HANDLED();
        }

        case TICK_SIG: {
            u8 value;

            // Capture accelerometer data only when there is data available
            if ((ACL2_STATUS_DATA_READY_MASK & ACL2_getStatus(&accelerometer)) != 0) {
                ACL2_SamplePacket data = ACL2_getSample(&accelerometer);

                int accelVal = (int)(ACL2_DataToG(&accelerometer, data.YData) * AO_RacingWheel.sensitivity) + 8;

                if (accelVal < 0) {
                    accelVal = 0;
                } else if (accelVal > 15) {
                    accelVal = 15;
                }

                value = accelVal;

                XGpio_DiscreteWrite(&led_gpio, LED_CHANNEL, 1 << value);

                if (value != AO_RacingWheel.prevAccelValue) {
                    AO_RacingWheel.prevAccelValue = value;
                    sendCommand(value);
                }
            }

            // Receive incoming speed data from computer
            unsigned int bytesReceived = XUartLite_Recv(AO_RacingWheel.activeUartPtr, &value, 1);

            if (bytesReceived && value && AO_RacingWheel.speedometer) {
                setFont(SevenSegNumFont);
                setColor(238, 64, 0);
                setColorBg(0, 54, 96);

                char secStr[4];

                secStr[0] = (value / 100) % 10 + 48;
                secStr[1] = (value / 10) % 10 + 48;
                secStr[2] = value % 10 + 48;
                secStr[3] = '\0';

                lcdPrint(secStr, 70, 190);
            }

            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_asphalt);
}

QState RacingWheel_asphalt_dpad(RacingWheel *me) {
    switch (Q_SIG(me)) {
        case Q_ENTRY_SIG: {
            //xil_printf("\r\nEntering Asphalt D-Pad State\n");
            drawLCDBackground();

            // Set RGB LED Indicator (Left RGB, Right RGB)
            // Left indicates active UART, right indicates racing mode
            // Note that this will be very bright
            unsigned int rgbledMask = (1 << (AO_RacingWheel.activeUart == 2 ? 3 : 4)) + 4;
            XGpio_DiscreteWrite(&rgbled_gpio, LED_CHANNEL, rgbledMask);
            setFont(BigFont);
            setColor(255, 255, 255);
            lcdPrint("NAVIGATION", 40, 20);
            return Q_HANDLED();
        }

        case Q_EXIT_SIG: {
            //xil_printf("Exiting Asphalt D-Pad State\r\n");

            // Clear all pressed keys
            sendCommand(28);

            return Q_HANDLED();
        }

        case DPAD_UP: {
            sendCommand(16);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_UP: {
            sendCommand(17);
            return Q_HANDLED();
        }

        case DPAD_LEFT: {
            sendCommand(18);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_LEFT: {
            sendCommand(19);
            return Q_HANDLED();
        }

        case DPAD_RIGHT: {
            sendCommand(20);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_RIGHT: {
            sendCommand(21);
            return Q_HANDLED();
        }

        case DPAD_DOWN: {
            sendCommand(22);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_DOWN: {
            sendCommand(23);
            return Q_HANDLED();
        }

        case DPAD_CENTER: {
            // Send command to press "Enter"
            sendCommand(26);
            return Q_HANDLED();
        }

        case DPAD_RELEASE_CENTER: {
            // Send command to release "Enter"
            sendCommand(27);
            return Q_HANDLED();
        }
    }
    return Q_SUPER(&RacingWheel_asphalt);
}

void sendCommand(u8 command) {
    XUartLite_Send(AO_RacingWheel.activeUartPtr, &command, 1);
}

void drawLCDBackground() {
    // Fill background with UCSB Navy
    setColor(0, 54, 96);
    fillRect(0, 0, 240, 320);
}
