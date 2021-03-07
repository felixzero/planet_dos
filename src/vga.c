#include "vga.h"

#define SCREEN_SEG 0x0A000
#define SEGMENT_PAGE_OFFSET 0x4B0
#define PAGE_VGA_OFFSET(p) (SCREEN_SEG + SEGMENT_PAGE_OFFSET*p)
#define CURRENT_VGA_OFFSET PAGE_VGA_OFFSET(current_vga_page)
#define SCREEN_W_BLOCK (SCREEN_W >> 2)

#define GC_INDEX 0x03ce
#define SC_INDEX 0x03c4
#define PALETTE_RESET 0x03c6
#define PALETTE_INDEX 0x03c8
#define PALETTE_DATA 0x03c9

unsigned int current_vga_page = 0;
char vga_color_palette[256*3];

void vga_page_flip()
{
    unsigned char start_offset = current_vga_page ? (SEGMENT_PAGE_OFFSET>>4) : 0;
    
    _asm {
        /* Wait for display to be active (i.e. within a frame) */
        mov dx,0x03da
        WaitActive:
            in al,dx
            test al,0x01
            jnz WaitActive
        
        /* Change display start address */
        mov dx,0x03d4
        mov al,0x0c
        mov ah,BYTE PTR start_offset
        out dx,ax
        
        /* Wait for VSync synchronization */
        mov dx,0x03da
        WaitVS:
            in al,dx
            test al,0x08
            jz WaitVS
    }
    
    current_vga_page = 1 - current_vga_page;
}

void set_vga_color_palette()
{
    char *vga_color_palette_ptr = vga_color_palette;
    
    _asm {
        /* Switch off interrupts */
        cli
        
        /* Set palette index to 0 */
        mov dx,0x3c8
        xor al,al
        out dx,al
        inc dx
        
        /* Batch copy the palette to the video card */
        mov cx,0x300
        lds si,vga_color_palette_ptr
        WriteEntry:
            lodsb
            out dx,al
            loop WriteEntry
        
        /* Restore interrupts */
        sti
    }
}

void clear_color(vga_color_t color, vga_plane_t plane_flag)
{
    unsigned int screen_seg = CURRENT_VGA_OFFSET;
	_asm {
        /* Select the proper planes. */
        mov dx,SC_INDEX
        mov ah,BYTE PTR plane_flag
        mov al,0x02
        out dx,ax
        
        /* Apply uniform color on the whole screen. */
        mov ax,WORD PTR screen_seg
        mov es,ax
        xor di,di
        mov cx,0x2580
        mov al,BYTE PTR color
        mov ah,al
        rep stosw
	}
}

void poke_pixel(vga_color_t color, vga_page_t page, vga_coord_t x, vga_coord_t y)
{
    vga_offset_t vga_location = PAGE_VGA_OFFSET(page);
    vga_offset_t vga_offset = y*SCREEN_W_BLOCK + (x>>2);
    char plane_flag = 1<<(x&0x03);
    
    _asm {
        mov al,0x02
        mov ah,BYTE PTR plane_flag
        mov dx,SC_INDEX
        out dx,ax
        
        mov ax,WORD PTR vga_location
        mov es,ax
        mov ax,WORD PTR vga_offset
        mov di,ax
        mov al,BYTE PTR color
        stosb
    }
}

void fill_half_chkbrd(vga_color_t color1, vga_color_t color2, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_plane_t pattern)
{
    vga_offset_t vga_location = CURRENT_VGA_OFFSET;
    vga_offset_t vga_offset = y1*SCREEN_W_BLOCK + (x1>>2);
    vga_offset_t width = (x2>>2)-(x1>>2);
    vga_offset_t height = (y2-y1)>>1;
    
    _asm {
        mov al,0x02
        mov ah,BYTE PTR pattern
        mov dx,SC_INDEX
        out dx,ax                           /* Set filling pattern */

        mov cx,WORD PTR height
        PrintLine:
            mov ax,WORD PTR vga_location
            mov es,ax                       /* Segment is the current screen */
            mov ax,0xA0
            mul cx
            sub ax,0xA0
            add ax,WORD PTR vga_offset
            mov di,ax                       /* Offset is the block containing the start pixel */
            push cx                         /* Save loop counter */
            
            mov cx,WORD PTR width
            mov al,BYTE PTR color1
            rep stosb                       /* Fill one line with the first color */
            
            sub di,WORD PTR width
            add di,0x50                     /* Go at the beginning of next line */
            
            mov cx,WORD PTR width
            mov al,BYTE PTR color2
            rep stosb                       /* Fill one line with the second color */
            
            pop cx                          /* Restore loop counter */
            loop PrintLine
    }    
}

void fill_chkbrd(vga_color_t color1, vga_color_t color2, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2)
{
    fill_half_chkbrd(color1, color2, x1, y1, x2, y2, 0xa);  /* 1010 */
    fill_half_chkbrd(color2, color1, x1, y1, x2, y2, 0x5);  /* 0101 */
}

