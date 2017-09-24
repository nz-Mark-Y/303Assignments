/*
 * sccharts.h
 *
 *  Created on: 18/09/2017
 *      Author: myep112
 */

#ifndef SCCHARTS_H_
#define SCCHARTS_H_

// Functions
void reset();
void tick();

// Inputs
extern char VSense;
extern char ASense;
extern char AVITO;
extern char PVARPTO;
extern char LRITO;
extern char VRPTO;
extern char AEITO;
extern char URITO;
extern char URI_NOTRUNNING;

// Outputs
extern char VPace;
extern char APace;

#endif /* SCCHARTS_H_ */
