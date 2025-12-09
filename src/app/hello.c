#include <stdint.h>
#include "../header/stdlib/string.h"

// Minimal syscall wrapper (same convention as shell)
static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

int main(void)
{
    const char *msg = "Hello from external app\n";
    syscall(6, (uint32_t)msg, strlen(msg), 0x0F);
    return 0;
}
