/* COMPSYS 303 Assignment 2
 * DDD Pacemaker (Dual sensing, Dual pacing, Dual mode)
 *
 * Mode0: Heart signals represented by Key0 and Key1 presses
 * Mode1: Heart signals received via UART using a non-blocking fgetc function
 *
 * Any dip switch can be used to change the mode (all switches down == mode0)
 * After a mode switch is changed then the system must be reset (using Key3) for it to successfully change modes
 *
 * Savi Mohan
 * Mark Yep
 * (Group 3)
 */
//=============================================
#include <system.h>
#include <sys/alt_alarm.h>
#include <sys/alt_irq.h>
#include <altera_avalon_pio_regs.h>
#include <alt_types.h>
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sccharts.h"
//=============================================
#define AVI_VALUE 600
#define AEI_VALUE 1600
#define PVARP_VALUE 100
#define VRP_VALUE 300
#define LRI_VALUE 1900
#define URI_VALUE 1800

#define DEBOUNCE_VALUE 200
#define SIGNAL_VALUE 3
//=============================================
void mode0();
void mode1();
void init_buttons_pio();
void buttons_isr();

void initialise_avi_timer();
void initialise_aei_timer();
void initialise_pvarp_timer();
void initialise_vrp_timer();
void initialise_lri_timer();
void initialise_uri_timer();
void initialise_debounce_timer();
void initialise_LED_timer();
void initialise_signal_timer();

alt_u32 avi_isr_function();
alt_u32 aei_isr_function();
alt_u32 pvarp_isr_function();
alt_u32 vrp_isr_function();
alt_u32 lri_isr_function();
alt_u32 uri_isr_function();
alt_u32 debounce_isr_function();
alt_u32 LED_isr_function();
alt_u32 signal_isr_function();
//=============================================
static alt_alarm avi_timer;																			// Timers
static alt_alarm aei_timer;
static alt_alarm pvarp_timer;
static alt_alarm vrp_timer;
static alt_alarm lri_timer;
static alt_alarm uri_timer;
static alt_alarm debounce_timer;
static alt_alarm LED_timer;
static alt_alarm signal_timer;

static int buttonValue = 1;																			// Flag to hold which button is pressed
static volatile int vpace_LED_on = 0;																// Flag to hold if the VPace LED should be on or off
static volatile int apace_LED_on = 0;																// Flag to hold if the APace LED should be on or off
//=============================================
int main() {
	printf("Hello from Nios II!\n");

	reset();
	if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) {											// Initialise buttons if in correct mode
		init_buttons_pio();
	}

	while(1) {
		tick();

		if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) {
			mode0();
		} else {
			mode1();
		}
	}
	return 0;
}

void mode0() {
	/*
	 * Contains logic for timers, inputs, buttons and outputs for Mode 0
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	if (AVITO != 0) {
		printf("hello");
	}

	if (VPace == 1) {																			 	// VPace signal handler
		printf("vpace set\n");
		initialise_pvarp_timer();
		initialise_vrp_timer();
		initialise_aei_timer();
		initialise_lri_timer();
		initialise_uri_timer();
		vpace_LED_on = 1;
		initialise_LED_timer(&vpace_LED_on);
	}
	if (APace == 1) {																				// APace signal handler
		printf("apace set\n");
		initialise_avi_timer();
		apace_LED_on = 1;
		initialise_LED_timer(&apace_LED_on);
	}

	if (vpace_LED_on == 1) {																		// LED Logic
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x01);
	} else if (apace_LED_on == 1) {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x02);
	} else {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x00);
	}
}

void mode1() {
	/*
	 * Contains logic for timers, inputs, buttons and outputs for Mode 1
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */

}

void init_buttons_pio() {
	/*
	 * Initialises buttons and their interrupts for mode 0
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* context_going_to_be_passed = (void*) &buttonValue; 										// Cast before passing to ISR
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0); 												// Clears the edge capture register
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7); 											// Enable interrupts for all buttons
	alt_irq_register(BUTTONS_IRQ, context_going_to_be_passed, buttons_isr); 						// Register the ISR
}

void buttons_isr(void* context) {
	/*
	 * The interrupt service routine for any button press
	 * Determines which button is pressed, sets necessary bits and starts corresponding timers
	 *
	 * Parameters: context
	 * Returns: NONE
	 */
	int* temp = (int*) context;
	(*temp) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);												// Clear the edge capture register
	if ((*temp & 0x01) > 0) {
		printf("vsense pressed\n");
		initialise_pvarp_timer();
		initialise_vrp_timer();
		initialise_aei_timer();
		initialise_lri_timer();
		initialise_uri_timer();
		VSense = 1;																					// VSense button is pressed
		initialise_debounce_timer(&VSense);
	}
	if ((*temp & 0x02) > 0) {
		printf("asense pressed\n");
		initialise_avi_timer();
		ASense = 1;																					// ASense button is pressed
		initialise_debounce_timer(&ASense);
	}
}

