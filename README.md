Fork of EBiCS firmware for Lishui devices. Ported to Xaiomi M365 controller. 
Use JST PA series 2mm pitch for the connectors. (need to be confirmed) 


Note:  This is WiP, use at your own risk. 
Note:  Makes no attempt to implement M365 protocols between BMS/BLE controller.

Send @56000BAUD via UART3 commands of 12 Bytes (Ant+LEV syntax)

run autodetect routine for proper hallsignal processing once first, keep the wheel in the air for this procedure:

AA 00 00 06 01 00 00 00 00 00 00 AD

to stop:  
AA 00 00 06 00 00 00 00 00 00 00 AC  

to set the setpoint for the motor current:

AA 00 00 06 00 00 00 LL MM 00 00 CC

LL is the LSB and MM the MSB of the 16bit signed integer. Values from -2096 to +2096 are senseful.

CC is the checksum, the XOR of bytes 0 to 11, see the online compiler example:
https://onlinegdb.com/VIHGUEOwa

Negative values give negative torque, means breaking / reverse motor spinning direction.

All system parameters have to be set edited in the config.h at the moment.
A serial protocol to set the parameters at runtime is in development.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

# Direct control by throttle on PA7
you can remove the 22k R21 resistor from the PCB and solder a wire to the pad to connect an analogue throttle.
To activate this option uncomment the line //#define ADCTHROTTLE in the config.h

![wiring diagram](https://github.com/Koxx3/SmartESC_STM32_v3/blob/master/Documentation/analog%20throttle%20input.jpg)

# Remote control from ESP32
You can use analogue throttle and brake and an ESP32 to remote control the SmartESC :
https://github.com/Koxx3/SmartESC_ESP32_serial_control/tree/SmarESC_V3

![wiring diagram](https://www.pedelecforum.de/forum/index.php?attachments/1611936761066-png.364172/)
