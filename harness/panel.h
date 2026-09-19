/* Magic Panel geometry: two MAX7221s, each driving 4 rows x 8 columns through one nibble of
 * each digit register (docs/firmware-map.md section 2.6). Renders the chain into an 8x8 grid
 * plus per-device status, and formats display records / filmstrip frames. */
#ifndef PANEL_H
#define PANEL_H
#include <stdint.h>
#include <stdio.h>
#include "parts/max7221.h"

#define PANEL_DEVICES 2
#define PANEL_ROWS 8

typedef struct panel_state {
    uint8_t rows[PANEL_ROWS];          /* as a viewer sees it: rows[0] = top row, bit 7 = LEFT column (A-1) */
    uint8_t intensity[PANEL_DEVICES];
    uint8_t shutdown[PANEL_DEVICES];   /* 1 = device IS shut down (register value 0) */
    uint8_t scan_limit[PANEL_DEVICES];
    uint8_t display_test[PANEL_DEVICES];
    uint8_t decode[PANEL_DEVICES];
    uint8_t orphan_bits;               /* set bits in the nibble no LED is wired to */
} panel_state_t;

void panel_render(const max7221_chain_t *c, panel_state_t *out);
int  panel_state_equal(const panel_state_t *a, const panel_state_t *b);
/* JSON object body (without cycle/ns), e.g. "grid":[...],"intensity":[..],... */
void panel_state_json(const panel_state_t *s, FILE *f);
/* Human filmstrip frame: header line + 8 rows of '#'/'.'. */
void panel_state_filmstrip(const panel_state_t *s, unsigned long seq, unsigned long long cycle,
                           unsigned long long prev_cycle, FILE *f);
#endif
