/* mpsim — deterministic simavr harness for the Magic Panel firmware.
 *
 *   mpsim --elf FW.elf --out RUNDIR [--script FILE | --run-cycles N | --run-ms N]
 *         [--eeprom FILE] [--no-vcd] [--interactive] [--quiet]
 *
 * Batch mode executes a line-oriented stimulus script (see parse_script) for a fixed cycle
 * budget. Interactive mode reads the same actions plus step_cycles, step_ms, display, quit from stdin and
 * answers each with one JSON line on stdout. All timing is in AVR cycles; nothing here reads
 * the wall clock. Outputs (RUNDIR/): frames.jsonl display.jsonl i2c.jsonl gpio.jsonl
 * events.jsonl filmstrip.txt summary.json trace.vcd. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_cycle_timers.h"
#include "avr_ioport.h"
#include "avr_eeprom.h"
#include "avr_twi.h"
#include "parts/max7221.h"
#include "panel.h"
#include "i2c_master.h"
#include "vcd.h"

#define HARNESS_VERSION "0.1.0"
#define F_CPU_HZ 16000000ULL
#define MS_TO_CYCLES(ms) ((avr_cycle_count_t)(ms) * (F_CPU_HZ / 1000ULL))

typedef struct step {
    avr_cycle_count_t cycle;
    enum { STEP_I2C_WRITE, STEP_I2C_READ, STEP_GPIO_SET, STEP_GPIO_RELEASE, STEP_MARKER } kind;
    uint8_t addr; uint8_t data[I2C_MAX_BYTES]; int n;
    char port; int pin; int value;
    char text[128];
    int line;
} step_t;

typedef struct sim {
    avr_t *avr;
    elf_firmware_t fw;
    max7221_chain_t chain;
    panel_state_t disp;
    unsigned long disp_seq;
    avr_cycle_count_t disp_prev_cycle;
    i2c_master_t i2c;
    FILE *f_frames, *f_display, *f_i2c, *f_gpio, *f_events, *f_film;
    vcd_t vcd; int vcd_on;
    int vcd_pin[3][8]; int vcd_twsr, vcd_latch, vcd_dispchg;
    char outdir[1024];
    unsigned long n_frames, n_display, n_i2c, n_gpio, n_events, n_latches;
    step_t *steps; int nsteps, steps_cap;
    avr_cycle_count_t run_cycles;
    int interactive, quiet;
    unsigned long disp_changes_window;     /* display changes since the last interactive command */
    i2c_txn_t last_txn; int have_last_txn;
    const char *elf_path, *eeprom_path, *script_path;
    int stopped; char stop_reason[64];
    uint8_t ext_mask[3], ext_value[3];     /* externally driven pin levels per port B,C,D */
} sim_t;

static sim_t *G;   /* for the logger only */

/* ------------------------------------------------------------------ utils */
static double ns_of(avr_cycle_count_t c) { return (double)c * (1e9 / (double)F_CPU_HZ); }

static void logger(avr_t *avr, const int level, const char *fmt, va_list ap) {
    if (G && G->quiet && level > LOG_WARNING) return;
    vfprintf(stderr, fmt, ap);
}

static void nosleep(avr_t *avr, avr_cycle_count_t how_long) { (void)avr; (void)how_long; }

static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', f); fputc(*s, f); }
        else if ((unsigned char)*s < 0x20) fprintf(f, "\\u%04x", *s);
        else fputc(*s, f);
    }
    fputc('"', f);
}

static void event(sim_t *s, const char *kind, const char *fmt, ...) {
    char buf[512]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    fprintf(s->f_events, "{\"cycle\":%llu,\"ns\":%.1f,\"kind\":\"%s\",\"detail\":",
            (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), kind);
    json_str(s->f_events, buf); fputs("}\n", s->f_events);
    s->n_events++;
}

static FILE *open_out(sim_t *s, const char *name) {
    char p[1200]; snprintf(p, sizeof p, "%s/%s", s->outdir, name);
    FILE *f = fopen(p, "w");
    if (!f) { perror(p); exit(2); }
    return f;
}

