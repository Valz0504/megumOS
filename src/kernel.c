#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"

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

// void kernel_setup(void) {
//     framebuffer_clear();
//     framebuffer_write(3, 8,  'H', 0, 0xF);
//     framebuffer_write(3, 9,  'a', 0, 0xF);
//     framebuffer_write(3, 10, 'i', 0, 0xF);
//     framebuffer_write(3, 11, '!', 0, 0xF);
//     framebuffer_set_cursor(3, 10);
//     while (true);
// }


void kernel_setup(void) {
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);

    int row = 0, col = 0;
    keyboard_state_activate();

    while (true) {
        char c;
        get_keyboard_buffer(&c);
        if (!c) continue; 

        if (c == '\b') {  
            if (col > 0) {
                col--;
                framebuffer_write(row, col, ' ', 0xF, 0);
                framebuffer_set_cursor(row, col);
            } else if (row > 0) {
                row--;
                col = 79;
                framebuffer_write(row, col, ' ', 0xF, 0);
                framebuffer_set_cursor(row, col);
            }
        } 
        else if (c == '\n') {
            row++;
            col = 0;
            framebuffer_set_cursor(row, col);
        } 
        else {
            framebuffer_write(row, col, c, 0xF, 0);
            col++;
            if (col >= 80) {
                col = 0;
                row++;
            }
            framebuffer_set_cursor(row, col);
        }
    }
}


// void kernel_setup(void) {
//     load_gdt(&_gdt_gdtr);
//     pic_remap();
//     initialize_idt();
//     framebuffer_clear();
//     framebuffer_set_cursor(0, 0);
//     __asm__("int $0x4");
//     while (true);
// }




