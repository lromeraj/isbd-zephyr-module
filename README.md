# Iridium SBD module for Zephyr OS
This library has been implemented in order to give support to the *Iridium 9602 SBD Transceiver* in Zephyr projects. 
This implementation should work too for modern transceivers like *9603*, and also for different Iridium modems which share similar AT commands.

# Wiki
This _README_ is used for general information and overall project status, all the detailed documentation related to this module can be found in the [wiki of this repository](https://github.com/lromeraj/isbd-zephyr-module/wiki).

> **IMPORTANT**: this repository is still on its early stages and **it's not ready for production use** (by the moment).

# Architecture
The architecture of this module is quite simple, but if you want to see in more detail how does it work under the hood please refer to [this section](https://github.com/lromeraj/isbd-zephyr-module/wiki/Architecture) in the wiki.

# Emulator
This repository also includes a [recently implemented Iridium SBD emulator](https://github.com/lromeraj/isbd-emu) for testing purposes. Take a look to the [testing wiki section](https://github.com/lromeraj/isbd-zephyr-module/wiki/Testing).

# Cloning the repository
Remember to clone this repository using the `--recursive` flag to fill the directories of submodule dependencies automatically:

``` bash
git clone https://github.com/lromeraj/isbd-zephyr-module.git --recursive
```

In case you have cloned it without `--recursive` flag, use the following command:
``` bash
git submodule update --init
```

For further information take a look to [the following wiki section](https://github.com/lromeraj/isbd-zephyr-module/wiki/Getting-started).