/* ------------------------------------------------------------------ display */
static void record_display(sim_t *s, int force) {
    panel_state_t now; panel_render(&s->chain, &now);
    if (!force && panel_state_equal(&now, &s->disp)) return;
    s->disp = now;
    avr_cycle_count_t c = s->avr->cycle;
    fprintf(s->f_display, "{\"seq\":%lu,\"cycle\":%llu,\"ns\":%.1f,", s->disp_seq, (unsigned long long)c, ns_of(c));
    panel_state_json(&now, s->f_display); fputs("}\n", s->f_display);
    panel_state_filmstrip(&now, s->disp_seq, c, s->disp_prev_cycle, s->f_film);
    s->disp_prev_cycle = c; s->disp_seq++; s->n_display++; s->disp_changes_window++;
    if (s->vcd_on) { vcd_set(&s->vcd, s->vcd_dispchg, c, 1); vcd_set(&s->vcd, s->vcd_dispchg, c, 0); }
}

static void on_frame(void *param, int dev, uint8_t reg, uint8_t value, uint16_t raw) {
    sim_t *s = param;
    fprintf(s->f_frames, "{\"cycle\":%llu,\"ns\":%.1f,\"latch\":%lu,\"device\":%d,\"reg\":%u,\"reg_name\":\"%s\",\"value\":%u,\"raw\":\"0x%04x\"}\n",
            (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), s->chain.latches, dev, reg,
            max7221_reg_name(reg), value, raw);
    s->n_frames++;
}

static void on_chain_event(void *param, const char *kind, const char *detail) {
    event((sim_t *)param, kind, "%s", detail);
}

/* ------------------------------------------------------------------ pins */
typedef struct pinctx { sim_t *s; char port; int pin; } pinctx_t;
static pinctx_t PINS[3][8];

static void on_pin(struct avr_irq_t *irq, uint32_t value, void *param) {
    pinctx_t *pc = param; sim_t *s = pc->s;
    int pi = pc->port - 'B';
    if (s->vcd_on) vcd_set(&s->vcd, s->vcd_pin[pi][pc->pin], s->avr->cycle, value);
    if (pc->port == 'B' && pc->pin == 0) { max7221_set_data(&s->chain, value); return; }
    if (pc->port == 'D' && pc->pin == 7) { max7221_set_clk(&s->chain, value); return; }
    if (pc->port == 'D' && pc->pin == 6) {
        unsigned long before = s->chain.latches + s->chain.empty_latches;
        max7221_set_load(&s->chain, value);
        if (s->chain.latches + s->chain.empty_latches != before) {
            s->n_latches = s->chain.latches;
            if (s->vcd_on) { vcd_set(&s->vcd, s->vcd_latch, s->avr->cycle, 1); vcd_set(&s->vcd, s->vcd_latch, s->avr->cycle, 0); }
            record_display(s, 0);
        }
        return;
    }
    if (pc->port == 'C' && (pc->pin == 4 || pc->pin == 5)) return;   /* TWI: message-level in simavr */
    fprintf(s->f_gpio, "{\"cycle\":%llu,\"ns\":%.1f,\"pin\":\"%c%d\",\"value\":%u}\n",
            (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), pc->port, pc->pin, value & 1);
    s->n_gpio++;
}

static void on_ddr(struct avr_irq_t *irq, uint32_t value, void *param) {
    pinctx_t *pc = param; sim_t *s = pc->s;
    fprintf(s->f_gpio, "{\"cycle\":%llu,\"ns\":%.1f,\"pin\":\"DDR%c\",\"value\":%u}\n",
            (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), pc->port, value & 0xFF);
    s->n_gpio++;
}

static void on_twsr(struct avr_irq_t *irq, uint32_t value, void *param) {
    sim_t *s = param;
    if (s->vcd_on) vcd_set(&s->vcd, s->vcd_twsr, s->avr->cycle, value & 0xFF);
}

