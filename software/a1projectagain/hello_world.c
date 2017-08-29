/* Traffic Light Controller */

#include <system.h>
#include <sys/alt_alarm.h>
#include <sys/alt_irq.h>
#include <altera_avalon_pio_regs.h>
#include <alt_types.h>
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// FUNCTION PROTOTYPES ========================
// Timer ISRs ---------------------------------
alt_u32 tlc_timer_isr(void* context);
alt_u32 camera_timer_isr(void* context);

// TLC state machine functions ----------------
void init_tlc();
void simple_tlc(int* state);
void pedestrian_tlc(int* state);
void configurable_tlc(int* state);
int config_tlc(int *tl_state);
void camera_tlc(int* state);

// Button Inputs / Interrupts -----------------
void buttons_driver(unsigned int* button);
void handle_mode_button(unsigned int* buttonState);
void handle_vehicle_button(void);
void init_buttons_pio(void);
void NSEW_ped_isr(void* context, alt_u32 id);

// Configuration Functions --------------------
int update_timeout(void);
void config_isr(void* context, alt_u32 id);
void buffer_timeout(int value);
void timeout_data_handler(void);

void printToUART(char* stringToPrint);

// CONSTANTS ==================================
#define OPERATION_MODES 0x03	// number of operation modes (00 - 03 = 4 modes)
#define CAMERA_TIMEOUT	2000	// timeout period of red light camera (in ms)
#define TIMEOUT_NUM 6			// number of timeouts
#define TIME_LEN 8				// buffer length for time digits

// USER DATA TYPES ============================
// Timeout buffer structure -------------------
typedef struct  {
	int index;
	unsigned int timeout[TIMEOUT_NUM];
} TimeBuf;

// GLOBAL VARIABLES ===========================
static alt_alarm tlc_timer;		// alarm used for traffic light timing
static alt_alarm camera_timer;	// alarm used for camera timing

// NOTE: --------------------------------------
// set contexts for ISRs to be volatile to avoid unwanted Compiler optimisation
static volatile int tlc_timer_event = 0;
static volatile int camera_timer_event = 0;
static volatile int pedestrianNS = 0;
static volatile int pedestrianEW = 0;
static volatile int newTimeoutValues = 0;
static volatile int timerHit = 0;

// 4 States of 'Detection': -------------------
// Car Absent : 0
// Car Enters : 1
// Car is Detected running a Red : 2
// Car Leaves : 3
static int vehicle_detected = 0;

// Traffic light timeouts ---------------------
static unsigned int timeout[TIMEOUT_NUM] = {500, 6000, 2000, 500, 6000, 2000};
static TimeBuf timeout_buf = { -1, {500, 6000, 2000, 500, 6000, 2000} };

// UART ---------------------------------------
FILE* fp;
static volatile int c;
static volatile int timeoutValue;
char chararray[5] = {'0', '0', '0', '0', '\0'};
static volatile int valueCount = 0;
static unsigned int tempBuffer[TIMEOUT_NUM] = {500, 6000, 2000, 500, 6000, 2000};

static unsigned int mode = 0;
static int proc_state[OPERATION_MODES + 1] = {-1, -1, -1, -1}; // Process states: use -1 as initialisation state
static int camera_count = 1;
static int buttonValue = 1;

static int snapshotTaken = 0;
static char* timeTaken = 0;
static int toPrint = 0;
static char countString[10];
// Code =======================================
// Initialise the traffic light controller for all modes
void init_tlc() {
	void* timerContext = (void*) mode;
	alt_alarm_start(&tlc_timer, timeout[0], tlc_timer_isr, timerContext);
}

/* DESCRIPTION: Simple traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
void simple_tlc(int* state) {
	if (*state == -1) {
		init_tlc();
		(*state)++;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);	// both traffic lights will be red by default
		return;
	}

	if (tlc_timer_event == 1) {
		if (*state == 0) { // R, R state
			*state = 1; // G, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x0C);
		} else if (*state == 1) {
			*state = 2; // Y, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x14);
		} else if (*state == 2) {
			*state = 3; // R, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		} else if (*state == 3) {
			*state = 4; // R, G
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x21);
		} else if (*state == 4) {
			*state = 5; // R, Y
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x22);
		} else {
			*state = 0; // R, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		}
		tlc_timer_event = 0;
		return;
	}
}


/* DESCRIPTION: Handles the traffic light timer interrupt
 * PARAMETER:   context - opaque reference to user data
 * RETURNS:     Number of 'ticks' until the next timer interrupt. A return value
 *              of zero stops the timer.
 */
alt_u32 tlc_timer_isr(void* context) {
	int currentState = proc_state[mode];
	if (currentState != 5) {
		currentState++;
	} else {
		currentState = 0;
	}
	int	nextTimeout = timeout[currentState];
	printf("next timeout: %d\n", nextTimeout);
	printf("current state: %d\n", currentState);
	tlc_timer_event = 1;
	timerHit = 1;
	return nextTimeout;
}

