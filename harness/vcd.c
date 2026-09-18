#include "vcd.h"
#include <string.h>

static const char *code(int id, char *buf) { buf[0] = (char)('!' + id); buf[1] = 0; return buf; }

int vcd_open(vcd_t *v, const char *path, unsigned long long hz) {
    memset(v, 0, sizeof *v);
    v->f = fopen(path, "w");
    if (!v->f) return -1;
    fprintf(v->f, "$version mpsim (simavr) $end\n$comment clock %llu Hz; 1 cycle = %llu ps $end\n"
                  "$timescale 1ps $end\n$scope module magicpanel $end\n", hz, 1000000000000ULL / hz);
    return 0;
}

int vcd_add(vcd_t *v, const char *name, int width) {
    if (v->n >= VCD_MAX_SIGNALS) return -1;
    char c[2];
    strncpy(v->sig[v->n].name, name, sizeof v->sig[v->n].name - 1);
    v->sig[v->n].width = width;
    fprintf(v->f, "$var wire %d %s %s $end\n", width, code(v->n, c), name);
    return v->n++;
}

static void emit(vcd_t *v, int id, uint32_t value) {
    char c[2];
    if (v->sig[id].width == 1) fprintf(v->f, "%u%s\n", value & 1, code(id, c));
    else {
        fputc('b', v->f);
        for (int b = v->sig[id].width - 1; b >= 0; b--) fputc((value >> b) & 1 ? '1' : '0', v->f);
        fprintf(v->f, " %s\n", code(id, c));
    }
}

void vcd_start(vcd_t *v) {
    fputs("$upscope $end\n$enddefinitions $end\n#0\n$dumpvars\n", v->f);
    for (int i = 0; i < v->n; i++) emit(v, i, v->sig[i].last);
    fputs("$end\n", v->f);
    v->started = 1; v->last_cycle = 0;
}

void vcd_set(vcd_t *v, int id, unsigned long long cycle, uint32_t value) {
    if (id < 0) return;
    if (!v->started) { v->sig[id].last = value; return; }
    if (v->sig[id].inited && v->sig[id].last == value) return;
    if (cycle != v->last_cycle) { fprintf(v->f, "#%llu\n", cycle * 62500ULL); v->last_cycle = cycle; }
    emit(v, id, value);
    v->sig[id].last = value; v->sig[id].inited = 1;
}

void vcd_close(vcd_t *v, unsigned long long end_cycle) {
    if (!v->f) return;
    if (end_cycle > v->last_cycle) fprintf(v->f, "#%llu\n", end_cycle * 62500ULL);
    fclose(v->f); v->f = NULL;
}