/* ------------------------------------------------------------------ i2c */
static void on_i2c_done(void *param, const i2c_txn_t *t) {
    sim_t *s = param;
    fprintf(s->f_i2c, "{\"id\":%d,\"cycle\":%llu,\"ns\":%.1f,\"end_cycle\":%llu,\"requested_cycle\":%llu,"
                      "\"dir\":\"%s\",\"addr\":%u,\"bytes\":[",
            t->id, (unsigned long long)t->start, ns_of(t->start), (unsigned long long)t->end,
            (unsigned long long)t->requested, t->is_read ? "read" : "write", t->addr);
    int shown = t->n;
    for (int i = 0; i < shown; i++) fprintf(s->f_i2c, "%s%u", i ? "," : "", t->data[i]);
    fprintf(s->f_i2c, "],\"ack\":%s,\"acked_bytes\":%d,\"nack_timeout\":%s,\"filtered\":%s,\"short_read\":%s,\"stalled\":%s,\"twsr\":[",
            t->ack_addr ? "true" : "false", t->acked_bytes, t->nack_timeout ? "true" : "false",
            t->filtered ? "true" : "false", t->short_read ? "true" : "false", t->stalled ? "true" : "false");
    for (int i = 0; i < t->nstatus; i++) fprintf(s->f_i2c, "%s\"0x%02x\"", i ? "," : "", t->status[i]);
    fputs("]}\n", s->f_i2c);
    s->n_i2c++;
    s->last_txn = *t; s->last_txn.next = NULL; s->have_last_txn = 1;
}

/* ------------------------------------------------------------------ stimulus */
/* Drive a pin from outside. simavr re-derives input pin levels on every PORT/DDR write
 * (internal pull-ups win unless an 'external' level is registered), so we register the level
 * as an external pull AND raise the pin IRQ for immediate effect. */
static void apply_external(sim_t *s, char port) {
    int pi = port - 'B';
    avr_ioport_external_t e = { .name = port, .mask = s->ext_mask[pi], .value = s->ext_value[pi] };
    avr_ioctl(s->avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(port), &e);
}

static void do_gpio_set(sim_t *s, char port, int pin, int value) {
    int pi = port - 'B'; uint8_t bit = (uint8_t)(1 << pin);
    s->ext_mask[pi] |= bit;
    s->ext_value[pi] = (uint8_t)((s->ext_value[pi] & ~bit) | (value ? bit : 0));
    apply_external(s, port);
    avr_raise_irq(avr_io_getirq(s->avr, AVR_IOCTL_IOPORT_GETIRQ(port), pin), value ? 1 : 0);
    event(s, "gpio_set", "%c%d=%d", port, pin, value ? 1 : 0);
}

/* Stop driving a pin: it floats back to whatever the AVR does with it (internal pull-up if
 * enabled, else the last level). */
static void do_gpio_release(sim_t *s, char port, int pin) {
    int pi = port - 'B'; uint8_t bit = (uint8_t)(1 << pin);
    s->ext_mask[pi] &= (uint8_t)~bit;
    apply_external(s, port);
    /* PORTB=0x25, PORTC=0x28, PORTD=0x2B (I/O space) -> data[] index +0x20; DDR = PORT-1 */
    static const int port_addr[3] = { 0x25 + 0x20, 0x28 + 0x20, 0x2B + 0x20 };
    uint8_t ddr = s->avr->data[port_addr[pi] - 1], prt = s->avr->data[port_addr[pi]];
    if (!(ddr & bit) && (prt & bit))
        avr_raise_irq(avr_io_getirq(s->avr, AVR_IOCTL_IOPORT_GETIRQ(port), pin), 1);
    event(s, "gpio_release", "%c%d", port, pin);
}

