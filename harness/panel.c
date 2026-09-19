#include "panel.h"
#include <string.h>

static uint8_t rev8(uint8_t v) {
    v = (uint8_t)((v & 0xF0) >> 4 | (v & 0x0F) << 4);
    v = (uint8_t)((v & 0xCC) >> 2 | (v & 0x33) << 2);
    return (uint8_t)((v & 0xAA) >> 1 | (v & 0x55) << 1);
}

void panel_render(const max7221_chain_t *c, panel_state_t *out) {
    uint8_t reg[PANEL_ROWS] = {0};
    memset(out, 0, sizeof *out);
    for (int d = 0; d < PANEL_DEVICES && d < c->n; d++) {
        const max7221_dev_t *dev = &c->dev[d];
        out->intensity[d] = dev->intensity;
        out->shutdown[d] = dev->shutdown ? 0 : 1;
        out->scan_limit[d] = dev->scan_limit;
        out->display_test[d] = dev->display_test;
        out->decode[d] = dev->decode;
        for (int g = 0; g < 8; g++) {
            uint8_t v = max7221_visible_digit(dev, g);
            int row = 4 * d + g / 2;          /* register row, 0..7 */
            if ((g & 1) == 0) {               /* even digit: high nibble */
                reg[row] |= v & 0xF0;
                out->orphan_bits += __builtin_popcount(v & 0x0F);
            } else {                          /* odd digit: low nibble */
                reg[row] |= v & 0x0F;
                out->orphan_bits += __builtin_popcount(v & 0xF0);
            }
        }
    }
    /* The panel is mounted so that register row 0 is the BOTTOM row and register bit 7 the
     * RIGHTMOST column (A-1, confirmed on hardware 2026-09-19: v010.5's OneTest starts at the
     * bottom left). The rendered grid is what a viewer sees: row 0 top, leftmost column first.
     * Device 0 therefore drives the bottom four rows. */
    for (int r = 0; r < PANEL_ROWS; r++) out->rows[PANEL_ROWS - 1 - r] = rev8(reg[r]);
}

int panel_state_equal(const panel_state_t *a, const panel_state_t *b) {
    return memcmp(a, b, sizeof *a) == 0;
}

static void bits8(uint8_t v, char *s) {
    for (int i = 0; i < 8; i++) s[i] = (v & (0x80 >> i)) ? '1' : '0';
    s[8] = 0;
}

void panel_state_json(const panel_state_t *s, FILE *f) {
    char b[9];
    fputs("\"grid\":[", f);
    for (int r = 0; r < PANEL_ROWS; r++) { bits8(s->rows[r], b); fprintf(f, "%s\"%s\"", r ? "," : "", b); }
    fputs("],\"rows_hex\":[", f);
    for (int r = 0; r < PANEL_ROWS; r++) fprintf(f, "%s\"%02x\"", r ? "," : "", s->rows[r]);
    fprintf(f, "],\"intensity\":[%u,%u],\"shutdown\":[%s,%s],\"scan_limit\":[%u,%u],"
               "\"display_test\":[%s,%s],\"decode\":[%u,%u],\"orphan_bits\":%u",
            s->intensity[0], s->intensity[1],
            s->shutdown[0] ? "true" : "false", s->shutdown[1] ? "true" : "false",
            s->scan_limit[0], s->scan_limit[1],
            s->display_test[0] ? "true" : "false", s->display_test[1] ? "true" : "false",
            s->decode[0], s->decode[1], s->orphan_bits);
}

void panel_state_filmstrip(const panel_state_t *s, unsigned long seq, unsigned long long cycle,
                           unsigned long long prev_cycle, FILE *f) {
    fprintf(f, "#%05lu  cycle %llu  t=%.3f ms  (+%.3f ms)  intensity %u/%u%s%s%s%s",
            seq, cycle, cycle / 16000.0, (cycle - prev_cycle) / 16000.0,
            s->intensity[0], s->intensity[1],
            s->shutdown[0] ? "  dev0:SHUTDOWN" : "", s->shutdown[1] ? "  dev1:SHUTDOWN" : "",
            (s->display_test[0] || s->display_test[1]) ? "  DISPLAY_TEST" : "",
            s->orphan_bits ? "  ORPHAN_BITS" : "");
    if (s->scan_limit[0] != 7 || s->scan_limit[1] != 7) fprintf(f, "  scan_limit %u/%u", s->scan_limit[0], s->scan_limit[1]);
    if (s->decode[0] || s->decode[1]) fprintf(f, "  decode 0x%02x/0x%02x", s->decode[0], s->decode[1]);
    fputc('\n', f);
    for (int r = 0; r < PANEL_ROWS; r++) {
        char line[PANEL_ROWS + 1];
        for (int i = 0; i < 8; i++) line[i] = (s->rows[r] & (0x80 >> i)) ? '#' : '.';
        line[8] = 0;
        fprintf(f, "    %s\n", line);
    }
}
