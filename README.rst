NCS-SPI-ArrayList-Example
#########################

Overview
********

This is a small example showing how to use the arraylist feature of the SPIM peripheral to sample the SPI at a fast rate based on a timer compare event. 
A separate timer module in counter mode is set up to count the number of transactions, and signal to the application when the buffer is either half full or full. 
In order to handle the situation where the buffer reset gets delayed the buffer has some margin at the end, allowing a certain amount of overflow. 
The user will need to check how many items are waiting in the buffer before processing them. 

Requirements
************

nRF Connect SDK v2.3.0

Tested on the following board(s):
- nrf52840dk_nrf52840

