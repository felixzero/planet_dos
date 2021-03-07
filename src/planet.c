#include "planet.h"
#include "vga.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <malloc.h>
#include <string.h>

#define PROJECTION_X_SHIFT 7
#define PROJECTION_Y_SHIFT (PROJECTION_X_SHIFT-1)
#define PROJECTION_X (1<<PROJECTION_X_SHIFT)
#define PROJECTION_Y (1<<PROJECTION_Y_SHIFT)
#define PLANET_PROJECTION(x,y) (planet_projection[((x)<<PROJECTION_Y_SHIFT) + (y)])
#define FRACTAL_NOISE_BUFFER(x,y,desamp) (fractal_noise_buffer[(((x)<<PROJECTION_Y_SHIFT)>>(desamp)) + (y)])

#define SIZE_3D_SHIFT 7
#define SIZE_3D (1<<SIZE_3D_SHIFT)
#define PLANET_3D(x,y) (planet_3d[(y<<SIZE_3D_SHIFT) + x])
#define PROJECTION_LOOKUP_TABLE(x,y) (projection_lookup_table[(y<<SIZE_3D_SHIFT) + x])

#define NOISE_3D_SHIFT 7

#define LATITUDE_LAYER_X 0
#define LATITUDE_LAYER_Y 0

char *planet_projection;
char *fractal_noise_buffer;
vga_color_t planet_3d[SIZE_3D*SIZE_3D];
unsigned int projection_lookup_table[SIZE_3D*SIZE_3D];

void set_projection_lookup_table()
{
    FILE *fp;
    
    fp = fopen("PROJECT.DAT", "rb");
    fread(projection_lookup_table, 2, SIZE_3D*SIZE_3D, fp);
    fclose(fp);
}

inline void set_palette_entry(int altitude, int latitude, int shading, char r, char g, char b)
{
    VGA_COLOR_PALETTE_R((altitude<<5)|(shading<<3)|latitude) = r;
    VGA_COLOR_PALETTE_G((altitude<<5)|(shading<<3)|latitude) = g;
    VGA_COLOR_PALETTE_B((altitude<<5)|(shading<<3)|latitude) = b;
}

inline char get_palette_entry(int altitude, int latitude, int shading, int canal)
{
    return vga_color_palette[3*((altitude<<5)|(shading<<3)|latitude) + canal];
}

