/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

/* Includes */
#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"
#include "xscugic.h"
#include "xil_exception.h"

/* Definitions */
#define GPIO_DEVICE_ID XPAR_AXI_GPIO_0_DEVICE_ID		/* GPIO Device that LEDs are connected to */
#define LED 0x01										/* Initial LED Value XX0000XX */
#define LED_DELAY 10000000								/* Software delay length */
#define LED_CHANNEL	1									/* GPIO port for LEDs */
#define BUTTONS_CHANNEL	2								/* GPIO port for Buttons */
#define INTC_DEVICE_ID XPAR_PS7_SCUGIC_0_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define BTN_INT XGPIO_IR_CH1_MASK
#define printf xil_printf								/* define to use printf as optimized for il_printf */

XGpio Gpio;												/* GPIO Device driver instance */
XScuGic INTCInst;

static int led_data;
static int btn_value;

/* Functions */
static void BTN_Intr_Handler(void *baseaddr_p);
static int Interrupt_System_Setup(XScuGic *XScuGicInstancePtr);
static int Intc_Init_Function(u16 DeviceId, XGpio *GpioInstancePtr);

/* Interrupt Handler Function */
void BTN_Intr_Handler(void *InstancePtr)
{
	XGpio_InterruptDisable(&Gpio, BTN_INT);				/* Disable GPIO Interrupts */
	/* Ignore additional button presses */
	if((XGpio_InterruptGetStatus(&Gpio)&BTN_INT)!=BTN_INT){
		return;
	}
	btn_value = XGpio_DiscreteRead(&Gpio, BUTTONS_CHANNEL);
	if(btn_value != 1)
		led_data = led_data+btn_value;				/* Increment counter based on button value, reset if center button is pressed */
	else
		led_data=0;
	XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, led_data);
	(void)XGpio_InterruptClear(&Gpio, BTN_INT);
	XGpio_InterruptEnable(&Gpio, BTN_INT);				/* Enable GPIO Interrupts */
}

/* Initial Setup function */
int Interrupt_System_Setup(XScuGic *XScuGicInstancePtr)
{
	XGpio_InterruptEnable(&Gpio, BTN_INT);				/* Enable GPIO Interrupts */
	XGpio_InterruptGlobalEnable(&Gpio);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler) XScuGic_InterruptHandler, XScuGicInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

int Intc_Init_Function(u16 DeviceId, XGpio *GpioInstancePtr)
{
	XScuGic_Config *IntcConfig;
	int status;

	/* Interrupt controller Initialization */
	IntcConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}

	/* Call to Interrupt setup */
	status = Interrupt_System_Setup(&INTCInst);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}

	/* Connect GPIO Interrupt to handler */
	status = XScuGic_Connect(&INTCInst, INTC_GPIO_INTERRUPT_ID, (Xil_ExceptionHandler) BTN_Intr_Handler, (void *)GpioInstancePtr);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}

	/* Enable GPIO Interrupts */
	XGpio_InterruptEnable(GpioInstancePtr, BTN_INT);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);

	/* Enable GPIO and Timer interrupts in controller */
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_ID);

	return XST_SUCCESS;
}

int LEDOutput_Eample(void)
{
	volatile int Delay;
	int led = LED; 						/* hold current LED value */

	/* Loop forever blinking LED */
	while(1){
		/* Write output to LEDs */
		XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, led);

		/* Flip LEDs */
		led = led << 1;

		if (led > 0x80)
			led = 1;

		/* Delay */
		for(Delay = 0; Delay < LED_DELAY; Delay++);
		printf("In While loop\r\n");
	}
	return XST_SUCCESS; /* should be unreachable */
}
int main()
{
    init_platform();

    int Status;
    int status;
    status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
    if (status != XST_SUCCESS){
    	return XST_FAILURE;
    }
    /* Set LEDs direction to Output */
    XGpio_SetDataDirection(&Gpio, LED_CHANNEL, 0x00);
    /*Status=LEDOutput_Eample();
    if (Status != XST_SUCCESS){
        return XST_FAILURE;
    }*/
    /* Set buttons direction to Input */
    XGpio_SetDataDirection(&Gpio, BUTTONS_CHANNEL, 0xFF);

    status = Intc_Init_Function(INTC_DEVICE_ID, &Gpio);
	if (status != XST_SUCCESS){
	   	return XST_FAILURE;
	}
    while(1)
    	;
    return 0;
}