/* DESCRIPTION: Handles the NSEW pedestrian button interrupt
 * PARAMETER:   context - opaque reference to user data
 *              id - hardware interrupt number for the device
 * RETURNS:     none
 */
void NSEW_ped_isr(void* context, alt_u32 id) {
	int* temp = (int*) context;
	(*temp) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0); // clear the edge capture register
	if ((*temp & 0x01) > 0) {
		pedestrianNS = 1;
	}
	if ((*temp & 0x02) > 0) {
		pedestrianEW = 1;
	}
	if ((*temp & 0x04) > 0) {
		if (vehicle_detected == 0) {
			printf("Vehicle Enters\n");
			vehicle_detected = 1; // If vehicle absent, button press means vehicle has entered intersection
		} else {
			printf("Vehicle Leaves\n");
			vehicle_detected = 0; // If at any other time, button press means vehicle has left intersection
		}
	}
	printf("button: %i\n", *temp);
}

/* DESCRIPTION: Initialise the interrupts for pedestrian buttons
 * PARAMETER:   none
 * RETURNS:     none
 */
void init_buttons_pio(void) {
	void* context_going_to_be_passed = (void*) &buttonValue; 								// cast before passing to ISR
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0); 										// clears the edge capture register
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7); 									// enable interrupts for all buttons
	alt_irq_register(BUTTONS_IRQ,context_going_to_be_passed, NSEW_ped_isr); 				// register the ISR
}


/* DESCRIPTION: Pedestrian traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
void pedestrian_tlc(int* state) {
	if (*state == -1) {
		init_tlc();
		(*state)++;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		return;
	}

	if (tlc_timer_event == 1) {
		if (*state == 0) { // R, R state
			if (pedestrianNS == 1) {
				*state = 1; // G, R
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x4C);
				pedestrianNS = 0;
			} else {
				*state = 1; // G, R
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x0C);
			}
		} else if (*state == 1) {
			*state = 2; // Y, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x14);
		} else if (*state == 2) {
			*state = 3; // R, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		} else if (*state == 3) {
			if (pedestrianEW == 1) {
				*state = 4; // R, G, P2
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0xA1);
				pedestrianEW = 0;
			} else {
				*state = 4; // R, G
				IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x21);
			}
		} else if (*state == 4) {
			*state = 5; // R, Y
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x22);
		} else {	// this accounts for state 5
			*state = 0; // R, R
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		}
		tlc_timer_event = 0;
		return;
	}
}

/* DESCRIPTION: Configurable traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
/*
If there is new configuration data... Load it.
Else run pedestrian_tlc();
*/
void configurable_tlc(int* state) {
	if (*state == -1) {
		init_tlc();
		(*state)++;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		return;
	}
	newTimeoutValues = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);
	if (((*state == 0) || (*state == 3)) && (newTimeoutValues > 3) && (timerHit == 1)){
		printToUART("Enter values now\n\r");
		timeout_data_handler();
		newTimeoutValues = 0;
		pedestrian_tlc(state);
	} else {
		pedestrian_tlc(state);
	}
}

/* DESCRIPTION: Implements the state machine of the traffic light controller in
 *              the ***configuration*** phase
 * PARAMETER:   tl_state - state of the traffic light
 * RETURNS:     Returns the state of the configuration phase
 */
/*
Puts the TLC in a 'safe' state... then begins update
*/
int config_tlc(int* tl_state) {
	// State of configuration
	static int state = 0;

	if (*tl_state == -1) {
		// Process initialisation state
		state = 0;
		return 0;
	}

	return state;
}


/* DESCRIPTION: Parses the configuration string and updates the timeouts
 * PARAMETER:   none
 * RETURNS:     none
 */
/*
 buffer_timeout() must be used 'for atomic transfer to the main timeout buffer'
*/
void timeout_data_handler(void) {
	fp = fopen(UART_NAME, "rw"); // open up UART with read and write access
	if (fp != NULL) {// check if the UART is open successfully
		int k = 0;
		while(1) {
			c = fgetc(fp);
			if (c== '\n') {
				break;
			}
			if (c == '\r') {
				break;
			}
			if (c == ',') {
				int a;
				for(a=0;a<(4-k);a++){
					chararray[3]=chararray[2];
					chararray[2]=chararray[1];
					chararray[1]=chararray[0];
					chararray[0] = '0';
				}
				sscanf(chararray, "%d", &timeoutValue);
				tempBuffer[valueCount] = timeoutValue;
				chararray[0] = '0';
				chararray[1] = '0';
				chararray[2] = '0';
				chararray[3] = '0';
				chararray[4] = '\0';
				k = 0;
				valueCount += 1;
			} else {
				chararray[k] = c;
				k += 1;
			}
		}
		fclose(fp); // remember to close the file
	}

	if (valueCount == 5) {
		int j;
		for (j=0; j<6; j++) {
			if (tempBuffer[j] <= 0) {
				printToUART("Invalid values\n\r");
				return;
			}
		}
		for (j=0; j<6; j++) {
			timeout[j]=tempBuffer[j];
		}
		timerHit = 0;
	}
	valueCount = 0;
}