void set_color_palette()
{
    const int ice_threshold = 40;
    const int sea_ice_threshold = 30;
    const int shore_threshold = 4;
    
    const char rock_r = 20, rock_g = 18, rock_b = 10;
    
    int latitude, altitude, r, factor, latitude_lookup;
    
    /* Set land colors */
    set_palette_entry(shore_threshold, 0, 1,  5, 25,  0);  /* Equatorial forest */
    set_palette_entry(shore_threshold, 1, 1, 15, 32,  7);
    set_palette_entry(shore_threshold, 2, 1, 35, 41, 15);  
    set_palette_entry(shore_threshold, 3, 1, 50, 50, 20); /* Tropical desert */
    set_palette_entry(shore_threshold, 4, 1, 35, 43, 15);
    set_palette_entry(shore_threshold, 5, 1, 26, 35, 10);
    set_palette_entry(shore_threshold, 6, 1, 12, 27,  5);
    set_palette_entry(shore_threshold, 7, 1,  4, 20,  0);  /* Toundra */
    
    for (altitude = shore_threshold+1; altitude<8; altitude++)
    {
        for (latitude=0; latitude<8; latitude++)
        {
            factor = (altitude - shore_threshold) * 256 / (8 - shore_threshold);
            r = latitude*4 + altitude*3;
            latitude_lookup = (r - shore_threshold*3)/4;
            
            set_palette_entry(altitude, latitude, 1,
                        (get_palette_entry(shore_threshold, latitude_lookup, 1, 0) * (256-factor) + rock_r*factor)>>8,
                        (get_palette_entry(shore_threshold, latitude_lookup, 1, 1) * (256-factor) + rock_g*factor)>>8,(get_palette_entry(shore_threshold, latitude_lookup, 1, 2) * (256-factor) + rock_b*factor)>>8);
        }
    }
    
    /* Set see and ice colors */
    for (latitude=0; latitude<8; latitude++)
        for (altitude=0; altitude<8; altitude++)
        {
            r = latitude*4 + altitude*3; /* 0 - 49 range */

            if (r > ice_threshold)
                set_palette_entry(altitude, latitude, 1, 60, 60, 60);
            if (altitude < shore_threshold-2)
                set_palette_entry(altitude, latitude, 1, 0, 12, 42);
            else if (altitude == shore_threshold-2)
                set_palette_entry(altitude, latitude, 1, 4, 22, 55);
            else if (altitude == shore_threshold-1)
                set_palette_entry(altitude, latitude, 1, 4, 40, 55);
            if (altitude < shore_threshold && r > sea_ice_threshold)
                set_palette_entry(altitude, latitude, 1, 40, 50, 60);
        }
    
    /* Apply shading effects to palette colors */
    for (latitude=0; latitude<8; latitude++)
        for (altitude=0; altitude<8; altitude++)
        {
            /* Dark */
            set_palette_entry(altitude, latitude, 0, 0, 0, 0);
            
            /* Specular only on sea */
            if (altitude < shore_threshold)
                set_palette_entry(altitude, latitude, 2,
                                  (63+get_palette_entry(altitude, latitude, 1, 0))/2,
                                  (63+get_palette_entry(altitude, latitude, 1, 1))/2,
                                  (63+get_palette_entry(altitude, latitude, 1, 2))/2);
            else
                set_palette_entry(altitude, latitude, 2,
                                  get_palette_entry(altitude, latitude, 1, 0),
                                  get_palette_entry(altitude, latitude, 1, 1),
                                  get_palette_entry(altitude, latitude, 1, 2));
        }
    
    for (latitude=0; latitude<8; latitude++)
        set_palette_entry(shore_threshold, latitude, 0, 10, 8, 0);

    /* Static color range: 0b...11... */
    VGA_COLOR_PALETTE_R(C_BLACK) = 0;
    VGA_COLOR_PALETTE_G(C_BLACK) = 0;
    VGA_COLOR_PALETTE_B(C_BLACK) = 0;

    VGA_COLOR_PALETTE_R(C_WHITE) = 63;
    VGA_COLOR_PALETTE_G(C_WHITE) = 63;
    VGA_COLOR_PALETTE_B(C_WHITE) = 63;
    
    set_vga_color_palette();
}

