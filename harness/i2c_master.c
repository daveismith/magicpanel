#include "i2c_master.h"
#include "avr_twi.h"
#include "sim_cycle_timers.h"
#include <stdlib.h>
#include <string.h>

enum { ST_IDLE, ST_WAIT_ADDR_ACK, ST_WAIT_DATA_ACK, ST_WAIT_READ_BYTE, ST_WAIT_READ_FLAG,
       ST_WAIT_STOP_PROCESSED, ST_WAIT_STOP_ACK, ST_WAIT_NACK_PROCESSED, ST_WAIT_NACK_ACK };

#define R_TWAR 0xBA
#define R_TWCR 0xBC
#define R_TWAMR 0xBD

static void start_next(i2c_master_t *m);
static avr_cycle_count_t timeout(struct avr_t *avr, avr_cycle_count_t when, void *param);
static avr_cycle_count_t kick(struct avr_t *avr, avr_cycle_count_t when, void *param);

static void finish(i2c_master_t *m) {
    i2c_txn_t *t = m->cur;
    avr_cycle_timer_cancel(m->avr, timeout, m);
    t->done = 1; t->end = m->avr->cycle;
    m->cur = NULL; m->state = ST_IDLE; m->completed++;
    if (m->on_done) m->on_done(m->param, t);
    /* pop */
    m->head = t->next; if (!m->head) m->tail = NULL;
    free(t);
    if (m->head) avr_cycle_timer_register(m->avr, 4, kick, m);
}

static avr_cycle_count_t timeout(struct avr_t *avr, avr_cycle_count_t when, void *param) {
    i2c_master_t *m = param;
    if (!m->cur) return 0;
    if (m->state == ST_WAIT_ADDR_ACK) m->cur->nack_timeout = 1;   /* nobody answered: NACK */
    else m->cur->stalled = 1;                                     /* slave went quiet mid-transaction */
    finish(m);
    return 0;
}

static void arm(i2c_master_t *m, avr_cycle_count_t cycles) {
    avr_cycle_timer_register(m->avr, cycles, timeout, m);
}

static void send(i2c_master_t *m, uint8_t msg, uint8_t data) {
    avr_raise_irq(m->irq + TWI_IRQ_INPUT, avr_twi_irq_msg(msg, m->cur->addr, data));
    arm(m, m->stall_timeout);
}

/* Read path: the slave's byte arrives either combined with the address ack (first byte,
 * msg ADDR|READ|ACK?) or as a bare READ|ACK message followed by an ADDR message whose ACK
 * flag mirrors the slave's TWEA (set = slave has more data). A real master NACKs the byte the
 * slave flagged as last; any further bytes would read as 0xFF on a released bus. */
static void read_decide(i2c_master_t *m, uint8_t byte, int slave_has_more) {
    i2c_txn_t *t = m->cur;
    if (m->idx < t->n && m->idx < I2C_MAX_BYTES) t->data[m->idx] = byte;
    m->idx++; t->acked_bytes++;
    if (m->idx < t->n && slave_has_more) {
        m->state = ST_WAIT_READ_BYTE;
        send(m, TWI_COND_READ | TWI_COND_ACK, 1);
        return;
    }
    if (m->idx < t->n) {
        t->short_read = 1;
        for (int i = m->idx; i < t->n && i < I2C_MAX_BYTES; i++) t->data[i] = 0xFF;
    }
    m->state = ST_WAIT_NACK_PROCESSED;
    send(m, TWI_COND_READ, 0);          /* NACK: slave sees TW_ST_DATA_NACK (0xC0) and re-arms */
}

static void write_next_or_stop(i2c_master_t *m) {
    i2c_txn_t *t = m->cur;
    if (m->idx < t->n) {
        m->state = ST_WAIT_DATA_ACK;
        send(m, TWI_COND_WRITE, t->data[m->idx++]);
    } else {
        m->state = ST_WAIT_STOP_PROCESSED;
        send(m, TWI_COND_STOP, 0);      /* the slave reports TWSR 0xA0 then re-arms (ADDR|ACK) */
    }
}

