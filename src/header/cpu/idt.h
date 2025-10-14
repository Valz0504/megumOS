#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// IDT hard limit, see Intel x86 manual 3a - 6.10 Interrupt Descriptor Table
#define IDT_MAX_ENTRY_COUNT    256
#define ISR_STUB_TABLE_LIMIT   64
#define INTERRUPT_GATE_R_BIT_1 0b000
#define INTERRUPT_GATE_R_BIT_2 0b110
#define INTERRUPT_GATE_R_BIT_3 0b0

// Interrupt Handler / ISR stub for reducing code duplication,
// this array can be iterated in initialize_idt()
extern void *isr_stub_table[ISR_STUB_TABLE_LIMIT];

/**
 * IDTGate, IDT entry that points into interrupt handler.
 * Defined according to Intel x86 Vol 3A, Figure 6-2: "IDT Gate Descriptor"
 *
 * Layout (8 bytes total):
 *  Bits | Field
 *  -----|-------------------------
 *  0–15 | Offset (low)
 * 16–31 | Segment Selector
 * 32–39 | Reserved (zero)
 * 40–47 | Type and Attributes (P, DPL, gate type, etc.)
 * 48–63 | Offset (high)
 */
struct IDTGate {
    uint16_t offset_low;        // bits 0–15 of handler address
    uint16_t segment_selector;  // code segment selector in GDT
    uint8_t  zero;              // reserved, must be zero
    uint8_t  type_attr;         // type and attribute flags
    uint16_t offset_high;       // bits 16–31 of handler address
} __attribute__((packed));

/**
 * IDTR structure used by lidt instruction.
 * Contains pointer and limit for the IDT table.
 */
struct IDTR {
    uint16_t limit;  // size of IDT in bytes - 1
    uint32_t base;   // linear address of IDT
} __attribute__((packed));

/**
 * Global IDT table (defined in idt.c)
 */
extern struct IDTGate idt[IDT_MAX_ENTRY_COUNT];

/**
 * Global IDTR register structure (defined in idt.c)
 */
extern struct IDTR _idt_idtr;


/**
 * Set one entry of IDT with the given handler.
 *
 * @param int_vector       Interrupt vector index (0–255)
 * @param handler_address  Address of interrupt handler
 * @param gdt_seg_selector GDT segment selector (use GDT_KERNEL_CODE_SEGMENT_SELECTOR)
 * @param privilege        Descriptor privilege level (0 = kernel, 3 = user)
 */
void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege);

/**
 * Initialize IDT table and load it into CPU using lidt.
 */
void initialize_idt(void);

#endif