void planet_graphics_init()
{
    unsigned int i, j, k;
    unsigned int desamp, pole_smoothing;
    unsigned int ul, ur, ll, lr;
    unsigned int x_factor, y_factor;
    int normalized;
    
    /* Initialization */
    srand(time(NULL));
    
    planet_projection = malloc(PROJECTION_X*PROJECTION_Y*sizeof(char));
    fractal_noise_buffer = malloc(PROJECTION_X*PROJECTION_Y*sizeof(char));
    
    for (i = 0; i < PROJECTION_X; ++i)
        for (j = 0; j < PROJECTION_Y; ++j)
            PLANET_PROJECTION(i, j) = 0;
    
    /* Set altitude map */
    for(desamp = 1; desamp < 5; ++desamp)
    {
        /* Fill noise buffer with random numbers */
        for (i=0; i<(PROJECTION_X>>desamp); i++)
            for (j=0; j<(PROJECTION_Y>>desamp); j++)
                FRACTAL_NOISE_BUFFER(i, j, desamp) = rand();

        for (i=0; i<(PROJECTION_X>>desamp); i++)
            for (j=0; j<(PROJECTION_Y>>desamp); j++)
            {
                if ((j < ((PROJECTION_Y>>desamp)>>3)) || (j >= (PROJECTION_Y>>desamp)-((PROJECTION_Y>>desamp)>>3)))
                    pole_smoothing = 8;
                else if ((j < ((PROJECTION_Y>>desamp)>>2)) || (j >= (PROJECTION_Y>>desamp)-((PROJECTION_Y>>desamp)>>2)))
                    pole_smoothing = 4;
                else
                    pole_smoothing = 1;
                
                FRACTAL_NOISE_BUFFER(i, j, desamp) /= pole_smoothing;
                for (k=0; k<pole_smoothing; k++)
                    FRACTAL_NOISE_BUFFER(i, j, desamp) += FRACTAL_NOISE_BUFFER((i-k)%(PROJECTION_X>>desamp), j, desamp)/pole_smoothing;
                
            }
            
        /* Perform basic linear interpolation */
        for (i = 0; i < PROJECTION_X; ++i)
            for (j = 0; j < PROJECTION_Y; ++j)
            {
                ll = FRACTAL_NOISE_BUFFER(i>>desamp, j>>desamp, desamp);
                lr = FRACTAL_NOISE_BUFFER(i>>desamp, ((j>>desamp)+1)%(PROJECTION_Y>>desamp), desamp);
                ul = FRACTAL_NOISE_BUFFER(((i>>desamp)+1)%(PROJECTION_X>>desamp), j>>desamp, desamp);
                ur = FRACTAL_NOISE_BUFFER(((i>>desamp)+1)%(PROJECTION_X>>desamp), ((j>>desamp)+1)%(PROJECTION_Y>>desamp), desamp);
                
                x_factor = (i % (1<<desamp)) * 16 / (1<<desamp);
                y_factor = (j % (1<<desamp)) * 16 / (1<<desamp);

                PLANET_PROJECTION(i, j) += ((ur*(x_factor * y_factor))/(16*16)
                                    + (ll*((16-x_factor) * (16-y_factor)))/(16*16)
                                    + (ul*(x_factor * (16-y_factor)))/(16*16)
                                    + (lr*((16-x_factor) * y_factor)/(16*16)))/2;
                PLANET_PROJECTION(i, j) /= 2;
            }
    }
    
    /* Format altitude map into the 3 following bits: 0b11100000 */
        for (i = 0; i < PROJECTION_X; ++i)
            for (j = 0; j < PROJECTION_Y; ++j)
            {
                normalized = 8*((int)(PLANET_PROJECTION(i, j)) - 64);
                if (normalized > 127)
                    normalized = 127;
                if (normalized < -128)
                    normalized = -128;
                PLANET_PROJECTION(i, j) = (((char)(normalized+128))>>5)<<5;
            }
    
    /* Load pre-computed projection */
    set_projection_lookup_table();
}

