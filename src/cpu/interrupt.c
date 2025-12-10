#include "../header/cpu/portio.h"
#include "../header/cpu/interrupt.h"
#include "../header/cpu/gdt.h" 
#include "../header/driver/keyboard.h"
#include "../header/text/framebuffer.h"
#include "../header/filesystem/ext2.h"
#include "../header/process/scheduler.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8; 
}


void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
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

void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

void syscall(struct InterruptFrame frame) {
    __asm__ volatile("sti");
    
    switch (frame.cpu.general.eax) {
        case 0:
            *((int8_t*) frame.cpu.general.ecx) = read(
                *(struct EXT2DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 1:
            *((int8_t*) frame.cpu.general.ecx) = read_directory(
                (struct EXT2DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 2:
            *((int8_t*) frame.cpu.general.ecx) = write(
                (struct EXT2DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 3:
            *((int8_t*) frame.cpu.general.ecx) = delete(
                *(struct EXT2DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 4: {
            char *output = (char*) frame.cpu.general.ebx;
            bool nonblocking = frame.cpu.general.ecx != 0;
            if (nonblocking) {
                if (!keyboard_try_read(output)) {
                    *output = '\0';
                }
            } else {
                get_keyboard_buffer(output);
            }
            break;
        }
        case 5:
            putchar(
                (char) frame.cpu.general.ebx, 
                frame.cpu.general.ecx
            );
            break;
        case 6:
            puts(
                (char*) frame.cpu.general.ebx, 
                frame.cpu.general.ecx, 
                frame.cpu.general.edx
            ); // Assuming puts() exist in kernel
            break;
        case 7: 
            keyboard_state_activate();
            break;
        case 8:
            framebuffer_clear();
            break;
        case 14:
            framebuffer_clear_color((uint8_t) frame.cpu.general.ebx);
            break;
        case 15:
            framebuffer_draw_text_frame(
                (const char *)frame.cpu.general.ebx,
                (uint8_t)frame.cpu.general.ecx,
                (uint8_t)frame.cpu.general.edx
            );
            break;
        case 16:
            *((int8_t*) frame.cpu.general.ecx) = read_partial(
                *((struct EXT2PartialRequest*) frame.cpu.general.ebx),
                (uint32_t*) frame.cpu.general.edx
            );
            break;
        case 17:
            framebuffer_draw_text_frame_color(
                (const char *)frame.cpu.general.ebx,
                (uint8_t)frame.cpu.general.ecx,
                (uint8_t)(frame.cpu.general.edx & 0xFF),
                (uint8_t)((frame.cpu.general.edx >> 8) & 0xFF)
            );
            break;
        case 9:
            *((int8_t*) frame.cpu.general.ecx) = move(
                (struct EXT2MoveRequest*) frame.cpu.general.ebx
            );
            break;
        case 10:
            *((int8_t*) frame.cpu.general.ecx) = read_directory(
                (struct EXT2DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 11:
            scheduler_clock_start();
            break;
        case 12:
            scheduler_clock_stop();
            break;
        case 13:
            *((bool*) frame.cpu.general.ecx) = scheduler_clock_status();
            break;
    }
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case PIC1_OFFSET + IRQ_TIMER:
            scheduler_tick(&frame);
            break;
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
        case 0x30:
            syscall(frame);
            break;
    }
}
