# Iridium SBD Library for Zephyr OS
This library has been implemented in order to give support for the Iridium 9602 SBD Transceiver, but it should work too for modern transceivers like 9603, nad also for modems which share similar AT commands.

At the moment this library has been implemented in order to work with blocking function calls, this is because the transceiver only processes one command at a time and implementing an asynchronous model out of the box may not be the best solution, but i am not saying that you should use this library to block all your main application processes, the idea for this library is to be used in a different thread in order to avoid blocking other important tasks of your application, as *Zephyr* and many other *RTOS* offer the possibility of "emulating" multitask processing on single core processor, this should be something trivial to implement, so what i recommend while using this library is:

1. Create an independent thread where you'll invoke the blocking functions of this library.
2. Create a queue in order to synchronize your main application logic with the library thread.

Below, you can see an example of a "Send and forgive" concept:
``` txt
      Main thread                 ISBD library thread 
           |                                | <------------ IDLE
           |     Send message "HOLA"        |
           |------------------------------> |
           |                                | OK. Sending message ...
           |                 
```
This is a quite simple example but tries to show a basic concept while using this library. In general you should consider to check the status codes returned by the different functions, so in this case you could implement a callback in order to receive resulting status codes.

# AT Command Status Tables
The following table shows the current status of the library based on the different AT commands available in the Iridium 9602 SBD Transceiver:

| Status | Icon | Preview | Description |
|--|--|----|----|
| Planned | `:clock1:` | :clock1: Planned |  The command is not implemented yet, <br/> but it is planned to be implemented in a near future |
| WIP | `:pencil2:` | :pencil2: WIP | The command is not implemented yet, <br/> but it is being implemented | 
| Implemented | `:hammer_and_pick:` | :hammer_and_pick: Implemented | Command is implemented, but it was not tested |
| Tested | `:heavy_check_mark:` | :heavy_check_mark: Tested | Command is implemented and working as expected |


## Standard AT commands
| AT | Command | Status | Observations |
| -- | -- | -- | -- |
| AT | AT Test Command | :heavy_check_mark: Tested | For internal use only |
| A/ | Repeat Last Command | :clock1: Planned | |
| E | Echo | :heavy_check_mark: Tested | For itnernal use only |
| I | Identification | :pencil2: WIP | |
| Q | Quiet Mode | :pencil2: Implemented | For internal use only |
| V | Verbose Mode | :heavy_check_mark: Tested | For internal use only |
| Z | Soft Reset | :pencil2: WIP |  |
| &D | DTR Option | :heavy_check_mark: Tested | For internal use only |
| &F | Restore Factory Settings | :x: | |
| &K | Flow Control | :heavy_check_mark: Tested | For internal use only |
| &V | View Active and Stored Configuration | :x: | |
| &W | Store Active Configuration | :pencil2: WIP | |
| &Y | Designate Default Reset Profile | :pencil2: WIP | |
| %R | Display registers | :x: | |
| *F | Flush to EEPROM | :pencil2: WIP | |
| *R | Radio Activity | :pencil2: WIP |  |

## Propietary AT commands
| AT | Command | Status | Observations |
| -- | -- | -- | -- |
| +CCLK | Real Time Clock | :hammer_and_pick: Implemented |  |
| +CGMI | Manufacturer Identification | :clock1: Planned |  |
| +CGMM | Model Identification | :clock1: Planned |  |
| +CGMR | Revision | :pencil2: WIP | Implemented |
| +CGSN | Serial Number | :heavy_check_mark: Tested |  |
| +CIER | Indicator Event Reporting | :clock1: Planned |  | 
| +CRIS | Ring Indication Status | :clock1: Planned |  |
| +CSQ | Signal Quality | :clock1: Planned |  |
| +CULK | Unlock | :x: |  |
| +GMI | Manufacturer identification | :x: |  |
| +GMM | Model identification | :clock1: Planned |  |
| +GMR | Revision | :clock1: Planned |  |
| +GSN | Serial number | :clock1: Planned |  |
| +IPR | Fixed DTE rate | :x: | Baudrate is something that should not be changed in production, in some circumstances an adaptive baudrate could be implemented in order to avoid electrical noise (for example), but for now, this is not planned to be implemented |
| +GSN | Serial number | :clock1: Planned |  |

> **NOTE**: some AT commands can give different responses depending on the suffix used, for example, the suffix `=?` is usually used to test a command to retrieve the different parameter values supported. Not all possible suffixes for all possible commands are implemented. The "read" variant `?` is implemented for almost every command that supports it.

> **NOTE**: by the moment tested means (at least) that it works on a *NXP FDRM K64F* board using the Iridium 9602 SBD Transceiver, however it should work with other combinations of boards and modems.