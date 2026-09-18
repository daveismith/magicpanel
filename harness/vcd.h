/* Minimal deterministic VCD writer (no wall-clock date). Timescale 1 ps; 1 cycle = 62500 ps. */
#ifndef VCD_H
#define VCD_H
#include <stdio.h>
#include <stdint.h>
#define VCD_MAX_SIGNALS 32
typedef struct vcd {
    FILE *f;
    int n;
    struct { char name[24]; int width; uint32_t last; int inited; } sig[VCD_MAX_SIGNALS];
    unsigned long long last_cycle;
    int started;
} vcd_t;
int  vcd_open(vcd_t *v, const char *path, unsigned long long hz);
int  vcd_add(vcd_t *v, const char *name, int width);     /* returns signal id, before vcd_start */
void vcd_start(vcd_t *v);
void vcd_set(vcd_t *v, int id, unsigned long long cycle, uint32_t value);
void vcd_close(vcd_t *v, unsigned long long end_cycle);
#endif
