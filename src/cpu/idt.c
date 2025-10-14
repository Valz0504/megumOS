#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"
#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"

// ---------------------------------------------------------------------------
//  External symbols
// ---------------------------------------------------------------------------

// ISR stub table defined in Assembly (see isr.asm or interrupt.asm)
extern void *isr_stub_table[ISR_STUB_TABLE_LIMIT];

// lidt loader (could also be inline asm here)
extern void load_idt(struct IDTR *idtr);

// ---------------------------------------------------------------------------
//  Global IDT and IDTR definition
// ---------------------------------------------------------------------------

struct IDTGate idt[IDT_MAX_ENTRY_COUNT];
struct IDTR _idt_idtr;

// ---------------------------------------------------------------------------
//  Helper: Set one entry in IDT
// ---------------------------------------------------------------------------

void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege) {
    uint32_t handler = (uint32_t) handler_address;
    struct IDTGate *gate = &idt[int_vector];

    gate->offset_low       = handler & 0xFFFF;
    gate->segment_selector = gdt_seg_selector;
    gate->zero             = 0;
    gate->offset_high      = (handler >> 16) & 0xFFFF;

    /*
        Type Attribute Layout (8 bits):
        Bit | Name        | Value
        ----|--------------|------
        7   | Present (P)  | 1
        6-5 | DPL          | privilege
        4   | 0            | 0
        3-0 | Gate Type    | 0xE (32-bit interrupt gate)

        So, 0x8E = 10001110b (P=1, DPL=00, Type=1110)
    */
    gate->type_attr = 0x80 | (privilege << 5) | 0x0E;
}

// ---------------------------------------------------------------------------
//  Initialize IDT and Load to CPU
// ---------------------------------------------------------------------------

void initialize_idt(void) {
    framebuffer_write(0, 4, 'A', 0xA, 0); // debug marker

    // Fill first 64 interrupt vectors with corresponding ISR stubs
    for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
        set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }

    framebuffer_write(0, 5, 'B', 0xA, 0); // debug marker

    // Setup IDTR structure
    _idt_idtr.base = (uint32_t) &idt;
    _idt_idtr.limit = (sizeof(struct IDTGate) * IDT_MAX_ENTRY_COUNT) - 1;

    // Load IDT into CPU
    __asm__ volatile("lidt (%0)" : : "r"(&_idt_idtr));

    framebuffer_write(0, 6, 'C', 0xA, 0); // debug marker
}
