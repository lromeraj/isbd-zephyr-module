# Iridium SBD Library for Zephyr OS
This library has been implemented in order to give support to the *Iridium 9602 SBD Transceiver* in Zephyr projects. 
This implementation should work too for modern transceivers like *9603*, and also for modems which share similar AT commands.

# Wiki
This _README_ is used for general information and overall project status, all the detailed documentation related to this library can be found in the [wiki of this repository](https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/wikis/home).

# Architecture
The architecture of this library is quite simple, but if you want to see in more detail how does it work under the hood please refer to [this section](https://glab.lromeraj.net/ucm/miot/tfm/iridium-sbd-library/-/wikis/Architecture) in the wiki.

# Cloning the repository

Remember to clone this repository using the `--recursive` flag to fill the directories of submodule dependencies automatically:

``` bash
git clone git@glab.lromeraj.net:ucm/miot/tfm/iridium-sbd-library.git --recursive
```

In case you have cloned it without `--recursive` flag, use the following command:
``` bash
git submodule update --init
```


# AT command status tables
The following table shows the current status of the library based on the different AT commands available in the Iridium 9602 SBD Transceiver:

| Status | Icon | Preview | Description |
|--|--|----|----|
| Planned | `:clock1:` | :clock1: Planned |  The command is not implemented yet, <br/> but it is planned to be implemented in a near future |
| WIP | `:pencil2:` | :pencil2: WIP | The command is not implemented yet, <br/> but it is being implemented | 
| Implemented | `:hammer_and_pick:` | :hammer_and_pick: Implemented | Command is implemented, but it was not tested yet |
| Tested | `:heavy_check_mark:` | :heavy_check_mark: Tested | Command is implemented and working as expected |

## Standard AT basic commands
| AT | Command | Status | Observations |
| -- | -- | --- | -- |
| AT | AT Test Command | :heavy_check_mark: Tested | For internal use only |
| A/ | Repeat Last Command | :clock1: Planned | |
| E | Echo | :heavy_check_mark: Tested | For internal use only |
| I | Identification | :pencil2: WIP | |
| Q | Quiet Mode | :heavy_check_mark: Tested | For internal use only |
| V | Verbose Mode | :heavy_check_mark: Tested | For internal use only |
| Z | Soft Reset | :clock1: Planned |  |
| &D | DTR Option | :heavy_check_mark: Tested |  |
| &F | Restore Factory Settings | :x: | |
| &K | Flow Control | :heavy_check_mark: Tested |  |
| &V | View Active and Stored Configuration | :x: | |
| &W | Store Active Configuration | :hammer_and_pick: Implemented | |
| &Y | Designate Default Reset Profile | :hammer_and_pick: Implemented | |
| %R | Display registers | :x: | |
| *F | Flush to EEPROM | :hammer_and_pick: Implemented | |
| *R | Radio Activity | :clock1: Planned |  |

## Proprietary AT extended commands
| AT | Command | Status | Observations |
| -- | -- | -- | -- |
| +CCLK | Real Time Clock | :heavy_check_mark: Tested |  |
| +CGMI | Manufacturer Identification | :clock1: Planned |  |
| +CGMM | Model Identification | :clock1: Planned |  |
| +CGMR | Revision | :pencil2: WIP | Implemented |
| +CGSN | Serial Number | :heavy_check_mark: Tested |  |
| +CIER | Indicator Event Reporting | :clock1: Planned |  | 
| +CRIS | Ring Indication Status | :clock1: Planned |  |
| +CSQ | Signal Quality | :heavy_check_mark: Tested |  |
| +CULK | Unlock | :x: |  |
| +GMI | Manufacturer identification | :x: |  |
| +GMM | Model identification | :clock1: Planned |  |
| +GMR | Revision | :clock1: Planned |  |
| +GSN | Serial number | :x: |  |
| +IPR | Fixed DTE rate | :x: | Baudrate is something that should not be changed in production, in some circumstances an adaptive baudrate could be implemented in order to avoid electrical noise (for example), but for now, this is not planned to be implemented |
| +IPR | Fixed DTE rate | :x: |  |
| +SBDWB | Write Binary Data to the ISU | :heavy_check_mark: Tested | |
| +SBDRB | Read Binary Data from the ISU | :heavy_check_mark: Tested | |
| +SBDWT | Write a Text Message to the Module | :heavy_check_mark: Tested | Pending improvements |
| +SBDRT | Write a Text Message to the Module | :heavy_check_mark: Tested | Pending improvements |
| +SBDI | Initiate an SBD Session | :pencil2: WIP | |
| +SBDDET | Detach | :clock1: Planned |  |
| +SBDDSC | Delivery Short Code | :clock1: Planned |  |
| +SBDIX | Initiate an SBD Session Extended | :heavy_check_mark: Tested |  |
| +SBDIXA | Initiate an SBD Session Extended | :heavy_check_mark: Tested |  |
| +SBDMTA | Mobile-Terminated Alert | :heavy_check_mark: Tested |  |
| +SBDREG | Network Registration | :hammer_and_pick: Implemented |  |
| +SBDAREG | Automatic Registration  | :clock1: Planned |  |
| +SBDD | Clear SBD Message Buffers | :heavy_check_mark: Tested | |
| +SBDC | Clear SBD MOMSN | :clock1: Planned | |
| +SBDS | Status | :clock1: Planned | |
| +SBDSX | Status Extended | :clock1: Planned | |
| +SBDTC | Transfer MO Buffer to MT Buffer | :heavy_check_mark: Tested |  |
| +SBDGW | Gateway | :clock1: Planned |  |
| +MSSTM | Request System Time | :clock1: Planned |  |

> **NOTE**: some AT commands can give different responses depending on the suffix used, for example, the suffix `=?` is usually used to test a command to retrieve the different parameter values supported. Not all possible suffixes for all possible commands are implemented. The "read" variant `?` is implemented for almost every command that supports it.

> **NOTE**: by the moment tested means (at least) that it works on a *NXP FDRM K64F* board using the *Iridium 9602 SBD Transceiver*, however it should work with other combinations of boards and modems.

## Example program output
``` txt
isu_get_revision    () OK; Revision: Call Processor Version: TA21004
Modem DSP Version: 1.7 svn: 2358
DBB Version: 0x0001 (ASIC)
RFA Version: 0x0007 (SRFA2)
NVM Version: KVS
Hardware Version: BOOT07d4/9602NrvA-D/04/RAW0d
BOOT Version: 2004 TD2-BLB960X-27 R4710
isu_set_mt_alert    () OK; MT Alert successfully enabled
isu_get_mt_alert    () OK; MT Alert: 1
isu_get_imei        () OK; IMEI: 300534063281170
isu_set_mo          () OK; 
isu_mo_to_mt        () OK; SBDTC: Outbound SBD Copied to Inbound SBD: size = 4
isu_get_mt          () OK; msg="MIoT", len=4, csum=0159
isu_get_ring_sts    () OK; Ring status: 0
isu_init_session    () OK; mo_sts=32, mo_msn=56, mt_sts=2, mt_msn=0, mt_length=0, mt_queued=0
isu_clear_buffer    () OK; Mobile buffers cleared
```
