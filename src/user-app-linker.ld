ENTRY(_start)

SECTIONS {
    . = 0x00200000; /* Load apps at 2 MiB to avoid shell region */
    
    .text ALIGN(4): {
        [a-z\/]*crt0_ret.o(.text)
        *(.text)
    }

    .data ALIGN(4): {
        *(.data)
    }

    .bss ALIGN(4): {
        *(COMMON)
        *(.bss)
    }

    .rodata ALIGN(4): {
        *(.rodata*)
    }

    _linker_user_program_end = .;
    ASSERT ((_linker_user_program_end <= (0x00200000 + 512 * 1024)), "Error: User app too big")
}
