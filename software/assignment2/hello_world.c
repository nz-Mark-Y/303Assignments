/*
 *
 */

#define AVI_VALUE 300
#define AEI_VALUE 800
#define PVARP_VALUE 50
#define VRP_VALUE 150
#define LRI_VALUE 950
#define URI_VALUE 900
#define DEBOUNCE_VALUE 200
#define SIGNAL_VALUE 5

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

static alt_alarm avi_timer;
static alt_alarm aei_timer;
static alt_alarm pvarp_timer;
static alt_alarm vrp_timer;
static alt_alarm lri_timer;
static alt_alarm uri_timer;
static alt_alarm debounce_timer;
static alt_alarm LED_timer;
static alt_alarm signal_timer;
static alt_alarm aei_signal_timer;

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
void initialise_aei_signal_timer();
alt_u32 avi_isr_function();
alt_u32 aei_isr_function();
alt_u32 pvarp_isr_function();
alt_u32 vrp_isr_function();
alt_u32 lri_isr_function();
alt_u32 uri_isr_function();
alt_u32 debounce_isr_function();
alt_u32 LED_isr_function();
alt_u32 signal_isr_function();
alt_u32 aei_signal_isr_function();


static int buttonValue = 1;
static volatile int vpace_LED_on = 0;
static volatile int apace_LED_on = 0;

int main() {
	printf("Hello from Nios II!\n");

	reset();
	init_buttons_pio();

	if (IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) == 0) {
		mode0();
	} else {
		mode1();
	}

	while(1) {
		tick();
		if (VPace == 1) {
			printf("vpace set\n");
			initialise_pvarp_timer();
			initialise_vrp_timer();
			initialise_aei_timer();
			initialise_lri_timer();
			initialise_uri_timer();
			vpace_LED_on = 1;
			initialise_LED_timer(&vpace_LED_on);
		}
		if (APace == 1) {
			printf("apace set\n");
			initialise_avi_timer();
			apace_LED_on = 1;
			initialise_LED_timer(&apace_LED_on);
		}

		if (vpace_LED_on == 1) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x01);
		} else if (apace_LED_on == 1) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x02);
		} else {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, 0x00);
		}
	}

	return 0;
}

void mode0() {

}

void mode1() {

}

void init_buttons_pio() {
	void* context_going_to_be_passed = (void*) &buttonValue; 								// Cast before passing to ISR
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0); 										// Clears the edge capture register
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7); 									// Enable interrupts for all buttons
	alt_irq_register(BUTTONS_IRQ, context_going_to_be_passed, buttons_isr); 				// Register the ISR
}

void buttons_isr(void* context) {
	int* temp = (int*) context;
	(*temp) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);										// Clear the edge capture register
	if ((*temp & 0x01) > 0) {
		printf("vsense pressed\n");
		initialise_pvarp_timer();
		initialise_vrp_timer();
		initialise_aei_timer();
		initialise_lri_timer();
		initialise_uri_timer();
		VSense = 1;																	// VSense button is pressed
		initialise_debounce_timer(&VSense);
	}
	if ((*temp & 0x02) > 0) {
		printf("asense pressed\n");
		initialise_avi_timer();
		ASense = 1;																	// ASense pedestrian button is pressed
		initialise_debounce_timer(&ASense);
	}
}

void initialise_avi_timer() {
	printf("avi timer start\n");
	int avi_context = 0;
	void* avi_timer_context = (void*) &avi_context;
	alt_alarm_start(&avi_timer, AVI_VALUE, avi_isr_function, avi_timer_context);
}

void initialise_aei_timer() {
	printf("aei timer start\n");
	int aei_context = 0;
	void* aei_timer_context = (void*) &aei_context;
	alt_alarm_start(&aei_timer, AEI_VALUE, aei_isr_function, aei_timer_context);
}

void initialise_pvarp_timer() {
	printf("pvarp timer start\n");
	int pvarp_context = 0;
	void* pvarp_timer_context = (void*) &pvarp_context;
	alt_alarm_start(&pvarp_timer, PVARP_VALUE, pvarp_isr_function, pvarp_timer_context);
}

void initialise_vrp_timer() {
	printf("vrp timer start\n");
	int vrp_context = 0;
	void* vrp_timer_context = (void*) &vrp_context;
	alt_alarm_start(&vrp_timer, VRP_VALUE, vrp_isr_function, vrp_timer_context);
}

void initialise_lri_timer() {
	printf("lri timer start\n");
	int lri_context = 0;
	void* lri_timer_context = (void*) &lri_context;
	alt_alarm_start(&lri_timer, LRI_VALUE, lri_isr_function, lri_timer_context);
}

void initialise_uri_timer() {
	printf("uri timer start\n");
	int uri_context = 0;
	void* uri_timer_context = (void*) &uri_context;
	alt_alarm_start(&uri_timer, URI_VALUE, uri_isr_function, uri_timer_context);
}

void initialise_debounce_timer(int* context) {
	printf("debounce timer start\n");
	void* debounce_timer_context = (void*) context;
	alt_alarm_start(&debounce_timer, DEBOUNCE_VALUE, debounce_isr_function, debounce_timer_context);
}

void initialise_LED_timer(int* context) {
	printf("led timer start\n");
	void* LED_timer_context = (void*) context;
	alt_alarm_start(&LED_timer, DEBOUNCE_VALUE, LED_isr_function, LED_timer_context);
}

void initialise_signal_timer(char* context) {
	printf("signal timer start\n");
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&signal_timer, SIGNAL_VALUE, signal_isr_function, signal_timer_context);
}


void initialise_aei_signal_timer(char* context) {
	printf("signal timer start\n");
	void* signal_timer_context = (void*) context;
	alt_alarm_start(&aei_signal_timer, SIGNAL_VALUE, aei_signal_isr_function, signal_timer_context);
}


alt_u32 avi_isr_function(void* context) {
	printf("avi timer end\n");
	AVITO = 1;
	initialise_signal_timer(&AVITO);
	return 0;
}

alt_u32 aei_isr_function(void* context) {
	printf("aei timer end\n");
	AEITO = 1;
	initialise_aei_signal_timer(&AEITO);
	return 0;
}

alt_u32 pvarp_isr_function(void* context) {
	printf("pvarp timer end\n");
	PVARPTO = 1;
	initialise_signal_timer(&PVARPTO);
	return 0;
}

alt_u32 vrp_isr_function(void* context) {
	printf("vrp timer end\n");
	VRPTO = 1;
	initialise_signal_timer(&VRPTO);
	return 0;
}

alt_u32 lri_isr_function(void* context) {
	printf("lri timer end\n");
	LRITO = 1;
	initialise_signal_timer(&LRITO);
	return 0;
}

alt_u32 uri_isr_function(void* context) {
	printf("uri timer end\n");
	URITO = 1;
	initialise_signal_timer(&URITO);
	return 0;
}

alt_u32 debounce_isr_function(void* context) {
	printf("debounce timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 LED_isr_function(void* context) {
	printf("led timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 signal_isr_function(void* context) {
	printf("signal timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}

alt_u32 aei_signal_isr_function(void* context) {
	printf("aei signal timer end\n");
	int *temp = (int*) context;
	*temp = 0;
	return 0;
}
