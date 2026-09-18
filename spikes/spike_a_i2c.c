/* Spike A: act as an external I2C master on simavr's TWI bus, write one command byte to the
 * firmware's slave address (0x14), and prove receiveEvent ran by counting MAX7221 LOAD
 * (PD6) rising edges: cmd 20 (Cross) issues allOFF()+PrintGrid() = 32 latches. A write to
 * a wrong address must produce no ACK and no latches. */
#include "spike_common.h"

typedef struct {
    avr_t *avr;
    avr_irq_t *twi;            /* TWI irq base: +TWI_IRQ_INPUT / +TWI_IRQ_OUTPUT */
    uint8_t addr, byte;
    int step;                  /* 0 idle, 1 addr sent, 2 data sent, 3 stop sent */
    int acked, done, nack;
    unsigned long latches;
} master_t;

static void on_load(struct avr_irq_t *irq, uint32_t value, void *param) {
    master_t *m = param;
    if (value) m->latches++;
}

static avr_cycle_count_t master_step(struct avr_t *avr, avr_cycle_count_t when, void *param) {
    master_t *m = param;
    if (m->step == 1) {          /* address was ACKed: send the data byte */
        m->step = 2;
        avr_raise_irq(m->twi + TWI_IRQ_INPUT, avr_twi_irq_msg(TWI_COND_WRITE, m->addr, m->byte));
    } else if (m->step == 2) {   /* data byte ACKed: send STOP (no WRITE flag!) */
        m->step = 3;
        avr_raise_irq(m->twi + TWI_IRQ_INPUT, avr_twi_irq_msg(TWI_COND_STOP, m->addr, 0));
        m->done = 1;
    }
    return 0;
}

static void on_twi_out(struct avr_irq_t *irq, uint32_t value, void *param) {
    master_t *m = param;
    avr_twi_msg_irq_t v; v.u.v = value;
    printf("  [cycle %9llu] slave -> master: msg=0x%02x addr=0x%02x data=0x%02x\n",
           (unsigned long long)m->avr->cycle, v.u.twi.msg, v.u.twi.addr, v.u.twi.data);
    if (v.u.twi.msg == (TWI_COND_ADDR | TWI_COND_ACK)) {
        m->acked++;
        avr_cycle_timer_register(m->avr, 8, master_step, m);   /* act a few cycles later */
    }
}

static void on_twi_status(struct avr_irq_t *irq, uint32_t value, void *param) {
    master_t *m = param;
    printf("  [cycle %9llu] TWSR status = 0x%02x\n", (unsigned long long)m->avr->cycle, value);
}

static void master_write(master_t *m, uint8_t addr, uint8_t byte, avr_cycle_count_t budget) {
    m->addr = addr; m->byte = byte; m->step = 1; m->acked = 0; m->done = 0;
    unsigned long before = m->latches;
    printf("master: START addr=0x%02x W, byte=%u\n", addr, byte);
    avr_raise_irq(m->twi + TWI_IRQ_INPUT,
                  avr_twi_irq_msg(TWI_COND_START | TWI_COND_ADDR | TWI_COND_WRITE, addr, 0));
    spike_run_cycles(m->avr, budget);
    printf("master: acks=%d stop_sent=%d latches_before=%lu latches_after=%lu (+%lu)\n",
           m->acked, m->done, before, m->latches, m->latches - before);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s firmware.elf\n", argv[0]); return 1; }
    master_t m; memset(&m, 0, sizeof m);
    m.avr = spike_load(argv[1]);
    m.twi = avr_io_getirq(m.avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_irq_register_notify(avr_io_getirq(m.avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT), on_twi_out, &m);
    avr_irq_register_notify(avr_io_getirq(m.avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_STATUS), on_twi_status, &m);
    avr_irq_register_notify(avr_io_getirq(m.avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6), on_load, &m);

    spike_run_cycles(m.avr, MS(50));
    printf("after 50 ms boot: %lu LOAD edges (44 bursts + the ctor's initial CS-high), TWAR=0x%02x TWCR=0x%02x\n",
           m.latches, m.avr->data[0xBA], m.avr->data[0xBC]);

    printf("\n== test 1: wrong address 0x15 ==\n");
    unsigned long boot = m.latches;
    master_write(&m, 0x15, 20, MS(20));
    int ok1 = (m.acked == 0 && m.latches == boot);

    printf("\n== test 2: correct address 0x14, cmd 20 (Cross) ==\n");
    unsigned long before = m.latches;
    master_write(&m, 0x14, 20, MS(60));
    /* ADDR|ACK arrives after the address, after the byte, and once more after STOP */
    int ok2 = (m.acked >= 2 && m.done && m.latches - before == 32);

    printf("\nRESULT: wrong-address ignored: %s; command executed (32 latches): %s\n",
           ok1 ? "PASS" : "FAIL", ok2 ? "PASS" : "FAIL");
    return (ok1 && ok2) ? 0 : 1;
}
