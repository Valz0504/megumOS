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
            // Null Descriptor - All zeros
            .segment_low = 0,
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0,
            .non_system = 0,
            .descriptor_privilege_level = 0,
            .segment_present = 0,
            .segment_limit_high = 0,
            .available = 0,
            .long_mode = 0,
            .default_operation_size = 0,
            .granularity = 0,
            .base_high = 0
        },
        {
            // Kernel Code Descriptor
            .segment_low = 0xFFFF,          // Limit 0xFFFF (first 16 bits)
            .base_low = 0,                  // Base 0x00000000 (first 16 bits)
            .base_mid = 0,                  // Base (next 8 bits)
            .type_bit = 0xA,                // Code: Execute/Read
            .non_system = 1,                // Code/Data segment
            .descriptor_privilege_level = 0, // Ring 0 (kernel)
            .segment_present = 1,           // Present
            .segment_limit_high = 0xF,      // Limit (last 4 bits)
            .available = 0,                 // Available for system use
            .long_mode = 0,                 // 32-bit mode (not 64-bit)
            .default_operation_size = 1,    // 32-bit protected mode
            .granularity = 1,               // 4KB granularity
            .base_high = 0                  // Base (last 8 bits)
        },
        {
            // Kernel Data Descriptor
            .segment_low = 0xFFFF,          // Limit 0xFFFF (first 16 bits)
            .base_low = 0,                  // Base 0x00000000 (first 16 bits)
            .base_mid = 0,                  // Base (next 8 bits)
            .type_bit = 0x2,                // Data: Read/Write
            .non_system = 1,                // Code/Data segment
            .descriptor_privilege_level = 0, // Ring 0 (kernel)
            .segment_present = 1,           // Present
            .segment_limit_high = 0xF,      // Limit (last 4 bits)
            .available = 0,                 // Available for system use
            .long_mode = 0,                 // 32-bit mode (not 64-bit)
            .default_operation_size = 1,    // 32-bit protected mode
            .granularity = 1,               // 4KB granularity
            .base_high = 0                  // Base (last 8 bits)
        },
        {
            // User Code Descriptor
            .segment_low = 0xFFFF,          // Limit 0xFFFF (first 16 bits)
            .base_low = 0,                  // Base 0x00000000 (first 16 bits)
            .base_mid = 0,                  // Base (next 8 bits)
            .type_bit = 0xA,                // Code: Execute/Read
            .non_system = 1,                // Code/Data segment
            .descriptor_privilege_level = 3, // Ring 3 (user)
            .segment_present = 1,           // Present
            .segment_limit_high = 0xF,      // Limit (last 4 bits)
            .available = 0,                 // Available for system use
            .long_mode = 0,                 // 32-bit mode (not 64-bit)
            .default_operation_size = 1,    // 32-bit protected mode
            .granularity = 1,               // 4KB granularity
            .base_high = 0                  // Base (last 8 bits)
        },
        {
            // User Data Descriptor
            .segment_low = 0xFFFF,          // Limit 0xFFFF (first 16 bits)
            .base_low = 0,                  // Base 0x00000000 (first 16 bits)
            .base_mid = 0,                  // Base (next 8 bits)
            .type_bit = 0x2,                // Data: Read/Write
            .non_system = 1,                // Code/Data segment
            .descriptor_privilege_level = 3, // Ring 3 (user)
            .segment_present = 1,           // Present
            .segment_limit_high = 0xF,      // Limit (last 4 bits)
            .available = 0,                 // Available for system use
            .long_mode = 0,                 // 32-bit mode (not 64-bit)
            .default_operation_size = 1,    // 32-bit protected mode
            .granularity = 1,               // 4KB granularity
            .base_high = 0                  // Base (last 8 bits)
        },
        {
            // TSS Descriptor (Task State Segment)
            .segment_low = sizeof(struct TSSEntry),
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0x9,                // TSS Available
            .non_system = 0,                // System segment
            .descriptor_privilege_level = 0, // Ring 0
            .segment_present = 1,           // Present
            .segment_limit_high = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .available = 0,
            .long_mode = 0,
            .default_operation_size = 1,    // 32-bit TSS
            .granularity = 0,               // Byte granularity for TSS
            .base_high = 0
        },
        {0} // Null terminator
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

