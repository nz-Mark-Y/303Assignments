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

alt_u32 avi_isr_function();
alt_u32 aei_isr_function();
alt_u32 pvarp_isr_function();
alt_u32 vrp_isr_function();
alt_u32 lri_isr_function();
alt_u32 uri_isr_function();
alt_u32 debounce_isr_function();
alt_u32 LED_isr_function();
//=============================================
static alt_alarm avi_timer;																			// Timers
static alt_alarm aei_timer;
static alt_alarm pvarp_timer;
static alt_alarm vrp_timer;
static alt_alarm lri_timer;
static alt_alarm uri_timer;
static alt_alarm debounce_timer;
static alt_alarm LED_timer;

static int buttonValue = 1;																			// Flag to hold which button is pressed
static volatile int vpace_LED_on = 0;																// Flag to hold if the VPace LED should be on or off
static volatile int apace_LED_on = 0;																// Flag to hold if the APace LED should be on or off
static volatile int pvarp_running = 0;																// Flag to hold if PVARP is running
static volatile int vrp_running = 0;     															// Flag to hold if VRP is running
static volatile int aei_running = 0;
static volatile int crit_flag = 0;
static volatile int setAVITO = 0;
static volatile int setAEITO = 0;
static volatile int setPVARPTO = 0;
static volatile int setVRPTO = 0;
static volatile int setLRITO = 0;
static volatile int setURITO = 0;

int fp, c;																							// Variables for UART read and write
char* uart_read;																					// Buffer character pointer for uart read
int mode = 0;
int previous_switch_value = 2;
int current_switch_value = 0;
//=============================================
int main() {
	printf("Welcome to the Nios II Pacemaker! \n");
	reset();

	while(1) {
		if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE)) {
			current_switch_value = 1;
		} else {
			current_switch_value = 0;
		}
		if (current_switch_value != previous_switch_value) {
			alt_alarm_stop(&avi_timer);
			alt_alarm_stop(&aei_timer);
			alt_alarm_stop(&pvarp_timer);
			alt_alarm_stop(&vrp_timer);
			alt_alarm_stop(&lri_timer);
			alt_alarm_stop(&uri_timer);
			if (current_switch_value) {
				mode = 1;
				init_uart();
			} else {
				mode = 0;
				init_buttons_pio();
				fp.close();
			}
			reset();
		}	
		crit_flag = 1;																				// Enter critical section
		tick();																						// Tick
		AVITO = 0;																					// Reset flags
		AEITO = 0;
		PVARPTO = 0;
		VRPTO = 0;
		LRITO = 0;
		URITO = 0;
		crit_flag = 0;																				// Exit critical section
		if (setAVITO) {																				// Update flags if necessary
			AVITO = 1;
			setAVITO = 0;
		}
		if (setAEITO) {
			AEITO = 1;
			setAEITO = 0;
		}
		if (setPVARPTO) {
			PVARPTO = 1;
			setPVARPTO = 0;
		}
		if (setVRPTO) {
			VRPTO = 1;
			setVRPTO = 0;
		}
		if (setLRITO) {
			LRITO = 1;
			setLRITO = 0;
		}
		if (setURITO) {
			URITO = 1;
			setURITO = 0;
		}

		if (mode == 0) {										// Select mode
			mode0();
		} else {
			mode1();
		}
		previous_switch_value = current_switch_value;
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
		if (aei_running == 0) {
			initialise_aei_timer();	//initialise AEI if it is not running
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
				initialise_aei_timer();
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
		initialise_aei_timer();
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
	fp = open(UART_NAME, O_NONBLOCK | O_RDWR);														// Open file pointer for non blocking, rw
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
		if (aei_running == 0) {
			initialise_aei_timer();				//initialise AEI if it is not running
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
	int aei_context = 0;																			// Update that AEI timer is running
	void* aei_timer_context = (void*) &aei_context;
	aei_running = 1;
	alt_alarm_stop(&aei_timer);
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
	alt_alarm_stop(&pvarp_timer);
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
	alt_alarm_stop(&vrp_timer);
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
	URI_NOTRUNNING = 0;
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

alt_u32 avi_isr_function(void* context) {
	/*
	 * ISR function that is called when AVI timer times out
	 *
	 * Parameters: context
	 * Returns: alt_u32 representing the next timer value
	 */
	printf("avi timer end\n");
	if (crit_flag) {																				// Check for critical section
		setAVITO = 1;																				// If in crit section, set flag to be updated
	} else {
		AVITO = 1;																					// Else update flag
	}
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
	aei_running = 0;
	if (crit_flag) {
		setAEITO = 1;
	} else {
		AEITO = 1;
	}
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
	pvarp_running = 0;
	if (crit_flag) {
		setPVARPTO = 1;
	} else {
		PVARPTO = 1;
	}
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
	vrp_running = 0;
	if (crit_flag) {
		setVRPTO = 1;
	} else {
		VRPTO = 1;
	}
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
	if (crit_flag) {
		setLRITO = 1;
	} else {
		LRITO = 1;
	}
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
	URI_NOTRUNNING = 1;
	if (crit_flag) {
		setURITO = 1;
	} else {
		URITO = 1;
	}
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