static void run_step(sim_t *s, step_t *st) {
    switch (st->kind) {
    case STEP_I2C_WRITE: {
        int id = i2c_master_queue(&s->i2c, st->addr, 0, st->data, st->n);
        event(s, "i2c_queued", "id=%d write addr=0x%02x n=%d", id, st->addr, st->n); break; }
    case STEP_I2C_READ: {
        int id = i2c_master_queue(&s->i2c, st->addr, 1, NULL, st->n);
        event(s, "i2c_queued", "id=%d read addr=0x%02x n=%d", id, st->addr, st->n); break; }
    case STEP_GPIO_SET: do_gpio_set(s, st->port, st->pin, st->value); break;
    case STEP_GPIO_RELEASE: do_gpio_release(s, st->port, st->pin); break;
    case STEP_MARKER: event(s, "marker", "%s", st->text); break;
    }
}

static avr_cycle_count_t step_timer(struct avr_t *avr, avr_cycle_count_t when, void *param) {
    step_t *st = param; run_step(G, st); return 0;
}

static long parse_num(const char *t, int *ok) {
    char *e; long v = strtol(t, &e, 0); *ok = (e != t && *e == 0); return v;
}

/* Parse one action ("i2c_write 0x14 20 ...", "i2c_read 0x14 2", "gpio_set B3 0", "marker text")
 * into st; returns 0 on success, else an error string. */
static const char *parse_action(char *line, step_t *st) {
    char *save = NULL; char *tok = strtok_r(line, " \t\r\n", &save);
    if (!tok) return "empty action";
    int ok;
    if (!strcmp(tok, "i2c_write") || !strcmp(tok, "i2c_read")) {
        st->kind = tok[4] == 'w' ? STEP_I2C_WRITE : STEP_I2C_READ;
        tok = strtok_r(NULL, " \t\r\n", &save); if (!tok) return "missing address";
        long a = parse_num(tok, &ok); if (!ok || a < 0 || a > 0x7F) return "bad address";
        st->addr = (uint8_t)a; st->n = 0;
        if (st->kind == STEP_I2C_READ) {
            tok = strtok_r(NULL, " \t\r\n", &save); if (!tok) return "missing read length";
            long n = parse_num(tok, &ok); if (!ok || n < 0 || n > I2C_MAX_BYTES) return "bad read length";
            st->n = (int)n;
        } else {
            while ((tok = strtok_r(NULL, " \t\r\n", &save))) {
                long b = parse_num(tok, &ok); if (!ok || b < 0 || b > 255) return "bad data byte";
                if (st->n >= I2C_MAX_BYTES) return "too many bytes";
                st->data[st->n++] = (uint8_t)b;
            }
        }
        return NULL;
    }
    if (!strcmp(tok, "gpio_set") || !strcmp(tok, "gpio_release")) {
        st->kind = tok[5] == 's' ? STEP_GPIO_SET : STEP_GPIO_RELEASE;
        tok = strtok_r(NULL, " \t\r\n", &save); if (!tok || strlen(tok) != 2) return "pin must be like B3";
        st->port = (char)toupper((unsigned char)tok[0]); st->pin = tok[1] - '0';
        if (st->port < 'B' || st->port > 'D' || st->pin < 0 || st->pin > 7) return "pin out of range";
        if (st->kind == STEP_GPIO_RELEASE) return NULL;
        tok = strtok_r(NULL, " \t\r\n", &save); if (!tok) return "missing value";
        long v = parse_num(tok, &ok); if (!ok || (v != 0 && v != 1)) return "value must be 0 or 1";
        st->value = (int)v; return NULL;
    }
    if (!strcmp(tok, "marker")) {
        st->kind = STEP_MARKER; tok = strtok_r(NULL, "\r\n", &save);
        snprintf(st->text, sizeof st->text, "%s", tok ? tok : ""); return NULL;
    }
    return "unknown action";
}

static void add_step(sim_t *s, const step_t *st) {
    if (s->nsteps == s->steps_cap) {
        s->steps_cap = s->steps_cap ? s->steps_cap * 2 : 64;
        s->steps = realloc(s->steps, s->steps_cap * sizeof *s->steps);
    }
    s->steps[s->nsteps++] = *st;
}

