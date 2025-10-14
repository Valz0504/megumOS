#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/text/framebuffer.h"

void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) {
        out(PIC2_COMMAND, PIC_ACK);
    }
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
    // Start initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Master PIC vector offset
    out(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    // ICW2: Slave PIC vector offset
    out(PIC2_DATA, PIC2_OFFSET);
    io_wait();
    
    // ICW3: Tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    out(PIC1_DATA, 0b0100);
    io_wait();
    // ICW3: Tell Slave PIC its cascade identity (0000 0010)
    out(PIC2_DATA, 0b0010);
    io_wait();
    
    // ICW4: Set 8086/88 mode
    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void activate_keyboard_interrupt(void) {
    // Enable only keyboard interrupt (IRQ1)
    // Mask = 11111101 = 0xFD (all disabled except bit 1)
    out(PIC1_DATA, ~(1 << IRQ_KEYBOARD));
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
    }
}