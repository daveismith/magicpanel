# Flashing the firmware

The panel's ATmega328P runs at 16 MHz with the Arduino **Duemilanove / Diecimila (ATmega328)**
bootloader, so it is programmed like that board.

## 1. Get the firmware

Download `MagicPanel-vX.Y.Z.hex` from the release that matches these pages
([Downloads](downloads.md)) and check it against `SHA256SUMS`:

```sh
shasum -a 256 -c SHA256SUMS --ignore-missing
```

## 2. Connect a USB-serial adapter

The firmware is loaded through the panel's serial bootloader. Connect a 5 V USB-serial adapter
(FTDI or similar, with DTR for auto-reset) to the panel's serial programming header.

Match the header's labels to the adapter: ground to ground, the panel's RX to the adapter's TX
and its TX to the adapter's RX, and DTR to DTR. Check the 5 V and ground pins before you power
anything up. Nothing else needs to be connected to upload.

## 3. Upload

=== "avrdude"

    ```sh
    avrdude -p m328p -c arduino -b 57600 -P /dev/ttyUSB0 \
            -U flash:w:MagicPanel-vX.Y.Z.hex:i
    ```

    Use the adapter's port (`/dev/cu.usbserial-…` on macOS, `COM3` etc. on Windows).

=== "Arduino IDE, from source"

    1. Install the **LedControl** library, version 1.0.6.
    2. Open `MagicPanel.ino` from the repository at the release tag.
    3. *Tools → Board*: **Arduino Duemilanove or Diecimila**; *Processor*: **ATmega328P**.
    4. Choose the adapter's port and click **Upload**.

=== "Command line, from source"

    ```sh
    git clone --recursive https://github.com/daveismith/magicpanel && cd magicpanel
    git checkout vX.Y.Z
    make setup firmware          # pinned toolchain; writes build/firmware.hex
    ```

    Then upload `build/firmware.hex` with the avrdude command from the first tab.

!!! warning "Keep the bootloader"
    Always upload through the serial bootloader as above. Don't use *Upload Using Programmer* or
    *Burn Bootloader*, or program the chip with an ISP programmer: those erase or replace the
    bootloader, and the panel can then no longer be updated over serial.

## 4. Check it

After power-up the panel stays dark with the rotary switch at 0 and no jumper fitted, and
waits for commands. With a controller on the bus, read the identity registers: the panel
answers `4D 50` ("MP") followed by the protocol and firmware version (see the
[I2C quick start](../use/i2c-quickstart.md#check-the-connection)). Firmware v010.5 answers `00 00`.

Settings saved with earlier versions of this firmware survive a re-flash (they live in EEPROM).
A panel that never ran this firmware starts with factory settings.
