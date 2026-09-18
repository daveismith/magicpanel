/* Generic MAX7221/MAX7219 daisy-chain model driven by raw DATA/CLK/LOAD pin levels.
 *
 * Semantics (MAX7221 datasheet): with LOAD/CS low, DIN is sampled into a 16-bit shift
 * register on each CLK rising edge; DOUT is the bit shifted out the far end and feeds the next
 * device's DIN. On the LOAD rising edge every device latches its own 16 bits as
 * [x x x x A3 A2 A1 A0][D7..D0]. With CS high the MAX7221 ignores clocks. */
#ifndef MAX7221_H
#define MAX7221_H
#include <stdint.h>

#define MAX7221_MAX_DEVICES 8

enum {
    MAX7221_REG_NOOP = 0x0, MAX7221_REG_DIGIT0 = 0x1, MAX7221_REG_DIGIT7 = 0x8,
    MAX7221_REG_DECODE = 0x9, MAX7221_REG_INTENSITY = 0xA, MAX7221_REG_SCANLIMIT = 0xB,
    MAX7221_REG_SHUTDOWN = 0xC, MAX7221_REG_DISPLAYTEST = 0xF,
};

typedef struct max7221_dev {
    uint16_t sr;                 /* shift register */
    uint8_t  digit[8];           /* digit registers 0x1..0x8 */
    uint8_t  decode;             /* 0x9 decode-mode bitmask */
    uint8_t  intensity;          /* 0xA, 0..15 */
    uint8_t  scan_limit;         /* 0xB, 0..7 */
    uint8_t  shutdown;           /* 0xC register value: 0 = shutdown, 1 = normal operation */
    uint8_t  display_test;       /* 0xF, 1 = all LEDs on */
} max7221_dev_t;

/* Called for every 16-bit word latched into a device (no-ops included). */
typedef void (*max7221_frame_cb)(void *param, int dev, uint8_t reg, uint8_t value, uint16_t raw);
/* Diagnostics: malformed bursts, undefined registers. */
typedef void (*max7221_event_cb)(void *param, const char *kind, const char *detail);

typedef struct max7221_chain {
    int n;
    max7221_dev_t dev[MAX7221_MAX_DEVICES];
    int data, clk, load;         /* current pin levels */
    int bits_since_load;
    unsigned long latches, frames, malformed, empty_latches;
    max7221_frame_cb on_frame;
    max7221_event_cb on_event;
    void *param;
} max7221_chain_t;

void max7221_init(max7221_chain_t *c, int n, max7221_frame_cb on_frame,
                  max7221_event_cb on_event, void *param);
void max7221_set_data(max7221_chain_t *c, int level);
void max7221_set_clk(max7221_chain_t *c, int level);
void max7221_set_load(max7221_chain_t *c, int level);
const char *max7221_reg_name(uint8_t reg);
/* Segment pattern shown for digit g of device d, applying decode mode, scan limit, shutdown
 * and display test. Bit layout: D7=DP D6=A D5=B D4=C D3=D D2=E D1=F D0=G. */
uint8_t max7221_visible_digit(const max7221_dev_t *d, int g);

#endif
