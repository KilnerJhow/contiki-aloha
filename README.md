The Contiki Operating System - With Aloha RDC
============================

Contiki is an open source operating system that runs on tiny low-power
microcontrollers and makes it possible to develop applications that
make efficient use of the hardware while providing standardized
low-power wireless communication for a range of hardware platforms.

In this version of contiki, we have implemented the Pure Aloha protocol with RDC and analyzed the performance of the protocol in terms of energy consumption.

The source code for the Pure Aloha protocol with RDC can be found in the following directories:
```
contiki/core/net/mac/contikimac/
contiki/core/net/mac
```
where the files `contikimac-for-aloha-rdc.c` and `contikimac-for-aloha-rdc.h` contain the implementation of the Pure Aloha protocol with RDC and the files `aloha-rdc.c` and `aloha-rdc.h` contain the implementation of Aloha Protocol.

The source code for the performance analysis of the protocol can be found in the following directory:
```
tcc
```
where the file `confidence_interval.py` contains the code for calculating the confidence interval and the energy consumption.
