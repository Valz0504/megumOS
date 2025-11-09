#include "../header/cpu/gdt.h"
#include "../header/cpu/interrupt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {
            // Null Descriptor
            .segment_low = 0,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0,
            .non_system = 0,
            .privilege = 0,
            .valid_bit = 0,
            .segment_high = 0,
            .available = 0,
            .long_mode = 0,
            .opr_32_bit = 0,
            .granularity = 0,
            .base_high = 0
        },
        {
            // Kernel Code Segment
            .segment_low = 0xFFFF,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0xA,
            .non_system = 1u,
            .privilege = 0u,
            .valid_bit = 1u,
            .segment_high = 0xF,
            .available = 0u,
            .long_mode = 0u,
            .opr_32_bit = 1u,
            .granularity = 1u,
            .base_high = 0
        },
        {
            // Kernel Data Segment
            .segment_low = 0xFFFF,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0x2,
            .non_system = 1u,
            .privilege = 0u,
            .valid_bit = 1u,
            .segment_high = 0xF,
            .available = 0u,
            .long_mode = 0u,
            .opr_32_bit = 1u,
            .granularity = 1u,
            .base_high = 0
        },
        {
            // User Code Segment
            .segment_low = 0xFFFF,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0xA,
            .non_system = 1u,
            .privilege = 3u,
            .valid_bit = 1u,
            .segment_high = 0xF,
            .available = 0u,
            .long_mode = 0u,
            .opr_32_bit = 1u,
            .granularity = 1u,
            .base_high = 0
        },
        {
            // User Data Segment
            .segment_low = 0xFFFF,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0x2,
            .non_system = 1u,
            .privilege = 3u,
            .valid_bit = 1u,
            .segment_high = 0xF,
            .available = 0u,
            .long_mode = 0u,
            .opr_32_bit = 1u,
            .granularity = 1u,
            .base_high = 0
        },
        {
            .segment_high      = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .segment_low       = sizeof(struct TSSEntry),
            .base_high         = 0,
            .base_mid          = 0,
            .base_low          = 0,
            .non_system        = 0,    // S bit
            .type_bit          = 0x9,
            .privilege         = 0,    // DPL
            .valid_bit         = 1,    // P bit
            .opr_32_bit        = 1,    // D/B bit
            .long_mode         = 0,    // L bit
            .granularity       = 0,    // G bit
        },
        {0}
    }
};

/**
 * _gdt_gdtr, predefined system GDTR. 
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    // TODO : Implement, this GDTR will point to global_descriptor_table. 
    //        Use sizeof operator
    .size = sizeof(global_descriptor_table) - 1,
    .address = &global_descriptor_table
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}

