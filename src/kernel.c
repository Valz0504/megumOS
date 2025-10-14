#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

void pic_remap(void);
void activate_keyboard_interrupt(void);

void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    
    framebuffer_clear();
    framebuffer_write(0, 0, 'H', 0xF, 0x0);
    framebuffer_write(0, 1, 'I', 0xF, 0x0);
    framebuffer_write(0, 2, ' ', 0xF, 0x0);
    framebuffer_write(0, 3, 'W', 0xF, 0x0);
    framebuffer_write(0, 4, 'E', 0xF, 0x0);
    framebuffer_write(0, 5, 'L', 0xF, 0x0);
    framebuffer_write(0, 6, 'C', 0xF, 0x0);
    framebuffer_write(0, 7, 'O', 0xF, 0x0);
    framebuffer_write(0, 8, 'M', 0xF, 0x0);
    framebuffer_write(0, 9, 'E', 0xF, 0x0);
    framebuffer_write(0, 10, ' ', 0xF, 0x0);
    framebuffer_write(0, 11, 'T', 0xF, 0x0);
    framebuffer_write(0, 12, 'O', 0xF, 0x0);
    framebuffer_write(0, 13, ' ', 0xF, 0x0);
    framebuffer_write(0, 14, 'M', 0xF, 0x0);
    framebuffer_write(0, 15, 'E', 0xF, 0x0);
    framebuffer_write(0, 16, 'G', 0xF, 0x0);
    framebuffer_write(0, 17, 'U', 0xF, 0x0);
    framebuffer_write(0, 18, 'M', 0xF, 0x0);
    framebuffer_write(0, 19, 'O', 0xF, 0x0);
    framebuffer_write(0, 20, 'S', 0xF, 0x0);
    framebuffer_write(0, 21, ' ', 0xF, 0x0);


    // Initialize PIC
    pic_remap();
    
    // Initialize IDT
    initialize_idt();
    
    // Activate keyboard interrupt
    activate_keyboard_interrupt();
    keyboard_state_activate();
    
    // Simple keyboard echo demo
    uint8_t row = 2, col = 0;
    framebuffer_set_cursor(row, col);
    
    while (true) {
        char buf;
        get_keyboard_buffer(&buf);
        
        if (buf != 0) {
            if (buf == '\n') {
                // Handle enter key
                row++;
                col = 0;
                if (row >= 25) {
                    row = 2;
                    // Clear screen except title
                    for (uint8_t r = 2; r < 25; r++) {
                        for (uint8_t c = 0; c < 80; c++) {
                            framebuffer_write(r, c, ' ', 0x7, 0x0);
                        }
                    }
                }
            } else if (buf == '\b') {
                // Handle backspace
                if (col > 0) {
                    col--;
                    framebuffer_write(row, col, ' ', 0x7, 0x0);
                }
            } else {
                // Handle normal character
                framebuffer_write(row, col, buf, 0xF, 0x0);
                col++;
                if (col >= 80) {
                    col = 0;
                    row++;
                    if (row >= 25) {
                        row = 2;
                    }
                }
            }
            framebuffer_set_cursor(row, col);
        }
    }
}
