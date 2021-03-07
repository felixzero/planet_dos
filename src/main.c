#include "vga.h"
#include "planet.h"

int main()
{
	int frame;
    int latitude, longitude, sin_longitude, cos_latitude;
    int x, y;

    planet_graphics_init();
    
    /* Activate the correct VGA mode and set the background image */
    switch_mode_x();
    planet_graphics_first_frame();

    /* From now on, real time! */
    vga_page_flip();
    
	for (frame=0; frame<10000; frame++)
	{        
        render_planet(frame);
        vga_page_flip();
	}
    
    /* Go back to DOS */
    switch_vga_mode(0x03);
    
    free_planet_memory();

	return 0;
}
