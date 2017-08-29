# README #

### COMPSYS 303 Assignment 1 ###
* Group 3
* Submission Version
* All Keys, LEDs, and PuTTY input as specified in assignment specification

### How do I get set up? ###
1. Open Altera Quartus II
2. Go to Tools > Programmer, and open the .sof file
3. Program the .sof file onto a DE2 FPGA device
4. Go to Tools > Nios II Build Tools for Eclipse
5. Select your workspace, open or import both the software project and the bsp project
6. Build all, then right click the software project and select Run As > Nios II Hardware

### How do I change traffic light controller modes? ###
* Use switches 19 and 20 to select which mode to use
* A switch in the down position represents a 0, and a switch in the up position represents a 1
* 00 = Mode 0, 01 = Mode 1, 10 = Mode 2, 11 = Mode 3
* Note that the mode will only change in a safe (R, R) state
* The LCD screen will display the current state and current mode

### How do I use the modes? ###
* *Mode 0* is a basic traffic light controller, and does not respond to inputs
* *Mode 1* incorporates two buttons to simulate pedestrian crossings. 
* Key 0 represents the North/South pedestrian button, and Key 1 represents the East/West button
* Pressing either button will cause the pedestrian crossing LED to turn on when the corresponding green light is active
* *Mode 2* incorporates Mode 1 and a configurable input for time delays between phases
* If switch 18 is up then on each safe state (R, R) the user will need to enter time delay values on PuTTY as specified
* The 6 values are to be entered comma separated and terminated with a "\n" and then press ENTER
* If the input is valid then the traffic light controller will adopt the input values
* *Mode 3* incorporates a red light running camera into the project
* Use Key 2 to simulate a car entering and exiting the intersection
* If a car enters the intersection on a yellow light then the camera will be activated
* If the car does not leave the intersection within a specified time frame then the camera will take a picture
* If the car does leave the intersection in time then the time taken is recorded
* All data is outputted via PuTTY

### Who do I talk to? ###
* Savi Mohan
* Mark Yep
