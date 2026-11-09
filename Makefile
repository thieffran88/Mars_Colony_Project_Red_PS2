# Build nativo (PC/Linux) para testes. Para o PS2 use: make -f Makefile.ps2
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-misleading-indentation
SDLCF   := $(shell sdl2-config --cflags)
SDLLIB  := $(shell sdl2-config --libs)
SRC     := src/main.c src/game.c src/ai.c src/render.c src/audio.c src/artdata.c

all: marscolony sim

marscolony: $(SRC) src/*.h data/art.rle data/title.rle
	$(CC) $(CFLAGS) $(SDLCF) -o $@ $(SRC) $(SDLLIB) -lm

sim: tests/sim.c src/game.c src/ai.c src/game.h src/ai.h
	$(CC) $(CFLAGS) -o $@ tests/sim.c src/game.c src/ai.c

test: sim
	./sim 300

assets:
	python3 tools/gen_assets.py
	python3 tools/gen_art.py

clean:
	rm -f marscolony sim

.PHONY: all test assets clean
