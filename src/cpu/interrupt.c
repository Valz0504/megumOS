#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"
#include "header/cpu/interrupt.h"

void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}


void io_wait(void)
{
    out(0x80, 0);
}

void pic_ack(uint8_t irq)
{
    if (irq >= 8)
        out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void)
{
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    uint8_t int_no = frame.int_number;

    switch (int_no) {
        case 0x00:
            framebuffer_write(0, 0, '0', 0x4, 0); 
            break;
        case 0x06:
            framebuffer_write(0, 0, '6', 0x4, 0); 
            break;
        case 0x0D:
            framebuffer_write(0, 0, 'D', 0x4, 0);
            break;
        case 0x0E:
            framebuffer_write(0, 0, 'E', 0x4, 0);
            break;

        case 0x20: 
            framebuffer_write(1, 0, 'T', 0xA, 0);
            pic_ack(IRQ_TIMER);
            break;

        case 0x21:
            framebuffer_write(1, 1, 'K', 0xA, 0);
            pic_ack(IRQ_KEYBOARD);
            break;

        default:
            framebuffer_write(5, 0, '!', 0xE, 0);
            framebuffer_write(5, 2, '0' + (int_no / 10), 0xE, 0);
            framebuffer_write(5, 3, '0' + (int_no % 10), 0xE, 0);

            if (int_no >= 32 && int_no < 48)
                pic_ack(int_no - 32);
            break;
    }
}


