#ifndef PLANET_H
#define PLANET_H

#define C_WHITE 0x18
#define C_BLACK 0x19

#define OFF_SCREEN_PAGE 2

void set_color_palette();
void planet_graphics_init();
void draw_star_background();
void planet_graphics_first_frame();
void render_planet(int frame);
void free_planet_memory();

#endif


