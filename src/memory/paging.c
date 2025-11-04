#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../header/memory/paging.h"

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
bool paging_allocate_check(uint32_t amount) {
    // Calculate how many page frames are needed for the requested amount
    uint32_t page_frame_count = (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
    
    // Check if we have enough free page frames
    return page_manager_state.free_page_frame_count >= page_frame_count;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    // Find a free physical frame
    uint32_t physical_frame_idx = 0;
    bool found = false;
    
    for (uint32_t i = 0; i < PAGE_FRAME_MAX_COUNT; i++) {
        if (!page_manager_state.page_frame_map[i]) {
            physical_frame_idx = i;
            found = true;
            break;
        }
    }
    
    // No free frame available
    if (!found) {
        return false;
    }
    
    // Mark the frame as used
    page_manager_state.page_frame_map[physical_frame_idx] = true;
    page_manager_state.free_page_frame_count--;
    
    // Calculate physical address from frame index
    void *physical_addr = (void *)(physical_frame_idx * PAGE_FRAME_SIZE);
    
    // Set up the flags for user page
    struct PageDirectoryEntryFlag flag = {
        .present_bit = 1,
        .write_bit = 1,
        .user_supervisor = 1,
        .use_pagesize_4_mb = 1
    };
    
    // Update the page directory entry
    update_page_directory_entry(page_dir, physical_addr, virtual_addr, flag);
    
    return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    // Get the page directory index from virtual address
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    
    // Check if the page is present
    if (!page_dir->table[page_index].flag.present_bit) {
        return false; // Page is not allocated
    }
    
    // Get the physical frame index from the page directory entry
    uint32_t physical_frame_idx = page_dir->table[page_index].lower_address;
    
    // Check if the frame index is valid
    if (physical_frame_idx >= PAGE_FRAME_MAX_COUNT) {
        return false;
    }
    
    // Free the physical frame
    page_manager_state.page_frame_map[physical_frame_idx] = false;
    page_manager_state.free_page_frame_count++;
    
    // Clear the page directory entry (set all to 0)
    page_dir->table[page_index] = (struct PageDirectoryEntry) {0};
    
    // Flush the TLB for this virtual address
    flush_single_tlb(virtual_addr);
    
    return true;
}

