#include <stdint.h>
#include <stdbool.h>
#include "../header/stdlib/string.h"

// Minimal syscall shim (same ABI as user-shell)
static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    __asm__ volatile("int $0x30");
}

int main(void) {
    bool running = false;
    syscall(13, 0, (uint32_t)&running, 0); // query clock status

    if (running) {
        const char *msg = "clock: already running\n";
        syscall(6, (uint32_t)msg, strlen(msg), 0x0F);
        return 0;
    }

    syscall(11, 0, 0, 0); // start clock
    const char *msg = "clock: started (bottom-right)\n";
    syscall(6, (uint32_t)msg, strlen(msg), 0x0F);
    return 0;
}