/* DESCRIPTION: Stores the new timeout values in a secondary buffer for atomic
 *              transfer to the main timeout buffer at a later stage
 * PARAMETER:   value - value to store in the buffer
 * RETURNS:     none
 */
void buffer_timeout(int value) {

}


/* DESCRIPTION: Implements the update operation of timeout values as a critical
 *              section by ensuring that timeouts are fully received before
 *              allowing the update
 * PARAMETER:   none
 * RETURNS:     1 if update is completed; 0 otherwise
 */
int update_timeout(void) {

	return 0;
}

/* DESCRIPTION: Handles the red light camera timer interrupt
 * PARAMETER:   context - opaque reference to user data
 * RETURNS:     Number of 'ticks' until the next timer interrupt. A return value
 *              of zero stops the timer.
 */
alt_u32 camera_timer_isr(void* context) {
	volatile int* trigger = (volatile int*)context;
	(*trigger)++;
	if (*trigger == CAMERA_TIMEOUT) {
		snapshotTaken = 1;
		toPrint = 1;
		return 0;
	}
	if (vehicle_detected != 2) {
		int num = *trigger;
		sprintf(countString, "%d", num);
		printf(countString);
		snapshotTaken = 0;
		toPrint = 1;
		return 0;
	}
	return 1;
}

/* DESCRIPTION: Camera traffic light controller
 * PARAMETER:   state - state of the controller
 * RETURNS:     none
 */
 /*
 Same functionality as configurable_tlc
 But also handles Red-light camera
 */
void camera_tlc(int* state) {
	if (*state == -1) {
		init_tlc();
		(*state)++;
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x24);
		return;
	}

	if (toPrint == 1) {
		if (snapshotTaken == 1) {
			snapshotTaken = 0;
			printToUART("Snapshot Taken\n\r");
			toPrint = 0;
		} else {
			printToUART("Time taken: ");
			printToUART(countString);
			printToUART("\n\r");
			timeTaken = 0;
			toPrint = 0;
		}
	}

	newTimeoutValues = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE);
	if (((*state == 0) || (*state == 3)) && (newTimeoutValues > 3) && (timerHit == 1)){
		printToUART("Enter values now\n\r");
		timeout_data_handler();
		newTimeoutValues = 0;
		pedestrian_tlc(state);
	} else {
		pedestrian_tlc(state);
	}

	if (((*state == 2) || (*state == 5)) && (vehicle_detected == 1)) { // One light yellow and vehicle enters
		printToUART("Camera Activated\n\r");
		camera_count = 0;
		vehicle_detected = 2;
		void* cameraContext = (void*) &camera_count;
		alt_alarm_start(&camera_timer, 1, camera_timer_isr, cameraContext);
	}
}

void printToUART(char* stringToPrint) {
	fp = fopen(UART_NAME, "w");
	if (fp != NULL) {
		fprintf(fp, "%s", stringToPrint);
		fclose(fp);
	}
}

int main(void) {
	FILE *lcd;
	lcd = fopen(LCD_NAME, "w");

	printf("Hello\n");

	init_buttons_pio();			// initialise buttons

	while (1) {
		if ((proc_state[mode] == -1) || (proc_state[mode] == 0) || (proc_state[mode] == 3)) {
			if ((IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) || (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 4)) {
				if (mode != 0) {
					mode = 0;
					alt_alarm_stop(&tlc_timer);
					proc_state[0] = -1;
				}
			}
			if ((IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 1) || (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 5)) {
				if (mode != 1) {
					mode = 1;
					alt_alarm_stop(&tlc_timer);
					proc_state[1] = -1;
				}
			}
			if ((IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 2) || (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 6)) {
				if (mode != 2) {
					mode = 2;
					alt_alarm_stop(&tlc_timer);
					proc_state[2] = -1;
				}
			}
			if ((IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 3) || (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 7)) {
				if (mode != 3) {
					mode = 3;
					alt_alarm_stop(&tlc_timer);
					proc_state[3] = -1;
				}
			}
		}

    	if(lcd != NULL) {
    		#define ESC 27
    		#define CLEAR_LCD_STRING "[2J"
    		fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
    		fprintf(lcd, "Mode: %d\n",mode);
    		fprintf(lcd, "State: %d\n",proc_state[mode]);
    	}

		// Execute the correct TLC
    	switch (mode) {
			case 0:
				simple_tlc(&proc_state[0]);
				break;
			case 1:
				pedestrian_tlc(&proc_state[1]);
				break;
			case 2:
				configurable_tlc(&proc_state[2]);
				break;
			case 3:
				camera_tlc(&proc_state[3]);
				break;
		}
	}
	fclose(lcd);
	return 1;
}
