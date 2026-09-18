# Builds mpsim against the vendored, pinned simavr. `make -C harness` after `make setup`.
SIMAVR   := ../third_party/simavr/simavr
OBJDIR   := $(firstword $(wildcard $(SIMAVR)/obj-*))
LIBELF_CFLAGS := $(shell pkg-config --cflags libelf 2>/dev/null)
LIBELF_LIBS   := $(shell pkg-config --libs libelf 2>/dev/null || echo -lelf)
CFLAGS   += -std=gnu99 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -I. -I$(SIMAVR)/sim $(LIBELF_CFLAGS)
LDLIBS   += $(OBJDIR)/libsimavr.a $(LIBELF_LIBS) -lm
SRC      := mpsim.c panel.c i2c_master.c vcd.c parts/max7221.c
HDR      := panel.h i2c_master.h vcd.h parts/max7221.h

mpsim: $(SRC) $(HDR)
	@test -n "$(OBJDIR)" || { echo "libsimavr not built: run tools/setup.sh"; exit 1; }
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f mpsim
.PHONY: clean
