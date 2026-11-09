# Build nativo (PC/Linux) para testes. Para o PS2 use: make -f Makefile.ps2
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-misleading-indentation
SDLCF   := $(shell sdl2-config --cflags)
SDLLIB  := $(shell sdl2-config --libs)
SRC     := src/main.c src/game.c src/ai.c src/render.c src/audio.c src/synth.c src/artdata.c

all: marscolony sim audio_render

marscolony: $(SRC) src/*.h data/art.rle data/title.rle
	$(CC) $(CFLAGS) $(SDLCF) -o $@ $(SRC) $(SDLLIB) -lm

sim: tests/sim.c src/game.c src/ai.c src/game.h src/ai.h
	$(CC) $(CFLAGS) -o $@ tests/sim.c src/game.c src/ai.c

audio_render: tests/audio_render.c src/synth.c src/synth.h
	$(CC) $(CFLAGS) -o $@ tests/audio_render.c src/synth.c -lm

test: sim audio_render
	./sim 300
	./audio_render /tmp

assets:
	python3 tools/gen_assets.py
	python3 tools/gen_art.py

clean:
	rm -f marscolony sim audio_render

.PHONY: all test assets clean
