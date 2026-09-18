/* Shared boilerplate for the Phase 1 spikes: load the ELF, make an atmega328p @16MHz. */
#ifndef SPIKE_COMMON_H
#define SPIKE_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "avr_ioport.h"
#include "avr_twi.h"

static avr_t *spike_load(const char *elf_path) {
    elf_firmware_t fw;
    memset(&fw, 0, sizeof fw);
    if (elf_read_firmware(elf_path, &fw) != 0) {
        fprintf(stderr, "cannot read %s\n", elf_path);
        exit(2);
    }
    /* Arduino ELFs carry no .mmcu section: name the part and clock explicitly. */
    strcpy(fw.mmcu, "atmega328p");
    fw.frequency = 16000000;
    avr_t *avr = avr_make_mcu_by_name(fw.mmcu);
    if (!avr) { fprintf(stderr, "no such mcu\n"); exit(2); }
    avr_init(avr);
    avr->log = LOG_WARNING;
    avr_load_firmware(avr, &fw);
    return avr;
}

static void spike_run_cycles(avr_t *avr, avr_cycle_count_t n) {
    avr_cycle_count_t end = avr->cycle + n;
    while (avr->cycle < end) {
        int st = avr_run(avr);
        if (st == cpu_Done || st == cpu_Crashed) {
            fprintf(stderr, "cpu stopped (state %d) at cycle %llu\n", st,
                    (unsigned long long)avr->cycle);
            exit(3);
        }
    }
}
#define MS(n) ((avr_cycle_count_t)(n) * 16000ULL)
#endif
