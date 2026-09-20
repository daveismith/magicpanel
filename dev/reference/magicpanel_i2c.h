/*
 * magicpanel_i2c.h - Magic Panel I2C register interface, protocol v1.0
 *
 * Controller-side constants for talking to a Magic Panel running firmware v012.0 or later.
 * The protocol is described in docs/i2c-protocol.md; this header is its machine-readable part
 * and is also what the firmware test-suite reads, so the two cannot drift apart.
 *
 * Wire format summary:
 *   register write : [MP_REG(reg), d0, d1, ...]          (at most 8 data bytes)
 *   set pointer    : [MP_REG(reg)]                        (then read; STOP or repeated START)
 *   register read  : N bytes from the pointer, auto-increment (at most 32 per transaction)
 *   legacy command : [seq]   one byte, 0..55, only while MP_CFG_LEGACY is set
 * Multi-byte values are little-endian.
 */
#ifndef MAGICPANEL_I2C_H
#define MAGICPANEL_I2C_H

#define MP_I2C_ADDR            0x14    /* 7-bit address (20 decimal) */
#define MP_REG_BIT             0x80    /* set in byte 0 of every register access */
#define MP_REG(r)              (MP_REG_BIT | (r))
#define MP_MAX_WRITE_DATA      8       /* data bytes after the register byte */
#define MP_MAX_READ            32      /* bytes per read transaction */

/* ---- Identity (read-only) ---------------------------------------------------------------- */
#define MP_WHO_AM_I            0x00    /* 2 bytes: 'M' 'P' */
#define MP_WHO_AM_I_0          0x4D
#define MP_WHO_AM_I_1          0x50
#define MP_PROTO_MAJOR         0x02
#define MP_PROTO_MINOR         0x03
#define MP_FW_MAJOR            0x04
#define MP_FW_MINOR            0x05
#define MP_FW_PATCH            0x06
#define MP_SEQ_COUNT           0x07
#define MP_CAPS                0x08
#define MP_I2C_ADDR_REG        0x09

#define MP_PROTO_MAJOR_VALUE   1
#define MP_PROTO_MINOR_VALUE   0

#define MP_CAP_LEGACY          0x01
#define MP_CAP_REPEAT          0x02
#define MP_CAP_BRIGHTNESS      0x04
#define MP_CAP_NAMES           0x08
#define MP_CAP_EEPROM_CONFIG   0x10
#define MP_CAP_ORIENTATION     0x20

/* ---- Status (read-only; 16 bytes, one read) ---------------------------------------------- */
#define MP_STATUS              0x10    /* start of the status block */
#define MP_STATUS_LEN          16
#define MP_SEQ_ID              0x10
#define MP_SOURCE              0x11
#define MP_STATE               0x12
#define MP_SUB_SEQ             0x13
#define MP_ITERATION           0x14
#define MP_REPEAT              0x15
#define MP_ELAPSED_MS          0x16    /* u32 */
#define MP_REMAINING_S10       0x1A    /* u16, 10 ms units */
#define MP_RUN_COUNTER         0x1C
#define MP_GPIO_CODE           0x1D
#define MP_LAST_ERROR          0x1E
#define MP_ERROR_COUNT         0x1F

#define MP_SEQ_NONE            0xFF    /* MP_SEQ_ID: nothing has run since reset */
#define MP_SUB_SEQ_OFF         0xFE    /* MP_SUB_SEQ: random show is in its off interval */
#define MP_REMAINING_UNKNOWN   0xFFFF

#define MP_SOURCE_NONE         0
#define MP_SOURCE_I2C          1       /* register START */
#define MP_SOURCE_LEGACY       2       /* one-byte legacy command */
#define MP_SOURCE_GPIO         3       /* rotary switch / jumper, including at power-on */
#define MP_SOURCE_GPIO_RESUME  4       /* GPIO mode restarted after an I2C sequence ended */

#define MP_STATE_IDLE          0
#define MP_STATE_RUNNING       1
#define MP_STATE_COMPLETE      2
#define MP_STATE_STOPPED       3     /* other values reserved */

#define MP_ERR_NONE            0
#define MP_ERR_UNKNOWN_REG     1
#define MP_ERR_READ_ONLY       2
#define MP_ERR_BAD_LENGTH      3
#define MP_ERR_BAD_VALUE       4
#define MP_ERR_LEGACY_OFF      5
#define MP_ERR_BAD_MAGIC       6

/* ---- Control ----------------------------------------------------------------------------- */
#define MP_START               0x20    /* W: seq, [repeat=1, 0=forever], [end=0] */
#define MP_STOP                0x21    /* W: mode */
#define MP_BRIGHTNESS          0x22    /* RW: 0..15 */

#define MP_REPEAT_FOREVER      0
#define MP_END_DEFAULT         0       /* leave the panel as the sequence leaves it */
#define MP_END_BLANK           1       /* blank the panel when the run ends */
#define MP_STOP_BLANK          0
#define MP_STOP_FREEZE         1
#define MP_BRIGHTNESS_MAX      15

/* ---- Configuration (RW; CONFIG, DEFAULT_BRIGHTNESS, ORIENTATION persisted by MP_SAVE) ----- */
#define MP_CONFIG              0x30
#define MP_DEFAULT_BRIGHTNESS  0x31
#define MP_ORIENTATION         0x32    /* RW: MP_ORIENT_NORMAL or MP_ORIENT_ROTATE_180 */
#define MP_SAVE                0x3F    /* W: MP_SAVE_MAGIC or MP_FACTORY_MAGIC */

#define MP_CFG_LEGACY          0x01
#define MP_CFG_GPIO_ENABLE     0x02
#define MP_CFG_GPIO_RESUME     0x04
#define MP_CONFIG_DEFAULT      0x07
#define MP_DEFAULT_BRIGHTNESS_DEFAULT 15
#define MP_ORIENT_NORMAL       0       /* as firmware v010.5 */
#define MP_ORIENT_ROTATE_180   1       /* turned 180 degrees, for panels installed the other way up */
#define MP_SAVE_MAGIC          0xA5
#define MP_FACTORY_MAGIC       0x5A

/* ---- Catalogue --------------------------------------------------------------------------- */
#define MP_INFO_INDEX          0x40    /* RW: sequence id to describe */
#define MP_INFO_FLAGS          0x41
#define MP_INFO_LENGTH_MS      0x42    /* u32 */
#define MP_INFO_NAME           0x46    /* 16 bytes ASCII, NUL-padded */
#define MP_INFO_NAME_LEN       16
#define MP_INFO_RECORD_LEN     22      /* read from MP_INFO_INDEX: index, flags, length, name */

#define MP_INFO_LOOPS          0x01    /* runs until stopped */
#define MP_INFO_RANDOM         0x02    /* content depends on the PRNG */
#define MP_INFO_ENDS_LIT       0x04    /* panel is left on at the end */
#define MP_INFO_HOLD           0x08    /* a static image held for the whole length */
#define MP_INFO_VARIES         0x10    /* the length differs from run to run: treat it as typical */
#define MP_LENGTH_INDEFINITE   0xFFFFFFFFUL

#define MP_LEGACY_LAST           55      /* highest one-byte legacy command */
#define MP_SEQ_RANDOM_SHOW       56
#define MP_SEQ_RANDOM_SHOW_LONG  57

#endif /* MAGICPANEL_I2C_H */
