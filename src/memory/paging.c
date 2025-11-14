#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
        [0x300] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
    }
};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0]                            = true,
        [1 ... PAGE_FRAME_MAX_COUNT-1] = false
    },
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1
};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr, 
    void *virtual_addr, 
    struct PageDirectoryEntryFlag flag
) {
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag          = flag;
    page_dir->table[page_index].lower_address = ((uint32_t) physical_addr >> 22) & 0x3FF;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr): "memory");
}



/* --- Memory Management --- */
// TODO: Implement
bool paging_allocate_check(uint32_t amount) {
    if (amount > page_manager_state.free_page_frame_count * PAGE_FRAME_SIZE) {
        return false;
    }
    return true;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    for (uint32_t i = 0; i < PAGE_FRAME_MAX_COUNT; i++) {
        if (!page_manager_state.page_frame_map[i]) {
            page_manager_state.page_frame_map[i] = true;
            page_manager_state.free_page_frame_count--;
            struct PageDirectoryEntryFlag flag = {
                .present_bit        = 1,
                .write_bit          = 1,
                .user_supervisor           = 1,
                .page_level_write_through  = 0,
                .page_level_cache_disable = 0,
                .accessed       = 0,
                .dirty          = 0,
                .use_pagesize_4_mb      = 1
            };
            update_page_directory_entry(page_dir, (void *)(i * PAGE_FRAME_SIZE), virtual_addr, flag);
            break;
        }
    }
    if (page_manager_state.free_page_frame_count == 0) {
        return false;
    }

    return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    uint32_t physical_frame = page_dir->table[page_index].lower_address << 22;
    page_manager_state.page_frame_map[physical_frame / PAGE_FRAME_SIZE] = false;
    page_manager_state.free_page_frame_count++;
    page_dir->table[page_index].flag.present_bit = 0;
    page_dir->table[page_index].flag.write_bit   = 0;
    page_dir->table[page_index].flag.user_supervisor    = 0;
    page_dir->table[page_index].flag.use_pagesize_4_mb = 0;
    page_dir->table[page_index].lower_address = 0;
    flush_single_tlb(virtual_addr);

    return true;
}

__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct {
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager = {
    .page_directory_used = {false},
};

struct PageDirectory* paging_create_new_page_directory(void) { 
    for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++) {
        if (!page_directory_manager.page_directory_used[i]) {
            page_directory_manager.page_directory_used[i] = true;
            struct PageDirectory *new_page_dir = &page_directory_list[i];
            *new_page_dir = _paging_kernel_page_directory;
            new_page_dir->table[0x300].flag.present_bit       = 1;
            new_page_dir->table[0x300].flag.write_bit         = 1;
            new_page_dir->table[0x300].flag.use_pagesize_4_mb = 1;
            new_page_dir->table[0x300].lower_address          = 0;
            return new_page_dir;
        }
    }
    return NULL; // No free page directory available
}

bool paging_free_page_directory(struct PageDirectory *page_dir) {
    for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++) {
        if (&page_directory_list[i] == page_dir) {
            page_directory_manager.page_directory_used[i] = false;
            for (int j = 0; j < PAGE_ENTRY_COUNT; j++) {
                page_dir->table[j].flag.present_bit = 0;
                page_dir->table[j].flag.write_bit   = 0;
                page_dir->table[j].flag.user_supervisor    = 0;
                page_dir->table[j].flag.use_pagesize_4_mb = 0;
                page_dir->table[j].lower_address = 0;
            }
            return true;
        }
    }
    return false; 
}


struct PageDirectory* paging_get_current_page_directory_addr(void) {
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr): /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory*) virtual_addr_page_dir;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr) {
    uint32_t physical_addr_page_dir = (uint32_t) page_dir_virtual_addr;
    // Additional layer of check & mistake safety net
    if ((uint32_t) page_dir_virtual_addr > KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__  volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir): "memory");
}