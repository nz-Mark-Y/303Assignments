/* COMPSYS 303 Assignment 2
 * DDD Pacemaker (Dual sensing, Dual pacing, Dual mode)
 *
 * Mode0: Heart signals represented by Key0 and Key1 presses
 * Mode1: Heart signals received via UART using a non-blocking fgetc function
 *
 * Any dip switch can be used to change the mode (all switches down == mode0)
 * After a mode switch is changed then the system must be reset (using Key3) for it to successfully change modes
 *
 * For mode 1, we have included logic for when timers should start and stop in regards to sent and received V and A signals.
 * Only timeout flags are communicated between the SCCharts implementation and the Nios II implementation, for minimal coupling.
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
#include <sys/_default_fcntl.h>
#include "sccharts.h"
#include "defines.h"
//=============================================
void mode0();
void mode1();
void init_buttons_pio();
void buttons_isr();
void init_uart();

void initialise_avi_timer();
void initialise_aei_timer();
void initialise_pvarp_timer();
void initialise_vrp_timer();
void initialise_lri_timer();
void initialise_uri_timer();
void initialise_debounce_timer();
void initialise_LED_timer();
void initialise_avi_signal_timer();
void initialise_aei_signal_timer();
void initialise_pvarp_signal_timer();
void initialise_vrp_signal_timer();
void initialise_lri_signal_timer();
void initialise_uri_signal_timer();

alt_u32 avi_isr_function();
alt_u32 aei_isr_function();
alt_u32 pvarp_isr_function();
alt_u32 vrp_isr_function();
alt_u32 lri_isr_function();
alt_u32 uri_isr_function();
alt_u32 debounce_isr_function();
alt_u32 LED_isr_function();

alt_u32 signal_avi_isr_function();
alt_u32 signal_aei_isr_function();
alt_u32 signal_pvarp_isr_function();
alt_u32 signal_vrp_isr_function();
alt_u32 signal_lri_isr_function();
alt_u32 signal_uri_isr_function();
//=============================================
static alt_alarm avi_timer;																			// Timers
static alt_alarm aei_timer;
static alt_alarm pvarp_timer;
static alt_alarm vrp_timer;
static alt_alarm lri_timer;
static alt_alarm uri_timer;
static alt_alarm debounce_timer;
static alt_alarm LED_timer;
static alt_alarm avi_signal_timer;
static alt_alarm aei_signal_timer;
static alt_alarm pvarp_signal_timer;
static alt_alarm vrp_signal_timer;
static alt_alarm lri_signal_timer;
static alt_alarm uri_signal_timer;

static int buttonValue = 1;																			// Flag to hold which button is pressed
static volatile int vpace_LED_on = 0;																// Flag to hold if the VPace LED should be on or off
static volatile int apace_LED_on = 0;																// Flag to hold if the APace LED should be on or off
static volatile int aei_running = 0;																// Flag to hold if AEI is running
static volatile int pvarp_running = 0;																// Flag to hold if PVARP is running
static volatile int vrp_running = 0;     															// Flag to hold if VRP is running

int fp, c;																							// Variables for UART read and write
char* uart_read;																					// Buffer character pointer for uart read
//=============================================
int main() {
	printf("Welcome to the Nios II Pacemaker! \n");

	reset();
	if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) {
		init_buttons_pio();																			// Initialise buttons if in correct mode
	} else {
		init_uart();																				// Initialise uart if in correct mode
	}

	while(1) {
		tick();

		if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) {										// Select mode
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
	if (VPace == 1) {																			 	// VPace signal handler
		printf("** vpace set **\n");
		if (vrp_running == 1) {																		// If in VRP phase, ignore as VR
			return;
		}
		initialise_pvarp_timer();
		initialise_vrp_timer();
		if (aei_running == 0) {																		// If AEI timer is running, don't start AEI again
			initialise_aei_timer();
		}
		initialise_lri_timer();
		initialise_uri_timer();
		vpace_LED_on = 1;																			// Set LED on
		initialise_LED_timer(&vpace_LED_on);
	}
	if (APace == 1) {																				// APace signal handler
		printf("** apace set **\n");
		if (pvarp_running == 1) {																	// If in PVARP phase, ignore as AR
			return;
		}
		initialise_avi_timer();
		apace_LED_on = 1;																			// Set LED on
		initialise_LED_timer(&apace_LED_on);
	}

	if (vpace_LED_on == 1) {																		// LED Logic
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x01);											// LED 1
	} else if (apace_LED_on == 1) {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x02);											// LED 2
	} else {
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x00);											// No LEDs
	}
}

void mode1() {
	/*
	 * Contains logic for timers, inputs, buttons and outputs for Mode 1
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */

	c = read(fp, uart_read, 1);																		// Read from UART into buffer, non blocking
	if (c) {
		if (*uart_read != 0) {																		// Ignore an empty buffer
			if(*uart_read == 'A') {																	// ASense received
				if (pvarp_running == 1) {															// If in PVARP phase, ignore as AR
					printf("ar registered\n");
					*uart_read = 0;																	// Clear buffer
					return;
				}
				printf("asense received\n");
				initialise_avi_timer();
				ASense = 1;																			// Set ASense as sensed
				initialise_debounce_timer(&ASense);
			} else if (*uart_read == 'V') {
				if (vrp_running == 1) {																// If in VRP phase, ignore as VR
					printf("vr registered\n");
					*uart_read = 0;																	// Clear buffer
					return;
				}
				printf("vsense received\n");
				initialise_pvarp_timer();
				initialise_vrp_timer();
				if (aei_running == 0) {																// If AEI timer is running, don't start AEI again
					initialise_aei_timer();
				}
				initialise_lri_timer();
				initialise_uri_timer();
				VSense = 1;																			// Set VSense as sensed
				initialise_debounce_timer(&VSense);
			} else {
				VSense = 0;
				ASense = 0;
			}
			*uart_read = 0;																			// Clear buffer
		} else {
			VSense = 0;
			ASense = 0;
		}
	} else {
		VSense = 0;
		ASense = 0;
	}

	if (VPace == 1) {																			 	// VPace signal handler
		printf("** vpace set **\n");
		if (vrp_running == 1) {																		// If in VRP phase, ignore as VR
			return;
		}
		initialise_pvarp_timer();
		initialise_vrp_timer();
		if (aei_running == 0) {																		// If AEI timer is running, don't start AEI again
			initialise_aei_timer();
		}
		initialise_lri_timer();
		initialise_uri_timer();
		write(fp,"V",1);
	}
	if (APace == 1) {																				// APace signal handler
		printf("** apace set **\n");
		if (pvarp_running == 1) {																	// If in PVARP phase, ignore as AR
			return;
		}
		initialise_avi_timer();
		write(fp,"A",1);
	}
}