static int parse_script(sim_t *s, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }
    char line[1024]; int ln = 0, errors = 0;
    while (fgets(line, sizeof line, f)) {
        ln++;
        char *p = line; while (isspace((unsigned char)*p)) p++;
        if (*p == 0 || *p == '#') continue;
        char key[32]; int consumed = 0;
        if (sscanf(p, "%31s%n", key, &consumed) != 1) continue;
        char *rest = p + consumed; while (isspace((unsigned char)*rest)) rest++;
        int ok;
        if (!strcmp(key, "run_cycles") || !strcmp(key, "run_ms")) {
            char *e; unsigned long long v = strtoull(rest, &e, 0);
            if (e == rest) { fprintf(stderr, "%s:%d: bad number\n", path, ln); errors++; continue; }
            s->run_cycles = key[4] == 'c' ? v : MS_TO_CYCLES(v);
        } else if (!strcmp(key, "at_cycle") || !strcmp(key, "at_ms")) {
            char num[32]; if (sscanf(rest, "%31s%n", num, &consumed) != 1) { errors++; continue; }
            long long v = parse_num(num, &ok); if (!ok || v < 0) { fprintf(stderr, "%s:%d: bad time\n", path, ln); errors++; continue; }
            step_t st; memset(&st, 0, sizeof st); st.line = ln;
            st.cycle = key[3] == 'c' ? (avr_cycle_count_t)v : MS_TO_CYCLES(v);
            const char *err = parse_action(rest + consumed, &st);
            if (err) { fprintf(stderr, "%s:%d: %s\n", path, ln, err); errors++; continue; }
            add_step(s, &st);
        } else { fprintf(stderr, "%s:%d: unknown directive '%s'\n", path, ln, key); errors++; }
    }
    fclose(f);
    return errors ? -1 : 0;
}

/* ------------------------------------------------------------------ run */
static void run_until(sim_t *s, avr_cycle_count_t end) {
    while (!s->stopped && s->avr->cycle < end) {
        int st = avr_run(s->avr);
        if (st == cpu_Done || st == cpu_Crashed) {
            s->stopped = 1; snprintf(s->stop_reason, sizeof s->stop_reason, st == cpu_Done ? "cpu_done" : "cpu_crashed");
            event(s, "cpu_stopped", "%s at cycle %llu", s->stop_reason, (unsigned long long)s->avr->cycle);
        }
    }
}

static void flush_all(sim_t *s) {
    fflush(s->f_frames); fflush(s->f_display); fflush(s->f_i2c); fflush(s->f_gpio); fflush(s->f_events); fflush(s->f_film);
    if (s->vcd_on) fflush(s->vcd.f);
}

static void write_summary(sim_t *s, const char *status) {
    FILE *f = open_out(s, "summary.json");
    fprintf(f, "{\n  \"harness_version\": \"%s\",\n  \"status\": \"%s\",\n  \"elf\": ", HARNESS_VERSION, status);
    json_str(f, s->elf_path);
    fprintf(f, ",\n  \"eeprom\": "); json_str(f, s->eeprom_path ? s->eeprom_path : "default:0xff");
    fprintf(f, ",\n  \"script\": "); json_str(f, s->script_path ? s->script_path : "");
    fprintf(f, ",\n  \"mcu\": \"atmega328p\",\n  \"f_cpu\": %llu,\n  \"total_cycles\": %llu,\n  \"total_ns\": %.1f,\n",
            F_CPU_HZ, (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle));
    fprintf(f, "  \"counts\": {\"frames\": %lu, \"latches\": %lu, \"empty_latches\": %lu, \"malformed_bursts\": %lu, "
               "\"display_changes\": %lu, \"i2c_transactions\": %lu, \"gpio_changes\": %lu, \"events\": %lu, \"script_steps\": %d},\n",
            s->n_frames, s->chain.latches, s->chain.empty_latches, s->chain.malformed, s->n_display, s->n_i2c, s->n_gpio,
            s->n_events, s->nsteps);
    fprintf(f, "  \"final_display\": {"); panel_state_json(&s->disp, f); fprintf(f, "}\n}\n");
    fclose(f);
}

