# README #

### COMPSYS 303 Assignment 2 ###
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

### How do I change pacemaker modes? ###
* Use any switch to determine that pacemaker mode. If any switch is in the up position then mode 1 is selected. 
* If all switches are in the down position then mode 0 is selected
* Note that the mode will only change on reset, so after switching to a different mode press the reset (KEY 3) button.
* Note that our reset key no longer seems to be working, so please reprogram after using a dip switch to change the mode

### How do I use the modes? ###
* *Mode 0* is a pacemaker in which the VSense and ASense inputs are inputted using the buttons on the DE2 board.
* Press Key 0 to simulate a VSense, and press Key 1 to simulate an ASense.
* *Mode 1* incorporates a UART input from a virtual heart emulation on a computer. 
* Open the virtual heart program after the DE2 board is programmed and connected via serial interface.
* Enter baud rate and COM port, and the pacemaker will begin pacing the heart.
* All inputs and outputs are visible on the ECG screen, as per the specifications.

### Who do I talk to? ###
* Savi Mohan
* Mark Yep
