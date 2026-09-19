# Flashing the firmware

The panel's ATmega328P runs at 16 MHz with the Arduino **Duemilanove / Diecimila (ATmega328)**
bootloader, so it is programmed like that board.

## 1. Get the firmware

Download `MagicPanel-vX.Y.Z.hex` from the release that matches these pages
([Downloads](downloads.md)) and check it against `SHA256SUMS`:

```sh
shasum -a 256 -c SHA256SUMS --ignore-missing
```

## 2. Connect a programmer

=== "USB-serial adapter (bootloader)"

    Connect a 5 V USB-serial adapter (FTDI or similar, with DTR for auto-reset) to the panel's
    serial programming header.

    TODO(owner): header location, pin order and a photo.

=== "ISP programmer"

    Connect a 5 V ISP programmer (USBasp, AVRISP mkII, an Arduino as ISP…) to the panel's 6-pin
    ISP header.

    TODO(owner): header location and orientation.

    !!! warning
        Writing flash over ISP erases the chip, **including the bootloader**. That is fine if you
        always use ISP; to keep serial uploads working, reflash the bootloader afterwards
        (Arduino IDE: *Tools → Burn Bootloader* with the Duemilanove/Diecimila board selected).

## 3. Upload

=== "avrdude, serial"

    ```sh
    avrdude -p m328p -c arduino -b 57600 -P /dev/ttyUSB0 \
            -U flash:w:MagicPanel-vX.Y.Z.hex:i
    ```

    Use the adapter's port (`/dev/cu.usbserial-…` on macOS, `COM3` etc. on Windows).

=== "avrdude, ISP"

    ```sh
    avrdude -p m328p -c usbasp -U flash:w:MagicPanel-vX.Y.Z.hex:i
    ```

    Replace `usbasp` with your programmer (`avrispmkII`, `stk500v1 -b 19200 -P …` for Arduino as ISP).

=== "Arduino IDE, from source"

    1. Install the **LedControl** library, version 1.0.6.
    2. Open `MagicPanel.ino` from the repository at the release tag.
    3. *Tools → Board*: **Arduino Duemilanove or Diecimila**; *Processor*: **ATmega328P**.
    4. Upload.

=== "Command line, from source"

    ```sh
    git clone --recursive https://github.com/daveismith/magicpanel && cd magicpanel
    git checkout vX.Y.Z
    make setup firmware          # pinned toolchain; the hex is written to build/
    ```

## 4. Check it

After power-up the panel stays dark with the rotary switch at 0 and no jumper fitted, and
waits for commands. With a controller on the bus, read the identity registers: the panel
answers `4D 50` ("MP") followed by the protocol and firmware version (see the
[I2C quick start](../use/i2c-quickstart.md#check-the-connection)). Firmware v010.5 answers `00 00`.

Settings saved with earlier versions of this firmware survive a re-flash (they live in EEPROM).
A panel that never ran this firmware starts with factory settings.