/* ------------------------------------------------------------------ interactive */
static void reply_display(sim_t *s, FILE *out) {
    fprintf(out, "{\"ok\":true,\"cycle\":%llu,\"ns\":%.1f,\"seq\":%lu,", (unsigned long long)s->avr->cycle,
            ns_of(s->avr->cycle), s->disp_seq ? s->disp_seq - 1 : 0);
    panel_state_json(&s->disp, out);
    fputs(",\"ascii\":[", out);
    for (int r = 0; r < PANEL_ROWS; r++) {
        char line[9]; for (int i = 0; i < 8; i++) line[i] = (s->disp.rows[r] & (0x80 >> i)) ? '#' : '.'; line[8] = 0;
        fprintf(out, "%s\"%s\"", r ? "," : "", line);
    }
    fputs("]}\n", out);
}

static void interactive(sim_t *s) {
    char line[1024]; FILE *out = stdout;
    setvbuf(out, NULL, _IOLBF, 0);
    fprintf(out, "{\"ok\":true,\"ready\":true,\"cycle\":%llu,\"harness_version\":\"%s\"}\n",
            (unsigned long long)s->avr->cycle, HARNESS_VERSION);
    while (fgets(line, sizeof line, stdin)) {
        char *p = line; while (isspace((unsigned char)*p)) p++;
        if (*p == 0 || *p == '#') continue;
        char key[32]; int consumed = 0;
        if (sscanf(p, "%31s%n", key, &consumed) != 1) continue;
        char *rest = p + consumed; while (isspace((unsigned char)*rest)) rest++;
        s->disp_changes_window = 0;
        if (!strcmp(key, "quit")) break;
        if (!strcmp(key, "step_cycles") || !strcmp(key, "step_ms")) {
            char *e; unsigned long long v = strtoull(rest, &e, 0);
            if (e == rest) { fputs("{\"ok\":false,\"error\":\"bad number\"}\n", out); continue; }
            avr_cycle_count_t n = key[5] == 'c' ? v : MS_TO_CYCLES(v);
            run_until(s, s->avr->cycle + n);
            flush_all(s);
            fprintf(out, "{\"ok\":true,\"cycle\":%llu,\"ns\":%.1f,\"display_changed\":%lu,\"display_seq\":%lu,\"stopped\":%s}\n",
                    (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), s->disp_changes_window,
                    s->disp_seq ? s->disp_seq - 1 : 0, s->stopped ? "true" : "false");
            continue;
        }
        if (!strcmp(key, "display")) { reply_display(s, out); continue; }
        if (!strcmp(key, "status")) {
            fprintf(out, "{\"ok\":true,\"cycle\":%llu,\"ns\":%.1f,\"frames\":%lu,\"display_changes\":%lu,\"i2c\":%lu,\"gpio\":%lu,\"events\":%lu,\"i2c_busy\":%s,\"stopped\":%s}\n",
                    (unsigned long long)s->avr->cycle, ns_of(s->avr->cycle), s->n_frames, s->n_display, s->n_i2c, s->n_gpio,
                    s->n_events, i2c_master_busy(&s->i2c) ? "true" : "false", s->stopped ? "true" : "false");
            continue;
        }
        step_t st; memset(&st, 0, sizeof st);
        char copy[1024]; snprintf(copy, sizeof copy, "%s %s", key, rest);
        const char *err = parse_action(copy, &st);
        if (err) { fprintf(out, "{\"ok\":false,\"error\":"); json_str(out, err); fputs("}\n", out); continue; }
        st.cycle = s->avr->cycle;
        run_step(s, &st);
        if (st.kind == STEP_I2C_WRITE || st.kind == STEP_I2C_READ) {
            /* run until the transaction completes (bounded) so the caller gets ack/data */
            avr_cycle_count_t limit = s->avr->cycle + MS_TO_CYCLES(200);
            s->have_last_txn = 0;
            while (!s->have_last_txn && !s->stopped && s->avr->cycle < limit) run_until(s, s->avr->cycle + 1000);
            flush_all(s);
            if (!s->have_last_txn) { fprintf(out, "{\"ok\":false,\"error\":\"transaction did not complete within 200 ms\",\"cycle\":%llu}\n", (unsigned long long)s->avr->cycle); continue; }
            i2c_txn_t *t = &s->last_txn;
            fprintf(out, "{\"ok\":true,\"id\":%d,\"ack\":%s,\"acked_bytes\":%d,\"start_cycle\":%llu,\"end_cycle\":%llu,\"cycle\":%llu,\"display_changed\":%lu,\"bytes\":[",
                    t->id, t->ack_addr ? "true" : "false", t->acked_bytes, (unsigned long long)t->start,
                    (unsigned long long)t->end, (unsigned long long)s->avr->cycle, s->disp_changes_window);
            int shown = t->n;
            for (int i = 0; i < shown; i++) fprintf(out, "%s%u", i ? "," : "", t->data[i]);
            fputs("]}\n", out);
        } else {
            flush_all(s);
            fprintf(out, "{\"ok\":true,\"cycle\":%llu}\n", (unsigned long long)s->avr->cycle);
        }
    }
}

