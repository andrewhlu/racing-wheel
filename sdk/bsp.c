#include <math.h>
#include "bsp.h"
#include "lcd.h"
#include "PmodACL2.h"
#include "qpn_port.h"
#include "racing_wheel.h"
#include "xgpio.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xintc.h"
#include "xparameters.h"
#include "xspi.h"
#include "xspi_l.h"
#include "xtmrctr.h"
#include "xtmrctr_l.h"
#include "xuartlite.h"
#include "xuartlite_l.h"

#define LED_CHANNEL 1

// Interrupt controller and drivers
PmodACL2 accelerometer;
XIntc sys_intc;
XTmrCtr sys_tmrctr;
XGpio led_gpio;
XGpio rgbled_gpio;
XGpio button_gpio;
XGpio switch_gpio;
XSpi lcdScreen;
XSpi_Config* lcdScreenConfig;
XUartLite usbUart;

#ifdef XPAR_BLUETOOTH_UART_DEVICE_ID
XUartLite bluetoothUart;
#endif

// Forward declarations
void timer_reset();
void timer_interrupt_handler();
void button_interrupt_handler();
void switch_interrupt_handler();

// Previous button state
unsigned int previousButton = 0;
unsigned int previousSwitch = 0;

/*..........................................................................*/
void BSP_init(void) {
    xil_printf("Starting BSP initialization\r\n");
    int status;

    // Set up interrupt controller
    XIntc_Initialize(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
                  (XInterruptHandler)timer_interrupt_handler, &sys_tmrctr);
    XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
                  (XInterruptHandler)button_interrupt_handler, &button_gpio);
    XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_SWITCHES_IP2INTC_IRPT_INTR,
                  (XInterruptHandler)switch_interrupt_handler, &switch_gpio);
    XIntc_Start(&sys_intc, XIN_REAL_MODE);
    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);
    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_SWITCHES_IP2INTC_IRPT_INTR);

    // Set up timer interrupt
    XTmrCtr_Initialize(&sys_tmrctr, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    XTmrCtr_SetOptions(&sys_tmrctr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    XTmrCtr_SetResetValue(&sys_tmrctr, 0, 0xFFFFFFFF - 10000000);

    // Set up button interrupt
    XGpio_Initialize(&button_gpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
    // Why is it not XPAR_AXI_GPIO_BTN_IP2INTC_IRPT_MASK???
    XGpio_InterruptEnable(&button_gpio, XPAR_MDM_1_INTERRUPT_MASK);
    XGpio_InterruptGlobalEnable(&button_gpio);
    XGpio_InterruptClear(&button_gpio, XPAR_MDM_1_INTERRUPT_MASK);

    // Set up switch interrupt
    XGpio_Initialize(&switch_gpio, XPAR_SWITCHES_DEVICE_ID);
    // Why is it not XPAR_AXI_GPIO_BTN_IP2INTC_IRPT_MASK???
    XGpio_InterruptEnable(&switch_gpio, XPAR_MDM_1_INTERRUPT_MASK);
    XGpio_InterruptGlobalEnable(&switch_gpio);
    XGpio_InterruptClear(&switch_gpio, XPAR_MDM_1_INTERRUPT_MASK);
    previousSwitch = XGpio_DiscreteRead(&switch_gpio, 1);

    // Initialize GPIO for LED
    XGpio_Initialize(&led_gpio, XPAR_AXI_GPIO_LED_DEVICE_ID);
    XGpio_Initialize(&rgbled_gpio, XPAR_RGBLEDS_DEVICE_ID);

    // Set up USB UART communication
    XUartLite_Initialize(&usbUart, XPAR_USB_UART_DEVICE_ID);
    status = XUartLite_SelfTest(&usbUart);
    xil_printf("USB UART Self test result: %d\r\n", status);

#ifdef XPAR_BLUETOOTH_UART_DEVICE_ID
    // Set up Bluetooth UART communication
    XUartLite_Initialize(&bluetoothUart, XPAR_BLUETOOTH_UART_DEVICE_ID);
    status = XUartLite_SelfTest(&bluetoothUart);
    xil_printf("Bluetooth UART Self test result: %d\r\n", status);
#endif

    // Enable interrupts in Microblaze
    microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
                                (void *)XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
    microblaze_enable_interrupts();
    timer_reset();

    // Initialize SPI Driver for LCD Screen
    lcdScreenConfig = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
    if (lcdScreenConfig == NULL) {
        xil_printf("Can't find spi device!\n");
        return;
    }

    status = XSpi_CfgInitialize(&lcdScreen, lcdScreenConfig, lcdScreenConfig->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("Initialize spi fail!\n");
        return;
    }

    // Reset the SPI device to leave it in a known good state.
    XSpi_Reset(&lcdScreen);

    // Setup the control register to enable master mode
    u32 controlReg = XSpi_GetControlReg(&lcdScreen);
    XSpi_SetControlReg(&lcdScreen,
            (controlReg | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) &
            (~XSP_CR_TRANS_INHIBIT_MASK));

    // Select 1st slave device
    XSpi_SetSlaveSelectReg(&lcdScreen, ~0x01);

    // Clear the LCD Screen
    initLCD();
    clrScr();

    // Initialize the accelerometer driver device
    ACL2_begin(&accelerometer, XPAR_ACL_GPIO_BASEADDR, XPAR_ACCEL_SPI_BASEADDR);
    ACL2_reset(&accelerometer);
    usleep(1000);
    ACL2_configure(&accelerometer);

    // Start timer
    XTmrCtr_Start(&sys_tmrctr, 0);

    xil_printf("\r\nFinishing BSP initialization\r\n");
}

/*..........................................................................*/
void QF_onStartup(void) {                 /* entered with interrupts locked */
    // Empty function
}


void QF_onIdle(void) {        /* entered with interrupts locked */
    QF_INT_UNLOCK();                       /* unlock interrupts */
}

/* Do not touch Q_onAssert */
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* avoid compiler warning */
    (void)line;                                   /* avoid compiler warning */
    QF_INT_LOCK();
    for (;;) {
    }
}

void timer_reset() {
    // Turn off the timer
    XTmrCtr_SetControlStatusReg(XPAR_TMRCTR_0_BASEADDR, 1, 0);
    // Put a zero in the load register
    XTmrCtr_SetLoadReg(XPAR_TMRCTR_0_BASEADDR, 1, 0);
    // Copy the load register into the counter register
    XTmrCtr_SetControlStatusReg(XPAR_TMRCTR_0_BASEADDR, 1, XTC_CSR_LOAD_MASK);
    // Enable (start) the timer
    XTmrCtr_SetControlStatusReg(XPAR_TMRCTR_0_BASEADDR, 1,	XTC_CSR_ENABLE_TMR_MASK);
}

void timer_interrupt_handler() {
    // Send tick signal
    QActive_postISR((QActive *)&AO_RacingWheel, TICK_SIG);

    // Timer interrupt handler
    uint32_t ControlStatusReg;
    ControlStatusReg = XTimerCtr_ReadReg(sys_tmrctr.BaseAddress, 0, XTC_TCSR_OFFSET);
    XTmrCtr_WriteReg(sys_tmrctr.BaseAddress, 0, XTC_TCSR_OFFSET, ControlStatusReg | XTC_CSR_INT_OCCURED_MASK);

    timer_reset();
}

void button_interrupt_handler() {
    // Get integer representing buttons pushed
    // 5 bit value - bits represent "CDRLU" when read out
    unsigned int button = XGpio_DiscreteRead(&button_gpio, 1);
    
    // Up (1)
    if (button & 1) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_UP);
    } else if (previousButton & 1) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RELEASE_UP);
    }
    
    // Left (2)
    if (button & 2) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_LEFT);
    } else if (previousButton & 2) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RELEASE_LEFT);
    }
    
    // Right (4)
    if (button & 4) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RIGHT);
    } else if (previousButton & 4) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RELEASE_RIGHT);
    }
    
    // Down (8)
    if (button & 8) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_DOWN);
    } else if (previousButton & 8) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RELEASE_DOWN);
    }
    
    // Center (16)
    if (button & 16) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_CENTER);
    } else if (previousButton & 16) {
        QActive_postISR((QActive *)&AO_RacingWheel, DPAD_RELEASE_CENTER);
    }

    previousButton = button;

    XGpio_InterruptClear(&button_gpio, XPAR_MDM_1_INTERRUPT_MASK);
}

void switch_interrupt_handler() {
    unsigned int switches = XGpio_DiscreteRead(&switch_gpio, 1);

    if ((previousSwitch ^ switches) & 0x8000) {
        if (switches & 0x8000) {
            QActive_postISR((QActive *)&AO_RacingWheel, SWITCH_15_ON);
        } else {
            QActive_postISR((QActive *)&AO_RacingWheel, SWITCH_15_OFF);
        }
    } else if ((previousSwitch ^ switches) & 0x0001) {
        if (switches & 0x0001) {
            QActive_postISR((QActive *)&AO_RacingWheel, SWITCH_0_ON);
        } else {
            QActive_postISR((QActive *)&AO_RacingWheel, SWITCH_0_OFF);
        }
    }

    previousSwitch = switches;

    XGpio_InterruptClear(&switch_gpio, XPAR_MDM_1_INTERRUPT_MASK);
}
