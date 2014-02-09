vetinari-clock
==============

Erratic tick mechanism for hacked clock.

Original project by Bart Basile http://e2e.ti.com/group/microcontrollerprojects/m/msp430microcontrollerprojects/664670.aspx

Adapted by Tom Price February 2014:

* Target microcontroller MSP430G2231 (smaller and cheaper than MSP430G2553)
* Tick sequence length adjustable using single define
* Bit array used to store sequence to save memory
* Layout of H bridge adapted slightly
* Energise time adjusted for the particular clock that I happened to have
