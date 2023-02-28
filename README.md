# Iridium SBD Library for Zephyr OS
This library has been implemented in order to give support for the Iridium 9602 SBD Transceiver, but it should work too for modern transceivers like `9603`, nad also for modems which share similar AT commands.

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
This a quite simple example but tries to show a basic concept for using this library. In general you should consider to check the status codes returned by the different functions, so in this case you could implement a function callback in order to receive resulting status codes, for example.



# AT Command Status Table
The following table shows the current status of the library based on the different AT commands available in the Iridium 9602 SBD Transceiver:

| AT Command | Implemented | Tested |
| -- | -- | -- |
| AT | :heavy_check_mark: | :heavy_check_mark: |


> **NOTE**: by the moment tested means (at least) that it works on a *NXP FDRM K64F* board using the Iridium 9602 SBD Transceiver, however it should work with other boards and modems.