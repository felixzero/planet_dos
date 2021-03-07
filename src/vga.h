#ifndef VGA_H
#define VGA_H

#define SCREEN_W 320
#define SCREEN_H 240

#define VGA_COLOR_PALETTE_R(i) vga_color_palette[3*(i) + 0]
#define VGA_COLOR_PALETTE_G(i) vga_color_palette[3*(i) + 1]
#define VGA_COLOR_PALETTE_B(i) vga_color_palette[3*(i) + 2]

typedef unsigned char vga_color_t;
typedef unsigned char vga_plane_t;
typedef unsigned int vga_coord_t;
typedef unsigned int vga_offset_t;
typedef unsigned char vga_mode_t;
typedef unsigned int vga_page_t;

inline void register_out(unsigned int reg, unsigned int value)
{
    _asm {
        mov dx,WORD PTR reg
        mov ax,WORD PTR value
        out dx,ax
    }
}

extern vga_page_t current_vga_page;
extern char vga_color_palette[];

/* Double buffering functions */
void vga_page_flip();

/* Color palette functions */
void set_vga_color_palette();

/* Basic drawing functions */
void clear_color(vga_color_t color, vga_plane_t plane_flag);
void poke_pixel(vga_color_t color, vga_page_t page, vga_coord_t x, vga_coord_t y);
void fill_chkbrd(vga_color_t color1, vga_color_t color2, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2);

/* Memory copy functions */
void copy_system_to_video(char *source, vga_page_t page, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_color_t bit_mask);
void copy_video_to_video(vga_page_t source_page, vga_coord_t source_x, vga_coord_t source_y, vga_page_t destination_page, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_plane_t plane_flag);

/* Initialization functions */
void switch_vga_mode(vga_mode_t mode);
void switch_mode_x();

#endif