void initialise_avi_timer() {
	/*
	 * Initialises the timer for measuring the AVI period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("avi timer start\n");
	int avi_context = 0;
	void* avi_timer_context = (void*) &avi_context;
	alt_alarm_start(&avi_timer, AVI_VALUE, avi_isr_function, avi_timer_context);
}

void initialise_aei_timer() {
	/*
	 * Initialises the timer for measuring the AEI period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("aei timer start\n");
	int aei_context = 0;
	void* aei_timer_context = (void*) &aei_context;
	alt_alarm_start(&aei_timer, AEI_VALUE, aei_isr_function, aei_timer_context);
}

void initialise_pvarp_timer() {
	/*
	 * Initialises the timer for measuring the PVARP period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("pvarp timer start\n");
	int pvarp_context = 0;
	void* pvarp_timer_context = (void*) &pvarp_context;
	alt_alarm_start(&pvarp_timer, PVARP_VALUE, pvarp_isr_function, pvarp_timer_context);
}

void initialise_vrp_timer() {
	/*
	 * Initialises the timer for measuring the VRP period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("vrp timer start\n");
	int vrp_context = 0;
	void* vrp_timer_context = (void*) &vrp_context;
	alt_alarm_start(&vrp_timer, VRP_VALUE, vrp_isr_function, vrp_timer_context);
}

void initialise_lri_timer() {
	/*
	 * Initialises the timer for measuring the LRI period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("lri timer start\n");
	int lri_context = 0;
	void* lri_timer_context = (void*) &lri_context;
	alt_alarm_start(&lri_timer, LRI_VALUE, lri_isr_function, lri_timer_context);
}

void initialise_uri_timer() {
	/*
	 * Initialises the timer for measuring the URI period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("uri timer start\n");
	int uri_context = 0;
	void* uri_timer_context = (void*) &uri_context;
	alt_alarm_start(&uri_timer, URI_VALUE, uri_isr_function, uri_timer_context);
}

void initialise_debounce_timer(int* context) {
	/*
	 * Initialises the timer for measuring the debounce period for switches
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("debounce timer start\n");
	void* debounce_timer_context = (void*) context;
	alt_alarm_start(&debounce_timer, DEBOUNCE_VALUE, debounce_isr_function, debounce_timer_context);
}

void initialise_LED_timer(int* context) {
	/*
	 * Initialises the timer for measuring the LED extension period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("led timer start\n");
	void* LED_timer_context = (void*) context;
	alt_alarm_start(&LED_timer, DEBOUNCE_VALUE, LED_isr_function, LED_timer_context);
}

void initialise_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	printf("signal timer start\n");
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&signal_timer, SIGNAL_VALUE, signal_isr_function, signal_timer_context);
}


alt_u32 avi_isr_function(void* context) {
	/*
	 * ISR function that is called when AVI timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("avi timer end\n");
	AVITO = 1;
	initialise_signal_timer(&AVITO);
	return 0;
}

alt_u32 aei_isr_function(void* context) {
	/*
	 * ISR function that is called when AEI timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("aei timer end\n");
	AEITO = 1;
	initialise_signal_timer(&AEITO);
	return 0;
}

alt_u32 pvarp_isr_function(void* context) {
	/*
	 * ISR function that is called when PVARP timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("pvarp timer end\n");
	PVARPTO = 1;
	initialise_signal_timer(&PVARPTO);
	return 0;
}

alt_u32 vrp_isr_function(void* context) {
	/*
	 * ISR function that is called when VRP timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("vrp timer end\n");
	VRPTO = 1;
	initialise_signal_timer(&VRPTO);
	return 0;
}

alt_u32 lri_isr_function(void* context) {
	/*
	 * ISR function that is called when LRI timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("lri timer end\n");
	LRITO = 1;
	initialise_signal_timer(&LRITO);
	return 0;
}

alt_u32 uri_isr_function(void* context) {
	/*
	 * ISR function that is called when URI timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("uri timer end\n");
	URITO = 1;
	initialise_signal_timer(&URITO);
	return 0;
}

alt_u32 debounce_isr_function(void* context) {
	/*
	 * ISR function that is called when debounce timer times out
	 * Sets the context variable (reference to the button pressed value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("debounce timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 LED_isr_function(void* context) {
	/*
	 * ISR function that is called when LED timer times out
	 * Sets the context variable (reference to the LED on value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("led timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 signal_isr_function(void* context) {
	/*
	 * ISR function that is called when signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("signal timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

