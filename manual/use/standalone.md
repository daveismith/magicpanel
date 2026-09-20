# Standalone operation

Without a controller, the panel runs a show chosen by the rotary switch and jumpers
([wiring](../connect/wiring.md#rotary-switch-and-jumpers)). The show loops until the switch
moves.

--8<-- "manual/generated/standalone-modes.md"

- **Changing position** blanks the panel and starts the new show from the beginning.
- **Position 0** blanks the panel and stops the show.
- **An I2C command** takes over from the show. When the commanded sequence ends, the
  rotary/jumper show starts again from the beginning (this can be turned off, see
  [configuration](../reference/i2c-protocol.md#7-configuration-0x300x3f)).
- **A controller can switch the inputs off** entirely (`CONFIG.GPIO_ENABLE`), for example to
  stop a fitted jumper from starting a show.

!!! note "The random shows"
    *Random show* plays a random pattern about once a minute and is dark in between. A connected
    controller may poll the panel while it runs without interrupting it (firmware 0.12.1 and
    later).
    *Random show long* (position 7) plays **one** pattern and then stays dark: its long pause
    overflows, exactly as in firmware v010.5.