static void on_output(struct avr_irq_t *irq, uint32_t value, void *param) {
    i2c_master_t *m = param;
    if (!m->cur) return;
    avr_twi_msg_irq_t v; v.u.v = value;
    uint8_t msg = v.u.twi.msg;
    switch (m->state) {
    case ST_WAIT_ADDR_ACK:
        if (!(msg & TWI_COND_ADDR)) break;
        avr_cycle_timer_cancel(m->avr, timeout, m);
        if (m->cur->is_read) {
            m->cur->ack_addr = 1;                  /* the slave answered at all */
            if (msg & TWI_COND_READ) read_decide(m, v.u.twi.data, (msg & TWI_COND_ACK) != 0);
            else { m->state = ST_WAIT_READ_BYTE; arm(m, m->stall_timeout); }
        } else {
            m->cur->ack_addr = (msg & TWI_COND_ACK) ? 1 : 0;
            if (!m->cur->ack_addr) { finish(m); return; }
            write_next_or_stop(m);
        }
        break;
    case ST_WAIT_DATA_ACK:
        if (msg == (TWI_COND_ADDR | TWI_COND_ACK)) {
            m->cur->acked_bytes++;
            write_next_or_stop(m);
        } else if (msg & TWI_COND_ADDR) {          /* NACKed data byte */
            m->state = ST_WAIT_STOP_PROCESSED;
            send(m, TWI_COND_STOP, 0);
        }
        break;
    case ST_WAIT_READ_BYTE:
        if (msg & TWI_COND_READ) { m->pending_byte = v.u.twi.data; m->have_pending = 1; m->state = ST_WAIT_READ_FLAG; }
        break;
    case ST_WAIT_READ_FLAG:
        if (msg & TWI_COND_ADDR) {
            avr_cycle_timer_cancel(m->avr, timeout, m);
            m->have_pending = 0;
            read_decide(m, (uint8_t)m->pending_byte, (msg & TWI_COND_ACK) != 0);
        }
        break;
    case ST_WAIT_STOP_ACK:
    case ST_WAIT_NACK_ACK:
        if (msg & TWI_COND_ADDR) { avr_cycle_timer_cancel(m->avr, timeout, m); finish(m); }
        break;
    default: break;
    }
}

static void on_status(struct avr_irq_t *irq, uint32_t value, void *param) {
    i2c_master_t *m = param;
    if (!m->cur) return;
    if (m->cur->nstatus < I2C_MAX_STATUS) m->cur->status[m->cur->nstatus++] = (uint8_t)value;
    if (m->state == ST_WAIT_STOP_PROCESSED && value == 0xA0) m->state = ST_WAIT_STOP_ACK;
    if (m->state == ST_WAIT_NACK_PROCESSED && value == 0xC0) m->state = ST_WAIT_NACK_ACK;
}

/* Real slave hardware is silent unless the address matches TWAR under the TWAMR mask and the
 * TWI is enabled. simavr's slave model keeps stale state between transactions and would raise
 * a spurious data interrupt on a mismatched START, so the harness applies the filter itself. */
static int slave_would_answer(const i2c_master_t *m, uint8_t addr) {
    const avr_t *avr = m->avr;
    if (!(avr->data[R_TWCR] & (1 << 2))) return 0;               /* TWEN */
    uint8_t mask = (uint8_t)(~(avr->data[R_TWAMR] >> 1) & 0x7F);
    return (addr & mask) == ((avr->data[R_TWAR] >> 1) & mask);
}

static void start_next(i2c_master_t *m) {
    if (m->cur || !m->head) return;
    m->cur = m->head; m->idx = 0; m->have_pending = 0;
    m->cur->start = m->avr->cycle;
    if (!slave_would_answer(m, m->cur->addr)) {
        m->cur->filtered = 1; m->cur->nack_timeout = 1;
        finish(m);
        return;
    }
    m->state = ST_WAIT_ADDR_ACK;
    avr_raise_irq(m->irq + TWI_IRQ_INPUT, avr_twi_irq_msg(
        TWI_COND_START | TWI_COND_ADDR | (m->cur->is_read ? TWI_COND_READ : TWI_COND_WRITE), m->cur->addr, 0));
    arm(m, m->ack_timeout);
}

static avr_cycle_count_t kick(struct avr_t *avr, avr_cycle_count_t when, void *param) {
    start_next((i2c_master_t *)param);
    return 0;
}

void i2c_master_init(i2c_master_t *m, avr_t *avr, i2c_done_cb on_done, void *param) {
    memset(m, 0, sizeof *m);
    m->avr = avr; m->on_done = on_done; m->param = param;
    m->ack_timeout = 40000;     /* 2.5 ms at 16 MHz; a real ACK arrives within ~1500 cycles */
    m->stall_timeout = 1600000; /* 100 ms: covers slow ISR entry; never hangs a run */
    m->irq = avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT), on_output, m);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_STATUS), on_status, m);
}

int i2c_master_queue(i2c_master_t *m, uint8_t addr, int is_read, const uint8_t *data, int n) {
    i2c_txn_t *t = calloc(1, sizeof *t);
    t->addr = addr & 0x7F; t->is_read = is_read; t->n = n > I2C_MAX_BYTES ? I2C_MAX_BYTES : n;
    if (!is_read && data) memcpy(t->data, data, t->n);
    t->requested = m->avr->cycle; t->id = m->next_id++;
    if (m->tail) m->tail->next = t; else m->head = t;
    m->tail = t;
    if (!m->cur) avr_cycle_timer_register(m->avr, 1, kick, m);
    return t->id;
}

int i2c_master_busy(const i2c_master_t *m) { return m->cur != NULL || m->head != NULL; }