void init_uart() {
	fp = open(UART_NAME, O_NONBLOCK | O_RDWR);

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
	if ((*temp & 0x01) > 0) {																		// VSense button
		if (vrp_running == 1) {																		// If in VRP phase, ignore as VR
			printf("vr registered\n");
			return;
		}
		printf("vsense pressed\n");
		initialise_pvarp_timer();
		initialise_vrp_timer();
		if (aei_running == 0) {																		// If AEI timer is running, don't start AEI again
			initialise_aei_timer();
		}
		initialise_lri_timer();
		initialise_uri_timer();
		VSense = 1;																					// Set VSense as pressed
		initialise_debounce_timer(&VSense);
	}
	if ((*temp & 0x02) > 0) {																		// ASense button
		if (pvarp_running == 1) {																	// If in PVARP phase, ignore as AR
			printf("ar registered\n");
			return;
		}
		printf("asense pressed\n");
		initialise_avi_timer();
		ASense = 1;																					// Set ASense as pressed
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
	alt_alarm_stop(&avi_timer);																		// Stop the timer before running it again
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
	aei_running = 1;																				// Update that AEI timer is running
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
	pvarp_running = 1;																				// Update that PVARP timer is running
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
	vrp_running = 1;																				// Update that VRP timer is running
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
	LRITO = 0;
	int lri_context = 0;
	void* lri_timer_context = (void*) &lri_context;
	alt_alarm_stop(&lri_timer);																		// Stop LRI timer before running it again
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
	alt_alarm_stop(&uri_timer);																		// Stop URI timer before running it again
	alt_alarm_start(&uri_timer, URI_VALUE, uri_isr_function, uri_timer_context);
}

void initialise_debounce_timer(int* context) {
	/*
	 * Initialises the timer for measuring the debounce period for switches
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* debounce_timer_context = (void*) context;
	alt_alarm_start(&debounce_timer, SIGNAL_VALUE, debounce_isr_function, debounce_timer_context);
}

void initialise_LED_timer(int* context) {
	/*
	 * Initialises the timer for measuring the LED extension period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* LED_timer_context = (void*) context;
	alt_alarm_start(&LED_timer, DEBOUNCE_VALUE, LED_isr_function, LED_timer_context);
}

void initialise_avi_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the avi signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&avi_signal_timer, SIGNAL_VALUE, signal_avi_isr_function, signal_timer_context);
}

void initialise_aei_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the aei signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&aei_signal_timer, SIGNAL_VALUE, signal_aei_isr_function, signal_timer_context);
}

void initialise_pvarp_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the pvarp signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&pvarp_signal_timer, SIGNAL_VALUE, signal_pvarp_isr_function, signal_timer_context);
}

void initialise_vrp_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the vrp signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&vrp_signal_timer, SIGNAL_VALUE, signal_vrp_isr_function, signal_timer_context);
}

void initialise_lri_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the lri signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&lri_signal_timer, SIGNAL_VALUE, signal_lri_isr_function, signal_timer_context);
}

void initialise_uri_signal_timer(char* context) {
	/*
	 * Initialises the timer for measuring the uri signal high period
	 *
	 * Parameters: NONE
	 * Returns: NONE
	 */
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&uri_signal_timer, SIGNAL_VALUE, signal_uri_isr_function, signal_timer_context);
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
	initialise_avi_signal_timer(&AVITO);
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
	aei_running = 0;
	initialise_aei_signal_timer(&AEITO);
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
	pvarp_running = 0;
	initialise_pvarp_signal_timer(&PVARPTO);
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
	vrp_running = 0;
	initialise_vrp_signal_timer(&VRPTO);
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
	initialise_lri_signal_timer(&LRITO);
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
	initialise_uri_signal_timer(&URITO);
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
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 signal_avi_isr_function(void* context) {
	/*
	 * ISR function that is called when avi signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
alt_u32 signal_aei_isr_function(void* context) {
	/*
	 * ISR function that is called when aei signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
alt_u32 signal_pvarp_isr_function(void* context) {
	/*
	 * ISR function that is called when pvarp signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
alt_u32 signal_vrp_isr_function(void* context) {
	/*
	 * ISR function that is called when vrp signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
alt_u32 signal_lri_isr_function(void* context) {
	/*
	 * ISR function that is called when lri signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
alt_u32 signal_uri_isr_function(void* context) {
	/*
	 * ISR function that is called when uri signal timer times out
	 * Sets the context variable (reference to the signal high value) to 0
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

