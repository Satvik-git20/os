# Smart Traffic Management System — src/Makefile

Linux+gcc toolchain for building the Module 2–4 C sources. Requires `gcc` and, for the bounded-buffer
test, `pthread`.

```make
CC      := gcc
CFLAGS  := -Wall -Wextra -O2
LDFLAGS := -lm

BIN     := bin
SRC     := $(wildcard *.c)
OBJ     := $(SRC:.c=.o)
BINS    := stms_sim stms_boundedbuffer stms_scheduler stms_lru

.PHONY: all clean

all: $(BIN) $(BINS)

$(BIN):
	mkdir -p $(BIN)

stms_sim: stms_sim.o
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^ $(LDFLAGS)

stms_boundedbuffer: stms_boundedbuffer.o
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^ -lpthread

stms_scheduler: stms_scheduler.o
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^

stms_lru: stms_lru.o
	$(CC) $(CFLAGS) -o $(BIN)/$@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN) *.o
```

## Build & run

```bash
cd src
make
./bin/stms_sim         # Modules 2 + 3 (priority schedule + Banker's)
./bin/stms_boundedbuffer   # Module 3 producer–consumer demo
./bin/stms_scheduler       # Module 4 ageing scheduler demo
./bin/stms_lru             # Module 4 LRU vs FIFO benchmark
```

> The C sources are written for a Linux `gcc` toolchain. On this workstation (no C compiler) they are
> numerically cross-checked by the pure-Python reference in `python/` and printed by
> `python/simulate.py`.
