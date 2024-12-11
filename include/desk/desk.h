#ifndef DESK_H
#define DESK_H

#include "../types.h"
#include "../vga.h"

#define DESK_WIDTH 800
#define DESK_HEIGHT 600

void desk_init();
void update();

void draw_utilitybar();
void addWindow(char* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

#endif
