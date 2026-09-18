#include "max7221.h"
#include <string.h>
#include <stdio.h>

void max7221_init(max7221_chain_t *c, int n, max7221_frame_cb on_frame,
                  max7221_event_cb on_event, void *param) {
    memset(c, 0, sizeof *c);
    c->n = n > MAX7221_MAX_DEVICES ? MAX7221_MAX_DEVICES : n;
    c->on_frame = on_frame; c->on_event = on_event; c->param = param;
    /* Power-on state per datasheet: registers reset, display blanked, device in shutdown,
     * scan limit 0 (digit 0 only), intensity minimum, no decode, no display test. */
    for (int i = 0; i < c->n; i++) {
        c->dev[i].shutdown = 0; c->dev[i].scan_limit = 0; c->dev[i].intensity = 0;
    }
}

const char *max7221_reg_name(uint8_t reg) {
    static const char *names[16] = {
        "NOOP", "DIGIT0", "DIGIT1", "DIGIT2", "DIGIT3", "DIGIT4", "DIGIT5", "DIGIT6", "DIGIT7",
        "DECODE_MODE", "INTENSITY", "SCAN_LIMIT", "SHUTDOWN", "UNDEF_0D", "UNDEF_0E", "DISPLAY_TEST"};
    return names[reg & 0x0F];
}

static void apply(max7221_chain_t *c, int i, uint8_t reg, uint8_t value) {
    max7221_dev_t *d = &c->dev[i];
    switch (reg) {
    case MAX7221_REG_NOOP: break;
    case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7: case 0x8:
        d->digit[reg - 1] = value; break;
    case MAX7221_REG_DECODE:      d->decode = value; break;
    case MAX7221_REG_INTENSITY:   d->intensity = value & 0x0F; break;
    case MAX7221_REG_SCANLIMIT:   d->scan_limit = value & 0x07; break;
    case MAX7221_REG_SHUTDOWN:    d->shutdown = value & 0x01; break;
    case MAX7221_REG_DISPLAYTEST: d->display_test = value & 0x01; break;
    default: {
        char buf[64];
        snprintf(buf, sizeof buf, "dev%d reg 0x%02x value 0x%02x", i, reg, value);
        if (c->on_event) c->on_event(c->param, "undefined_register", buf);
        break;
    }
    }
}

void max7221_set_data(max7221_chain_t *c, int level) { c->data = level ? 1 : 0; }

void max7221_set_clk(max7221_chain_t *c, int level) {
    int rising = level && !c->clk;
    c->clk = level ? 1 : 0;
    if (!rising || c->load) return;               /* MAX7221: CS must be low to clock */
    int carry = c->data;
    for (int i = 0; i < c->n; i++) {
        int out = (c->dev[i].sr >> 15) & 1;
        c->dev[i].sr = (uint16_t)((c->dev[i].sr << 1) | carry);
        carry = out;
    }
    c->bits_since_load++;
}

void max7221_set_load(max7221_chain_t *c, int level) {
    int rising = level && !c->load;
    c->load = level ? 1 : 0;
    if (!rising) return;
    if (c->bits_since_load == 0) { c->empty_latches++; return; }
    c->latches++;
    if (c->bits_since_load != 16 * c->n) {
        char buf[64];
        snprintf(buf, sizeof buf, "%d bits clocked before LOAD (expected %d)", c->bits_since_load, 16 * c->n);
        c->malformed++;
        if (c->on_event) c->on_event(c->param, "malformed_burst", buf);
    }
    /* Device 0 is nearest the MCU and holds the last 16 bits clocked; device n-1 holds the
     * first 16 bits. Report far device first so records follow the order the bytes were sent. */
    for (int i = c->n - 1; i >= 0; i--) {
        uint16_t raw = c->dev[i].sr;
        uint8_t reg = (raw >> 8) & 0x0F, value = raw & 0xFF;
        apply(c, i, reg, value);
        c->frames++;
        if (c->on_frame) c->on_frame(c->param, i, reg, value, raw);
    }
    c->bits_since_load = 0;
}

/* Code-B font, MAX7221 datasheet table 5. Index 0-9, '-', E, H, L, P, blank. */
static const uint8_t codeb[16] = {
    0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70, 0x7F, 0x7B, 0x01, 0x4F, 0x37, 0x0E, 0x67, 0x00};

uint8_t max7221_visible_digit(const max7221_dev_t *d, int g) {
    if (d->display_test) return 0xFF;
    if (!d->shutdown) return 0x00;                /* register 0 = shutdown mode: blank */
    if (g > d->scan_limit) return 0x00;           /* not scanned */
    uint8_t v = d->digit[g];
    if (d->decode & (1 << g)) return (uint8_t)(codeb[v & 0x0F] | (v & 0x80));
    return v;
}
