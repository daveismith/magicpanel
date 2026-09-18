/* Spike B: reconstruct MAX7221 frames from raw pin edges (bit-banged shiftOut on
 * PB0=DATA, PD7=CLK, PD6=LOAD). Shift on CLK rising while LOAD low; on LOAD rising,
 * split the 32-bit burst: first 16 bits -> device 1 (far), last 16 -> device 0 (near). */
#include "spike_common.h"

static const char *regname(uint8_t r) {
    static const char *n[] = {"NOOP","DIG0","DIG1","DIG2","DIG3","DIG4","DIG5","DIG6","DIG7",
                              "DECODE","INTENSITY","SCANLIMIT","SHUTDOWN","?","?","DISPTEST"};
    return r < 16 ? n[r] : "?";
}

typedef struct {
    avr_t *avr;
    int data, clk, load;
    uint64_t sr; int nbits;
    unsigned long bursts, frames, badbursts, printed;
} dec_t;

static void emit(dec_t *d, int dev, uint16_t f) {
    d->frames++;
    if (d->printed < 60) {
        d->printed++;
        printf("cycle %9llu  %11.3f ms  dev%d  reg 0x%02x %-9s val 0x%02x\n",
                   (unsigned long long)d->avr->cycle, d->avr->cycle / 16000.0,
                   dev, f >> 8, regname((f >> 8) & 0x0f), f & 0xff);
    }
}

static void on_pin(struct avr_irq_t *irq, uint32_t value, void *param) {
    dec_t *d = param;
    if (irq == avr_io_getirq(d->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0)) { d->data = value; return; }
    if (irq == avr_io_getirq(d->avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7)) {
        int rising = value && !d->clk; d->clk = value;
        if (rising && !d->load) { d->sr = (d->sr << 1) | (d->data & 1); d->nbits++; }
        return;
    }
    if (irq == avr_io_getirq(d->avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6)) {
        int rising = value && !d->load; d->load = value;
        if (rising) {
            d->bursts++;
            if (d->nbits == 32) {
                emit(d, 1, (uint16_t)(d->sr >> 16));   /* first-shifted bits end up in the far chip */
                emit(d, 0, (uint16_t)(d->sr & 0xffff));
            } else if (d->nbits) {
                d->badbursts++;
                printf("cycle %llu: burst with %d bits (expected 32)\n",
                       (unsigned long long)d->avr->cycle, d->nbits);
            }
            d->sr = 0; d->nbits = 0;
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s firmware.elf\n", argv[0]); return 1; }
    dec_t d; memset(&d, 0, sizeof d);
    d.avr = spike_load(argv[1]);
    avr_irq_register_notify(avr_io_getirq(d.avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0), on_pin, &d);
    avr_irq_register_notify(avr_io_getirq(d.avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7), on_pin, &d);
    avr_irq_register_notify(avr_io_getirq(d.avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6), on_pin, &d);
    spike_run_cycles(d.avr, MS(50));
    printf("\n50 ms: %lu bursts, %lu frames, %lu malformed bursts\n", d.bursts, d.frames, d.badbursts);
    /* Expected boot: ctor per device DISPTEST,SCANLIMIT,DECODE,DIG0-7,SHUTDOWN (11) x2 = 22,
     * setup() SHUTDOWN x2, INTENSITY x2, DIG0-7 x2 = 20 -> 42 bursts... plus 2 more? see run */
    printf("RESULT: %s\n", (d.frames == 88 && d.badbursts == 0) ? "PASS" : "FAIL");
    return (d.frames == 88 && d.badbursts == 0) ? 0 : 1;
}