void copy_system_to_video_plane(char *source, vga_coord_t plane_pixel_offset, vga_page_t page, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_color_t bit_mask)
{
    vga_offset_t vga_location = PAGE_VGA_OFFSET(page);
    vga_offset_t vga_offset = y1*SCREEN_W_BLOCK + (x1>>2);
    vga_offset_t width = (x2>>2)-(x1>>2);
    vga_offset_t height = y2-y1;
    vga_plane_t plane = (1<<plane_pixel_offset);
    
    /* Select the bit mask to write */
    register_out(GC_INDEX, (bit_mask<<8) | 0x08);
    
    _asm {
        mov ah,BYTE PTR plane
        mov al,0x02
        mov dx,SC_INDEX
        out dx,ax                   /* Copy 1000 pixels */
        
        mov cx,WORD PTR height
        WriteLine:
            mov bx,cx
            dec bx
        
            mov ax,WORD PTR vga_location
            mov es,ax                   /* Destination segment is VGA page */
            mov ax,0x50
            mul bx
            add ax,WORD PTR vga_offset
            mov di,ax                   /* Offset is the block containing the start pixel */

            /* Set the source to the proper location */
            mov ax,WORD PTR width
            shl ax,2
            mul bx
            lds si,source
            add si,ax
            add si,WORD PTR plane_pixel_offset
            
            push cx                     /* Saving main loop counter */
            mov cx,WORD PTR width
            WritePixel:
                mov al,[es:di]          /* Load the VGA latch registers */
                movsb
                add si,3
                loop WritePixel         /* Copy one row */
            
            pop cx                      /* Restoring main loop counter */
            loop WriteLine
    }
    
    /* Restore original flags: all bits from CPU, none from latches */
    register_out(GC_INDEX, 0xFF08);
}

void copy_system_to_video(char *source, vga_page_t page, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_color_t bit_mask)
{
    copy_system_to_video_plane(source, 0, page, x1, y1, x2, y2, bit_mask);
    copy_system_to_video_plane(source, 1, page, x1, y1, x2, y2, bit_mask);
    copy_system_to_video_plane(source, 2, page, x1, y1, x2, y2, bit_mask);
    copy_system_to_video_plane(source, 3, page, x1, y1, x2, y2, bit_mask);
}

void copy_video_to_video(vga_page_t source_page, vga_coord_t source_x, vga_coord_t source_y, vga_page_t destination_page, vga_coord_t x1, vga_coord_t y1, vga_coord_t x2, vga_coord_t y2, vga_plane_t plane_flag)
{
    vga_offset_t source_segment = PAGE_VGA_OFFSET(source_page);
    vga_offset_t destination_segment = PAGE_VGA_OFFSET(destination_page);
    vga_offset_t destination_vga_offset = y1*SCREEN_W_BLOCK + (x1>>2);
    vga_offset_t source_vga_offset = source_y*SCREEN_W_BLOCK + (source_x>>2);
    vga_offset_t width = (x2>>2)-(x1>>2);
    vga_offset_t height = y2-y1;
    
    /* Select all bits from the latch, none from the CPU */
    register_out(GC_INDEX, 0x0008);
    
    _asm {
        /* Save "CPU" data segment */
        push ds
        
        /* Select the proper planes. */
        mov dx,SC_INDEX
        mov ah,BYTE PTR plane_flag
        mov al,0x02
        out dx,ax
        
        /* Select the source and destination page */
        mov ax,WORD PTR destination_segment
        mov es,ax
        mov ax,WORD PTR source_segment
        mov ds,ax
        
        mov cx,WORD PTR height
        mov dx,WORD PTR width
        mov di,WORD PTR destination_vga_offset
        mov si,WORD PTR source_vga_offset
        mov ax,0x50
        sub ax,dx
        WriteLine:
            /* Save the line counter */
            mov bx,cx
            
            /* Copy one line */
            mov cx,dx
            rep movsb
            
            /* Align the pointers to the next line */
            sub di,dx
            add di,0x50
            sub si,dx
            add si,0x50
            
            /* Restore the line counter and loop */
            mov cx,bx
            loop WriteLine
        
        /* Restore CPU data segment */
        pop ds
    }
    
    /* Restore original flags: all bits from CPU, none from latches */
    register_out(GC_INDEX, 0xFF08);
}

void switch_vga_mode(vga_mode_t mode)
{
    _asm {
        xor ah,ah
        mov al,BYTE PTR mode
        int 0x10
    }
}

void switch_mode_x()
{
    switch_vga_mode(0x13);
    
    register_out(SC_INDEX, 0x0604);
    register_out(SC_INDEX, 0x0100);
    register_out(0x03c2, 0x00e3);
    register_out(SC_INDEX, 0x0300);
    
    _asm {
        mov dx,0x03d4
        mov al,0x11
        out dx,al
        inc dx
        in al,dx
        and al,0x7f
        out dx,al
    }
    
    register_out(0x03d4, 0x0d06);
    register_out(0x03d4, 0x3e07);
    register_out(0x03d4, 0x4109);
    register_out(0x03d4, 0xea10);
    register_out(0x03d4, 0xac11);
    register_out(0x03d4, 0xdf12);
    register_out(0x03d4, 0x0014);
    register_out(0x03d4, 0xe715);
    register_out(0x03d4, 0x0616);
    register_out(0x03d4, 0xe317);
}

