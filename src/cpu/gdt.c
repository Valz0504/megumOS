#include "../header/cpu/gdt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {
            // TODO : Implement
            .segment_low = 0x0000,
            .base_low = 0x0000,
            .base_mid = 0x00,
            .type_bit = 0,
            .non_system = 0,
            .descriptor_privilege_level = 0,
            .segment_present = 0,
            .segment_limit_high = 0,
            .available = 0,
            .long_mode = 0,
            .default_operation_size = 0,
            .granularity = 0,
            .base_high = 0x00
        },
        {
            // TODO : Implement
            .segment_low = 0xFFFF,
            .base_low = 0x0000,
            .base_mid = 0x00,
            .type_bit = 0xA,  // Code segment: execute/read
            .non_system = 1,  // Code/data segment
            .descriptor_privilege_level = 0, // Ring 0
            .segment_present = 1,
            .segment_limit_high = 0xF,
            .available = 0,
            .long_mode = 0,
            .default_operation_size = 1, // 32-bit
            .granularity = 1, // 4KB blocks
            .base_high = 0x00  
        },
        {
            // TODO : Implement
            .segment_low = 0xFFFF,
            .base_low = 0x0000,
            .base_mid = 0x00,
            .type_bit = 0x2,  // Data segment: read/write
            .non_system = 1,  // Code/data segment
            .descriptor_privilege_level = 0,
            .segment_present = 1,
            .segment_limit_high = 0xF,
            .available = 0,
            .long_mode = 0,
            .default_operation_size = 1,
            .granularity = 1,
            .base_high = 0x00
        }
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