/* ------------------------------------------------------------------ main */
static void usage(void) {
    fputs("usage: mpsim --elf FW.elf --out DIR [--script FILE] [--run-cycles N | --run-ms N]\n"
          "             [--eeprom FILE] [--no-vcd] [--interactive] [--quiet]\n", stderr);
    exit(1);
}

int main(int argc, char **argv) {
    static sim_t s; G = &s;
    int vcd = 1; avr_cycle_count_t run_cycles = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--elf") && i + 1 < argc) s.elf_path = argv[++i];
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) snprintf(s.outdir, sizeof s.outdir, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--script") && i + 1 < argc) s.script_path = argv[++i];
        else if (!strcmp(argv[i], "--eeprom") && i + 1 < argc) s.eeprom_path = argv[++i];
        else if (!strcmp(argv[i], "--run-cycles") && i + 1 < argc) run_cycles = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--run-ms") && i + 1 < argc) run_cycles = MS_TO_CYCLES(strtoull(argv[++i], NULL, 0));
        else if (!strcmp(argv[i], "--no-vcd")) vcd = 0;
        else if (!strcmp(argv[i], "--interactive")) s.interactive = 1;
        else if (!strcmp(argv[i], "--quiet")) s.quiet = 1;
        else usage();
    }
    if (!s.elf_path || !s.outdir[0]) usage();
    mkdir(s.outdir, 0755);
    avr_global_logger_set(logger);

    /* --- MCU --- */
    if (elf_read_firmware(s.elf_path, &s.fw) != 0) { fprintf(stderr, "cannot read %s\n", s.elf_path); return 2; }
    strcpy(s.fw.mmcu, "atmega328p"); s.fw.frequency = (uint32_t)F_CPU_HZ;
    s.fw.vcc = s.fw.avcc = s.fw.aref = 5000;      /* mV; ADC3 input itself stays 0 (decision D-1) */
    s.avr = avr_make_mcu_by_name(s.fw.mmcu);
    if (!s.avr) { fprintf(stderr, "unknown mcu\n"); return 2; }
    avr_init(s.avr);
    s.avr->log = s.quiet ? LOG_WARNING : LOG_OUTPUT;
    s.avr->sleep = nosleep;                        /* never touch the wall clock */
    avr_load_firmware(s.avr, &s.fw);
    if (s.eeprom_path) {
        static uint8_t ee[1024]; memset(ee, 0xff, sizeof ee);
        FILE *f = fopen(s.eeprom_path, "rb"); if (!f) { perror(s.eeprom_path); return 2; }
        size_t n = fread(ee, 1, sizeof ee, f); fclose(f);
        avr_eeprom_desc_t d = { .ee = ee, .offset = 0, .size = (uint32_t)n };
        avr_ioctl(s.avr, AVR_IOCTL_EEPROM_SET, &d);
    }

    /* --- outputs --- */
    s.f_frames = open_out(&s, "frames.jsonl"); s.f_display = open_out(&s, "display.jsonl");
    s.f_i2c = open_out(&s, "i2c.jsonl"); s.f_gpio = open_out(&s, "gpio.jsonl");
    s.f_events = open_out(&s, "events.jsonl"); s.f_film = open_out(&s, "filmstrip.txt");
    s.vcd_on = vcd;
    if (vcd) {
        char p[1200]; snprintf(p, sizeof p, "%s/trace.vcd", s.outdir);
        if (vcd_open(&s.vcd, p, F_CPU_HZ) != 0) { perror(p); return 2; }
        static const char *names[3][8] = {
            {"PB0_DATA", "PB1", "PB2", "PB3_JUMPER1", "PB4_JUMPER_GND", "PB5_JUMPER2", "PB6", "PB7"},
            {"PC0_ROT4", "PC1_ROT2", "PC2_ROT1", "PC3_ADC_SEED", "PC4_SDA", "PC5_SCL", "PC6", "PC7"},
            {"PD0", "PD1", "PD2", "PD3", "PD4", "PD5", "PD6_LOAD", "PD7_CLK"}};
        for (int p2 = 0; p2 < 3; p2++) for (int i = 0; i < 8; i++) s.vcd_pin[p2][i] = vcd_add(&s.vcd, names[p2][i], 1);
        s.vcd_twsr = vcd_add(&s.vcd, "TWSR_STATUS", 8);
        s.vcd_latch = vcd_add(&s.vcd, "MAX7221_LATCH", 1);
        s.vcd_dispchg = vcd_add(&s.vcd, "DISPLAY_CHANGE", 1);
        vcd_start(&s.vcd);
    }

    /* --- models & hooks --- */
    max7221_init(&s.chain, PANEL_DEVICES, on_frame, on_chain_event, &s);
    for (int p = 0; p < 3; p++) {
        char port = (char)('B' + p);
        for (int i = 0; i < 8; i++) {
            PINS[p][i] = (pinctx_t){ &s, port, i };
            avr_irq_register_notify(avr_io_getirq(s.avr, AVR_IOCTL_IOPORT_GETIRQ(port), i), on_pin, &PINS[p][i]);
        }
        avr_irq_register_notify(avr_io_getirq(s.avr, AVR_IOCTL_IOPORT_GETIRQ(port), IOPORT_IRQ_DIRECTION_ALL), on_ddr, &PINS[p][0]);
    }
    avr_irq_register_notify(avr_io_getirq(s.avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_STATUS), on_twsr, &s);
    i2c_master_init(&s.i2c, s.avr, on_i2c_done, &s);
    event(&s, "reset", "elf=%s eeprom=%s", s.elf_path, s.eeprom_path ? s.eeprom_path : "default:0xff");
    record_display(&s, 1);                         /* power-on state, seq 0 at cycle 0 */

    /* --- stimulus --- */
    if (s.script_path && parse_script(&s, s.script_path) != 0) return 2;
    if (run_cycles) s.run_cycles = run_cycles;
    for (int i = 0; i < s.nsteps; i++) {
        step_t *st = &s.steps[i];
        if (st->cycle == 0) run_step(&s, st);
        else avr_cycle_timer_register(s.avr, st->cycle, step_timer, st);
    }

    if (s.interactive) {
        interactive(&s);
    } else {
        if (!s.run_cycles) { fprintf(stderr, "no run budget: give --run-cycles/--run-ms or run_* in the script\n"); return 2; }
        run_until(&s, s.run_cycles);
        if (i2c_master_busy(&s.i2c)) event(&s, "warning", "run ended with an I2C transaction still in progress");
    }
    event(&s, "end", "cycle %llu", (unsigned long long)s.avr->cycle);
    flush_all(&s);
    write_summary(&s, s.stopped ? s.stop_reason : "ok");
    if (s.vcd_on) vcd_close(&s.vcd, s.avr->cycle);
    fclose(s.f_frames); fclose(s.f_display); fclose(s.f_i2c); fclose(s.f_gpio); fclose(s.f_events); fclose(s.f_film);
    return s.stopped ? 3 : 0;
}
