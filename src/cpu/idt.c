<<<<<<< HEAD
#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"

// IDT Table
struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT];

// IDTR
struct IDTR _idt_idtr = {
    .size = sizeof(interrupt_descriptor_table) - 1,
    .address = (uint32_t) &interrupt_descriptor_table
};

void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege) {
    struct IDTGate *idt_int_gate = &interrupt_descriptor_table[int_vector];
    
    // Set offset low (lower 16 bits)
    idt_int_gate->offset_low = (uint32_t) handler_address & 0xFFFF;
    
    // Set offset high (upper 16 bits)
    idt_int_gate->offset_high = ((uint32_t) handler_address >> 16) & 0xFFFF;
    
    // Set segment selector
    idt_int_gate->segment = gdt_seg_selector;
    
    // Set reserved bits
    idt_int_gate->_reserved = 0;
    idt_int_gate->_r_bit_1 = INTERRUPT_GATE_R_BIT_1;
    idt_int_gate->_r_bit_2 = INTERRUPT_GATE_R_BIT_2;
    idt_int_gate->_r_bit_3 = INTERRUPT_GATE_R_BIT_3;
    
    // Set gate as 32-bit
    idt_int_gate->gate_32 = 1;
    
    // Set privilege level (0 for kernel, 3 for user)
    idt_int_gate->privilege = privilege;
    
    // Set present bit (must be 1 for valid descriptor)
    idt_int_gate->valid_bit = 1;
}

void initialize_idt(void) {
    // Set all IDT gates using isr_stub_table
    for (uint8_t i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
        set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }
    
    // Load IDT using lidt instruction
    __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
    
    // Enable interrupt flag
    __asm__ volatile("sti");
}
=======
#include "../header/cpu/gdt.h"
#include "../header/cpu/interrupt.h"
#include "../header/cpu/idt.h"

struct InterruptDescriptorTable interrupt_descriptor_table;
struct IDTR _idt_idtr = {
    .size = sizeof(interrupt_descriptor_table) - 1,
    .address = &interrupt_descriptor_table
};

void initialize_idt(void) {
    /* 
     * TODO: 
     * Iterate all isr_stub_table,
     * Set all IDT entry with set_interrupt_gate()
     * with following values:
     * Vector: i
     * Handler Address: isr_stub_table[i]
     * Segment: GDT_KERNEL_CODE_SEGMENT_SELECTOR
     * Privilege: 0
     */
    for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
        set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }
    __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
    __asm__ volatile("sti");
}

void set_interrupt_gate(
    uint8_t  int_vector, 
    void     *handler_address, 
    uint16_t gdt_seg_selector, 
    uint8_t  privilege
) {
    struct IDTGate *idt_int_gate = &interrupt_descriptor_table.table[int_vector];
    // TODO : Set handler offset, privilege & segment
    // Use &-bitmask, bitshift, and casting for offset 
    uint32_t handler = (uint32_t) handler_address;
    idt_int_gate->offset_low  = handler & 0xFFFF;
    idt_int_gate->offset_high = (handler >> 16) & 0xFFFF;
    idt_int_gate->segment = gdt_seg_selector;
    idt_int_gate->_reserved = 0;

    // Target system 32-bit and flag this as valid interrupt gate
    idt_int_gate->_r_bit_1    = INTERRUPT_GATE_R_BIT_1;
    idt_int_gate->_r_bit_2    = INTERRUPT_GATE_R_BIT_2;
    idt_int_gate->_r_bit_3    = INTERRUPT_GATE_R_BIT_3;
    idt_int_gate->gate_32     = 1;
    idt_int_gate->privilege = privilege & 0x3;
    idt_int_gate->valid_bit   = 1;
}

>>>>>>> origin/feat/ch1
