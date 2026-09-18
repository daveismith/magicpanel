/* External I2C master on simavr's message-level TWI bus. Transactions are queued and run
 * back to back; each is a write (START, address, N data bytes, STOP) or a read (START,
 * address, N bytes with ACK/NACK; no STOP message is sent after reads because simavr would
 * deliver a spurious TW_SR_STOP to the slave firmware, which real hardware does not). */
#ifndef I2C_MASTER_H
#define I2C_MASTER_H
#include <stdint.h>
#include "sim_avr.h"
#include "sim_irq.h"

#define I2C_MAX_BYTES 64
#define I2C_MAX_STATUS 160

typedef struct i2c_txn {
    uint8_t addr;                 /* 7-bit */
    int is_read;
    uint8_t data[I2C_MAX_BYTES];  /* bytes to write, or bytes read back */
    int n;                        /* bytes requested */
    /* results */
    int ack_addr, acked_bytes, done, nack_timeout;
    int filtered;                 /* address did not match TWAR/TWAMR (or TWI disabled): never put on the bus */
    int short_read;               /* slave signalled last byte before n bytes; rest filled with 0xFF */
    int stalled;                  /* slave stopped responding mid-transaction */
    avr_cycle_count_t requested, start, end;
    uint8_t status[I2C_MAX_STATUS]; int nstatus;   /* TWSR codes observed */
    int id;
    struct i2c_txn *next;
} i2c_txn_t;

typedef void (*i2c_done_cb)(void *param, const i2c_txn_t *t);

typedef struct i2c_master {
    avr_t *avr;
    avr_irq_t *irq;               /* TWI irq base */
    i2c_txn_t *head, *tail, *cur;
    int state;                    /* internal */
    int idx;                      /* byte index within current txn */
    int next_id;
    avr_cycle_count_t ack_timeout;/* cycles to wait for an address ACK before declaring NACK */
    avr_cycle_count_t stall_timeout; /* cycles to wait for any other slave response */
    int pending_byte, have_pending;  /* read: byte received, waiting for the slave's more/last flag */
    i2c_done_cb on_done; void *param;
    unsigned long completed;
} i2c_master_t;

void i2c_master_init(i2c_master_t *m, avr_t *avr, i2c_done_cb on_done, void *param);
/* Queue a transaction; returns its id. Starts immediately if the bus is idle. */
int  i2c_master_queue(i2c_master_t *m, uint8_t addr, int is_read, const uint8_t *data, int n);
int  i2c_master_busy(const i2c_master_t *m);
#endif
