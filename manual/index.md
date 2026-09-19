# Magic Panel

The IA-PARTS **Magic Panel** is an 8×8 LED panel for droid domes. An ATmega328P drives the LEDs
through two MAX7221 chips and plays built-in light sequences, either on its own (chosen with the
rotary switch or a jumper) or on command from a droid controller over I2C.

--8<-- "manual/generated/version.md"

<div class="grid cards" markdown>

-   **[Flash the firmware](install/flashing.md)**

    ---

    Download a release and load it with avrdude or the Arduino IDE.

-   **[Wire it up](connect/wiring.md)**

    ---

    Power, the I2C bus, the rotary switch and jumpers.

-   **[Run it standalone](use/standalone.md)**

    ---

    Pick a looping show with the rotary switch; no controller needed.

-   **[Drive it over I2C](use/i2c-quickstart.md)**

    ---

    Start, stop and query sequences from Arduino, Raspberry Pi or an existing droid controller.

-   **[Browse the patterns](generated/patterns/index.md)**

    ---

    Every sequence, animated at real speed.

-   **[I2C protocol reference](reference/i2c-protocol.md)**

    ---

    Every register, for controller authors.

</div>

This firmware (v012; v011 is used by another Magic Panel firmware) is a compatible successor to
IA-PARTS Magic Panel FX v010.5: existing controllers that send one command byte keep working. See
[Upgrading from v010.5](upgrading.md).