void planet_graphics_first_frame()
{
    int lookup, i, j, latitude, longitude, phong_distance;
    
    set_color_palette();
    
    clear_color(C_BLACK, 0x0f);
    draw_star_background();
    
    /* Clear planet sprite */
    memset(planet_3d, C_BLACK, SIZE_3D*SIZE_3D);
    
    /* Make latitude texture */
    for (i = 0; i < SIZE_3D; ++i)
        for(j = 0; j < SIZE_3D; ++j)
        {
            lookup = PROJECTION_LOOKUP_TABLE(i, j);

            if (lookup >= PROJECTION_X*PROJECTION_Y)
                continue;
            latitude = abs((lookup % PROJECTION_Y) - (PROJECTION_Y>>1)) * 32 / (PROJECTION_Y>>1);
            if ((latitude % 4 == 0)                                     /* 0/4: Flat copy */
                || ((latitude>>2) == 7)                                 /* Special case: the pole */
                || ((latitude % 4 == 2) && ((i+j)%2 == 0))              /* 2/4: checkerboard */
                || ((latitude % 4 == 1) && !((i%2 == 0) && (j%2 == 0))) /* 1/4: kitchen tiling */
                || ((latitude % 4 == 3) && ((i%2 == 0) && (j%2 == 0)))) /* 3/4: inverted kitchen tiling */
                PLANET_3D(i, j) = (latitude>>2);
            else
                PLANET_3D(i, j) = (latitude>>2)+1;
        }
    
    copy_system_to_video(planet_3d, OFF_SCREEN_PAGE, LATITUDE_LAYER_X, LATITUDE_LAYER_Y,
                         LATITUDE_LAYER_X + SIZE_3D, LATITUDE_LAYER_Y + SIZE_3D, 0xFF);
    
    /* Make shading map */
    for (i = 0; i < SIZE_3D; ++i)
        for(j = 0; j < SIZE_3D; ++j)
        {
            lookup = PROJECTION_LOOKUP_TABLE(i, j);

            if (lookup >= PROJECTION_X*PROJECTION_Y)
                continue;
            
            latitude = abs((lookup % PROJECTION_Y) - (PROJECTION_Y>>1)) * 32 / (PROJECTION_Y>>1);
            phong_distance = latitude*latitude + (longitude - PROJECTION_X/8 - 2)*(longitude - PROJECTION_X/8);
            
            longitude = (lookup/PROJECTION_Y);
            if ((longitude > PROJECTION_X*3/8) || ((longitude > (PROJECTION_X*3/8 - 4)) && ((i+j)%2 == 0)))
                PLANET_3D(i, j) = 0x00;
            else if ((phong_distance < 5) || ((phong_distance < 20) && ((i+j)%2 == 0)))
                PLANET_3D(i, j) = 0x10;
            else
                PLANET_3D(i, j) = 0x08;
        }
    copy_system_to_video(planet_3d, OFF_SCREEN_PAGE, LATITUDE_LAYER_X, LATITUDE_LAYER_Y,
                         LATITUDE_LAYER_X + SIZE_3D, LATITUDE_LAYER_Y + SIZE_3D, 0x18);
    
    /* Duplicate screen 0 to screen 1 */
    copy_video_to_video(0, 0, 0, 1, 0, 0, SCREEN_W, SCREEN_H, 0x0F);
}

void draw_star_background()
{
    int i, x, y;
    
    /* Draw random small stars */
    for (i=0; i<100; ++i)
    {
        x = rand() % SCREEN_W;
        y = rand() % SCREEN_H;
        poke_pixel(C_WHITE, 0, x, y);       
    }
    
    /* Draw random big stars */
    for (i=0; i<10; ++i)
    {
        x = rand() % SCREEN_W;
        y = rand() % SCREEN_H;
        poke_pixel(C_WHITE, 0, x, y);       
        poke_pixel(C_WHITE, 0, x+1, y);
        poke_pixel(C_WHITE, 0, x, y+1);
        poke_pixel(C_WHITE, 0, x+1, y+1);
    }
}

void render_planet(int frame)
{
    int i, j;
    int lookup;
    
    frame %= PROJECTION_X;
    
    /* Clear planet sprite */
    memset(planet_3d, C_BLACK, SIZE_3D*SIZE_3D);
    
    /* Project altitude texture to sphere */
    for (i = 0; i < SIZE_3D; ++i)
        for(j = 0; j < SIZE_3D; ++j)
        {
            lookup = PROJECTION_LOOKUP_TABLE(i, j);
            if (lookup < PROJECTION_X*PROJECTION_Y)
                PLANET_3D(i, j) = planet_projection[(lookup + frame*PROJECTION_Y) % (PROJECTION_X*PROJECTION_Y)];
        }
    
    copy_video_to_video(OFF_SCREEN_PAGE, LATITUDE_LAYER_X, LATITUDE_LAYER_Y,
                        current_vga_page, (SCREEN_W>>1) - (SIZE_3D>>1), (SCREEN_H>>1) - (SIZE_3D>>1),
                        (SCREEN_W>>1) + (SIZE_3D>>1), (SCREEN_H>>1) + (SIZE_3D>>1), 0x0F);
    copy_system_to_video(planet_3d, current_vga_page,
                        (SCREEN_W>>1) - (SIZE_3D>>1), (SCREEN_H>>1) - (SIZE_3D>>1),
                        (SCREEN_W>>1) + (SIZE_3D>>1), (SCREEN_H>>1) + (SIZE_3D>>1), 0xE0);
}

void free_planet_memory()
{
    free(planet_projection);
    free(fractal_noise_buffer);
}
