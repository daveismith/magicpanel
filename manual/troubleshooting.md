# Troubleshooting

**The panel stays dark after power-up.**
: That is normal with the rotary switch at 0 and no jumper: it waits for I2C commands. Pick
  another position to check the LEDs ([standalone](use/standalone.md)).

**The controller gets no ACK from address 0x14.**
: Check that ground is shared with the controller, that SDA and SCL aren't swapped, and that the
  bus has pull-ups ([wiring](connect/wiring.md#i2c-bus)). A 3.3 V controller needs a level
  shifter.

**Identity reads `00 00`.**
: The panel runs firmware v010.5 or older. Only one-byte commands work; [flash](install/flashing.md)
  this firmware for the rest.

**The panel stays lit after commands 1–4.**
: By design: those sequences leave the panel on, as in v010.5. Send `0` (*All off*), or start
  them with the register and `end = 1` to blank at the end.

**A random show stays dark.**
: *Random show long* (position 7) plays one pattern and then stays dark (legacy behaviour).
  *Random show* is dark for about a minute between patterns; frequent I2C polling restarts that
  pause ([details](use/i2c-quickstart.md#common-tasks)).

**The panel ignores one-byte commands.**
: Legacy commands may have been switched off (`CONFIG` bit 0). Read register `0x30`. To restore
  every setting to factory values, write `[0xBF, 0x5A]`.

**A fitted jumper does nothing.**
: The rotary/jumper inputs may have been switched off (`CONFIG` bit 1). See above.

**A command was ignored.**
: Read the status block: `LAST_ERROR` (byte 14) and `ERROR_COUNT` (byte 15) say why
  ([error codes](reference/i2c-protocol.md#62-status-0x100x1f)).

**Patterns are upside down.**
: Your panel is probably mounted the other way round. Set `CONFIG` bit 3 to rotate the picture
  180° ([how](connect/wiring.md#orientation)).
