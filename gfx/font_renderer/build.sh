#!/bin/sh

gcc -std=c99 -O2 -Wall -D_POSIX_C_SOURCE=199309L ./gfx/font_renderer/font_renderer.c -o ./gfx/font_renderer/font_renderer -lX11 -lm
