# BO DFU

BO DFU is a bitbanged implementation of USB Low Speed and DFU for the ESP32 bootloader.

Once a bootloader with this component is built and flashed to the ESP32, it is able to receive firmware updates over USB without any special hardware or software.

With a WebUSB-capable device, it's even possible to flash ESP32 firmware directly in the browser:
https://boarchuz.github.io/bo_dfu_web/

## Installation

Clone into or add as a submodule to your ESP-IDF project's `bootloader_components` directory:

```
git clone https://github.com/boarchuz/bo_dfu bootloader_components/bo_dfu
# OR
git submodule add https://github.com/boarchuz/bo_dfu bootloader_components/bo_dfu
```

## Hardware

At minimum, a single 1.5k resistor is all that's needed.

Best practice dictates adding 22Ω termination resistors and ESD protection to the data pins also.

```
---------------┐
               |    1.5kΩ      
         USB_EN|-----www----┐               
               |            |              ┌-----
               |     22Ω    |              |
ESP32    USB_D-|-----www----┴--------------|  H
               |                           |  O
               |     22Ω                   |  S
         USB_D+|-----www-------------------|  T
               |                           |
---------------┘                           └-----
                   (ESD not pictured)    
```

## Configuration

All configuration options are available via Kconfig (`idf.py menuconfig`).

 - **GPIOs**

    At least two GPIOs (D+ and D-) are required; a third GPIO (USB_EN above) is highly recommended to allow control of attaching and detaching from the bus.

    Carefully select the GPIOs to avoid complicating factors on startup: no default internal pullups or pulldowns, strapping pins, etc.

    GPIOs must be output capable, and D+/D- must be both <32 or both >=32.

    For quick reference, some recommended GPIOs are 18, 19, 21, 22, 23, 25, 26, 27, 32, 33.

 - **USB**

    USB VID, PID, manufacturer, device name, BCD and so on are also configurable.
    
    By default, the device reports as an "Espressif ESP32", the VID is set to Espressif (0x303a), the PID to "Espressif test PID" (0x8000) and the BCD and serial are set using the ESP32's hardware revision and MAC at runtime.

- **Misc**

    Some additional features are included to provide more control over the DFU process.
    
    A strapping pin can be defined, allowing DFU mode to be entered on startup only if a button is being held, for example. There are also options to blink an LED, hold a button to exit DFU mode, set timeouts, etc.

## Host Clients

As well as the [browser-based updater](https://boarchuz.github.io/bo_dfu_web/) mentioned above, there is also [dfu-util](https://dfu-util.sourceforge.net/):
```
# List attached DFU-capable devices:
dfu-util -l

# Download firmware:
dfu-util -D build/app.bin
```

## Other Stuff...

 - **Bootstrapping**
 
    Of course, ESP32 does not have built in USB DFU support. So, to be clear, a `bo_dfu`-enabled bootloader must be flashed to the device using other methods first (UART, JTAG), along with the partition table, etc.

 - **File Compatibility**
 
    This is designed to work directly with the `[project_name].bin` produced in your build directory. It is not necessary to append a DFU suffix to the file before downloading (in fact this is not tested and may break).

 - **Compatibility**
 
    Only ESP32 is supported (not any -Sx, -Cx, etc), and only those capable of 240MHz CPU frequency.

 - **USB Compliance**

    This is a minimal USB Low Speed (1.5MHz) implementation with the specific and limited goal of adding and USB DFU support to the bootloader. It is *mostly* compliant.

 - **Partitions**

    Downloaded firmware will be written to the next OTA partition, and the OTA data updated to activate this partition, as per the usual ESP-IDF OTA scheme.

- **Building**

    Depending on the configuration, this increases the bootloader binary size by approximately 0x1000 bytes. If your build fails due to bootloader size, you will need to increase CONFIG_PARTITION_TABLE_OFFSET.
    
    The bootloader build should be optimised for size (-Os), with logging reduced or disabled.

- **Safety**

    Anything that erases or writes to flash memory comes with the risk of bricking the device. Use at your own risk.

    No security features are supported, including flash encryption, Secure Boot, app rollback, etc.

    I have written this from scratch. It works surprisingly well but, as always, expect bugs.

- **Speed**

    For reference, a typical 1MB firmware file downloads in approximately 90 seconds.
