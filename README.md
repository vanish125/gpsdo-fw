## Alternative firmware for the BH3SAP GPSDO

This is an alternative firmware for the BH3SAP GPSDO sold on various platforms.

![Image of the GPSDO running this firmware](https://github.com/fredzo/gpsdo-fw/blob/main/doc/gpsdo.jpg?raw=true)

### Theory of operation

Most of this is based on reverse engineering the circuit, and which pins certain things were connected to. The base of the device is a Bluepill board with a STM32F103C8T6 MCU. The oscillator is an Isotemp 143-141 OCXO which can be trimmed with a DC voltage. The output of the oscillator is buffered by a OPA692 and has an output resistor of 51 Ohms. The GPS differs between models, some have an ATGM336H and others a u-blox Neo-6M (perhaps not authentic). The 1PPS output from the GPS is not buffered, so it has very high output impedande.

* The OCXO is connected to the OSC_IN on the Bluepill board. The MCU is configured to be driven by an oscillator, so the crystal mounted on the Bluepill does nothing.
* The PPS is connected to the CH1 input on TIM1.
* The VCO is controlled via PWM from CH2 of TIM1. The PWM signal is sent to a couple of low pass filters, giving a DC voltage.

The MCU will PLL the clock up to 70MHz and then TIM1 is setup to count the internal 70MHz clock, while being gated by the PPS pulse from the GPS module. This means that it continually counts how many cycles on the clock passes between each PPS pulse. This is then used to adjust the VCO.

The VCO is simply adjusted by the error detected between two pulses. If 70000001 clocks are counted, the VCO voltage will drop a bit and so on. This is really simple, but due to the small adjustments, it will average out over time and it should work out since the counter is always running. If we are running at 70 000 000.01 we will get one more clock every 100 seconds, which will then cause a small adjustment (smallest adjustment possible).

It's fairly slow to reach a steady state, and it can probably easily be sped up with a better algorithm.

The displayed PPB error is a long running average. It counts the number of clock ticks over 128 seconds and compares it to the expected (128*SYSCLK)

### Usage

Power on the device with GPS antenna connected. Wait a long while for the PPB to reach close to zero. The used PWM value can then be stored in flash by going to `PWM` menu and press the encoder twice (a message will be shown after the first press).

This PWM will then be used on the next boot, and if no GPS antenna is connected, it will not be adjusted further.

The [original manual](https://raw.githubusercontent.com/fredzo/gpsdo-fw/b1f1766ef8beb795172a6fa325e783569361913e/doc/gpsdo-documentation.pdf) for the device talks about running the device without a GPS antenna after calibration, but I would advice against that since the oscillator seems sensitive to both ambient temperature, vibrations and orientation. Best results will be had when the GPS antenna is connected at all times.

### Menu system

This alternative firmware has a 2 level menu system. Movinf from one menu item to another is done by turning the rotary encoder, and entering a givem menu (when applicable) is done by pressing the encoder.

Here is the menu tree :
- Main Screen: displays the number of detected satellites, the PPB value and the current UTC time read from GPS frame
- PPB Screen: displays current PPB value
  - Mean value: the mean PPB value (running average over 128 seconds)
  - Instant value: last calculated PPB value
  - Frequency: the measured current MCU frequency (based on the number ot ticks counted between two GPS PPS pulses, should be around 70 000 000 for 70 MHz)
  - Error: the last measured frequency error (in Hz)
  - Correction: the last correction applied to PWM value
  - Millis: the gap in milliseconds between GPS PPS reference and MCU calculated PPS (should be 0)
- PWM Screen: the current PWM value, press the encoder twice to save this value to flash memory
- GPS Screen: displays the number of detected satellites and the current GPS time
  - Time: the current GPS time
  - Latitude: the GPS detected latitude (with form ddmm(.)mmmm)
  - Longitude: the GPS detected longitude (with form ddmm(.)mmmm)
  - Altitude: the GPS detected altitude (in meters)
  - Geoid: the Geoid-to-ellipsoid separation (in meters)
  - Sat. #: the numner of satellites
  - HDOP: the current Horizontal Dilution of Precision value
- Uptime Screen : displays the number of seconds elapsed since last boot
- GGA Frames Screen: the number of GGA frames received from the GPS module since last boot
- Contrast Screen : press the encoder to change the contrast value by turning the rotary encoder ; press again to exit (when editing contrast value, `?` is displayed after contrast)
- Version Screen : shows the current firmware version

### Flashing the firmware

To flash this alternative firmware in your GPSDO you will need :
- A [ST-Link V2 dongle](https://github.com/fredzo/gpsdo-fw/blob/main/doc/st-link-v2.png?raw=true)
- The [STM32CubeProgrammer software](https://www.st.com/en/development-tools/stm32cubeprog.html).

First you need to open it to access the bluepill board inside it.

To do so, you only need to remove the 4 screews at the top left and right sides of the front and bacck pannels of your GPSDO:
![Open Case](https://github.com/fredzo/gpsdo-fw/blob/main/doc/open-case.jpg?raw=true)

You now have access to the bluepill board, but you need to bend the 4 pins of the programmation header to the top so that you can plug Dupont wires to that header:
![Bend pins](https://github.com/fredzo/gpsdo-fw/blob/main/doc/st-link-connection.jpg?raw=true)

You can now launch the [STM32CubeProgrammer software](https://www.st.com/en/development-tools/stm32cubeprog.html) click `Connect`, click `Open File`, select the `gpsdo.bin` file downloaded in the `Release` section and hit `Download` in STM32CubeProgrammer:
![Bend pins](https://github.com/fredzo/gpsdo-fw/blob/main/doc/stm32-cube-programmer.png?raw=true)


### Building and flashing

Clone the repo, update submodules and do the cmake. (Or just download a release) You should not need any other dependencies than arm-none-eabi-gcc. The bluepill can be flashed in multiple ways, check the documentation for it for information. Included is a openocd configuration for connecting to the device via SWD using a JLink adapter.

### Display

The display consists of multiple screens. The main one on bootup show this:

```
|10 1.12
18:27:55
```

Top left corner contains an indicator for PPS pulses. Next to that is the current number of satellites used by the GPS module. To the right of that is the current measured PPB error. The PPB error will show ">=10" if the error is larger than 9.99, this is only due to a lack of space on the display.

Bottom line is the current UTC time from GPS module.

Rotating the encoder will switch to additional screens showing:

1) Full estimated PPB error
2) Current PWM 
3) Uptime of the device, in seconds

### USB

It would be nice to have NMEA output over USB, and the Bluepill dev board in the GPSDO does have a USB connector. It's however difficult to use since it requires a PLLCLK of 48MHz. But since we use 10MHz as input instead of 8MHz this can't be achieved. It should be possible to run the HSI to the PLL and then run the USB off of that. Then run the HSE directly to the peripherals. But then the timers would be running at 10MHz and that would cause the PWM to be slower, and the measurements to have lower resolution.

### GPS passthrough

Instead of using the USB, I have added GPS passthrough on an unused UART. Pins PA2 (TX) and PA3 (RX) can be used to communicate with the GPS module. This is bidirectional, so the GPS can be used by a computer or configured via manufacturer software.

I ended up routing these two pins and ground to an external header on the backside of the device, and then plugging in a serial to USB converter when needed.
