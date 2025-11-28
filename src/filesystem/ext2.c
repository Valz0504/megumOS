#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

// Static superblock and BGD table for caching
static struct EXT2Superblock superblock;
static struct EXT2BlockGroupDescriptorTable bgd_table;

/* =================== HELPER FUNCTIONS ============================*/
// Forward declarations for inode helpers used before their definitions
void read_node(struct EXT2Inode *node, uint32_t inode);
void sync_node(struct EXT2Inode *node, uint32_t inode);
static bool is_ancestor(uint32_t ancestor, uint32_t node);
static bool allocate_one_block(uint32_t preferred_bgd, uint32_t *out_block);
static void free_one_block(uint32_t block_num);

char *get_entry_name(void *entry) {
    return (char *)((uint8_t *)entry + sizeof(struct EXT2DirectoryEntry));
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset) {
    return (struct EXT2DirectoryEntry *)((uint8_t *)ptr + offset);
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry) {
    return (struct EXT2DirectoryEntry *)((uint8_t *)entry + entry->rec_len);
}

uint16_t get_entry_record_len(uint8_t name_len) {
    uint16_t base_len = sizeof(struct EXT2DirectoryEntry) + name_len;
    return (base_len + 3) & ~3; // Align to 4 bytes
}

uint32_t get_dir_first_child_offset(void *ptr) {
    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)ptr;
    struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);
    return (uint32_t)((uint8_t *)get_next_directory_entry(dotdot) - (uint8_t *)ptr);
}

uint32_t inode_to_bgd(uint32_t inode) {
    return (inode - 1) / INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode) {
    return (inode - 1) % INODES_PER_GROUP;
}

static void update_parent_pointer(uint32_t dir_inode, uint32_t new_parent_inode) {
    struct EXT2Inode dir_node;
    read_node(&dir_node, dir_inode);

    if ((dir_node.i_mode & EXT2_S_IFDIR) == 0 ||
        dir_node.i_blocks == 0 ||
        dir_node.i_block[0] == 0) {
        return;
    }

    struct BlockBuffer dir_buf;
    read_blocks(&dir_buf, dir_node.i_block[0], 1);

    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)dir_buf.buf;
    struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);
    dotdot->inode = new_parent_inode;

    write_blocks(&dir_buf, dir_node.i_block[0], 1);
}

// Walk up parent chain to see if "ancestor" is in the path of "node"
static bool is_ancestor(uint32_t ancestor, uint32_t node) {
    uint32_t current = node;
    while (true) {
        if (current == ancestor) return true;
        if (current == 1) return false; // reached root

        struct EXT2Inode cur_node;
        read_node(&cur_node, current);
        if ((cur_node.i_mode & EXT2_S_IFDIR) == 0 || cur_node.i_blocks == 0 || cur_node.i_block[0] == 0) {
            return false;
        }

        struct BlockBuffer dir_buf;
        read_blocks(&dir_buf, cur_node.i_block[0], 1);
        struct EXT2DirectoryEntry *dot = (void *)dir_buf.buf;
        struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);

        if (dotdot->inode == current || dotdot->inode == 0) {
            return false; // malformed, bail
        }
        current = dotdot->inode;
    }
}

/* =================== INODE READ/WRITE HELPERS ============================*/

// Helper function to read a single inode without loading entire table
void read_node(struct EXT2Inode *node, uint32_t inode) {
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    
    // Calculate which block contains this inode
    uint32_t inode_block_offset = local_idx / INODES_PER_TABLE;
    uint32_t inode_in_block = local_idx % INODES_PER_TABLE;
    
    // Read only the block containing this inode
    struct BlockBuffer inode_block;
    uint32_t block_addr = bgd_table.table[bgd_idx].bg_inode_table + inode_block_offset;
    read_blocks(&inode_block, block_addr, 1);
    
    // Copy inode from the block
    struct EXT2Inode *inode_ptr = (struct EXT2Inode *)inode_block.buf;
    memcpy(node, &inode_ptr[inode_in_block], sizeof(struct EXT2Inode));
}

void sync_node(struct EXT2Inode *node, uint32_t inode) {
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    
    // Calculate which block contains this inode
    uint32_t inode_block_offset = local_idx / INODES_PER_TABLE;
    uint32_t inode_in_block = local_idx % INODES_PER_TABLE;
    
    // Read only the block containing this inode
    struct BlockBuffer inode_block;
    uint32_t block_addr = bgd_table.table[bgd_idx].bg_inode_table + inode_block_offset;
    read_blocks(&inode_block, block_addr, 1);
    
    // Update inode in the block
    struct EXT2Inode *inode_ptr = (struct EXT2Inode *)inode_block.buf;
    memcpy(&inode_ptr[inode_in_block], node, sizeof(struct EXT2Inode));
    
    // Write back only this block
    write_blocks(&inode_block, block_addr, 1);
}

/* =================== FILESYSTEM INITIALIZATION ============================*/

bool is_empty_storage(void) {
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return memcmp(boot_sector.buf, fs_signature, BLOCK_SIZE) != 0;
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode) {
    struct BlockBuffer dir_buf;
    memset(&dir_buf, 0, sizeof(struct BlockBuffer));
    
    // Create "." entry
    struct EXT2DirectoryEntry *dot = (struct EXT2DirectoryEntry *)dir_buf.buf;
    dot->inode = inode;
    dot->rec_len = get_entry_record_len(1);
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    char *dot_name = get_entry_name(dot);
    dot_name[0] = '.';
    
    // Create ".." entry
    struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);
    dotdot->inode = parent_inode;
    dotdot->rec_len = BLOCK_SIZE - dot->rec_len;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    char *dotdot_name = get_entry_name(dotdot);
    dotdot_name[0] = '.';
    dotdot_name[1] = '.';
    
    node->i_mode = EXT2_S_IFDIR;
    node->i_size = BLOCK_SIZE;
    node->i_blocks = 1;
    
    allocate_node_blocks(&dir_buf, node, inode_to_bgd(inode));
}

void create_ext2(void) {
    write_blocks(fs_signature, BOOT_SECTOR, 1);
    
    memset(&superblock, 0, sizeof(struct EXT2Superblock));
    superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
    superblock.s_blocks_count = DISK_SPACE / BLOCK_SIZE;
    superblock.s_free_blocks_count = superblock.s_blocks_count;
    superblock.s_free_inodes_count = superblock.s_inodes_count;
    superblock.s_first_data_block = 1;
    superblock.s_first_ino = 1;
    superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
    superblock.s_frags_per_group = BLOCKS_PER_GROUP;
    superblock.s_inodes_per_group = INODES_PER_GROUP;
    superblock.s_magic = EXT2_SUPER_MAGIC;
    superblock.s_prealloc_blocks = 0;
    superblock.s_prealloc_dir_blocks = 0;
    
    memset(&bgd_table, 0, sizeof(struct EXT2BlockGroupDescriptorTable));
    
    uint32_t current_block = 3;
    
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        bgd_table.table[i].bg_block_bitmap = current_block++;
        bgd_table.table[i].bg_inode_bitmap = current_block++;
        bgd_table.table[i].bg_inode_table = current_block;
        current_block += INODES_TABLE_BLOCK_COUNT;
        
        bgd_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP;
        bgd_table.table[i].bg_free_inodes_count = INODES_PER_GROUP;
        bgd_table.table[i].bg_used_dirs_count = 0;
        
        struct BlockBuffer block_bitmap;
        memset(&block_bitmap, 0, sizeof(struct BlockBuffer));
        
        if (i == 0) {
            uint32_t reserved_blocks = 3 + 2 + INODES_TABLE_BLOCK_COUNT;
            for (uint32_t b = 0; b < reserved_blocks && b < BLOCKS_PER_GROUP; b++) {
                uint32_t byte_idx = b / 8;
                uint32_t bit_idx = b % 8;
                if (byte_idx < BLOCK_SIZE) {
                    block_bitmap.buf[byte_idx] |= (1 << bit_idx);
                    bgd_table.table[i].bg_free_blocks_count--;
                    superblock.s_free_blocks_count--;
                }
            }
        }
        
        write_blocks(&block_bitmap, bgd_table.table[i].bg_block_bitmap, 1);
        
        struct BlockBuffer inode_bitmap;
        memset(&inode_bitmap, 0, sizeof(struct BlockBuffer));
        write_blocks(&inode_bitmap, bgd_table.table[i].bg_inode_bitmap, 1);
        
        // Initialize inode table blocks one by one
        struct BlockBuffer empty_block;
        memset(&empty_block, 0, sizeof(struct BlockBuffer));
        for (uint32_t j = 0; j < INODES_TABLE_BLOCK_COUNT; j++) {
            write_blocks(&empty_block, bgd_table.table[i].bg_inode_table + j, 1);
        }
    }
    
    write_blocks(&superblock, 1, 1);
    write_blocks(&bgd_table, 2, 1);
    
    struct EXT2Inode root_inode;
    memset(&root_inode, 0, sizeof(struct EXT2Inode));
    
    uint32_t root_inode_num = allocate_node();
    init_directory_table(&root_inode, root_inode_num, root_inode_num);
    
    uint32_t root_bgd = inode_to_bgd(root_inode_num);
    bgd_table.table[root_bgd].bg_used_dirs_count++;
    
    sync_node(&root_inode, root_inode_num);
    
    write_blocks(&bgd_table, 2, 1);
    write_blocks(&superblock, 1, 1);
}

void initialize_filesystem_ext2(void) {
    if (is_empty_storage()) {
        create_ext2();
    } else {
        read_blocks(&superblock, 1, 1);
        read_blocks(&bgd_table, 2, 1);
    }
}

bool is_directory_empty(uint32_t inode) {
    struct EXT2Inode node;
    read_node(&node, inode);

    if (node.i_blocks == 0)
        return true;

    struct BlockBuffer buf;
    read_blocks(&buf, node.i_block[0], 1);

    uint32_t offset = 0;

    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *e = (struct EXT2DirectoryEntry *)(buf.buf + offset);

        if (e->rec_len == 0) break;
        if (e->inode != 0) {
            char *name = (char *)(buf.buf + offset + sizeof(*e));

            // SKIP "." DAN ".."
            if (e->name_len == 1 && name[0] == '.') {
                offset += e->rec_len;
                continue;
            }
            if (e->name_len == 2 && name[0] == '.' && name[1] == '.') {
                offset += e->rec_len;
                continue;
            }

            // Entry valid selain "." & ".."
            return false;
        }

        offset += e->rec_len;
    }

    // Tidak ada entry selain "." & ".."
    return true;
}


/* =================== MEMORY MANAGEMENT ============================*/

uint32_t allocate_node(void) {
    for (uint32_t bgd_idx = 0; bgd_idx < GROUPS_COUNT; bgd_idx++) {
        if (bgd_table.table[bgd_idx].bg_free_inodes_count == 0) {
            continue;
        }
        
        struct BlockBuffer inode_bitmap;
        read_blocks(&inode_bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
        
        // Search all available inodes (not just 64)
        // But limit to what fits in one bitmap block (512 bytes = 4096 bits)
        uint32_t max_inodes = INODES_PER_GROUP;
        if (max_inodes > BLOCK_SIZE * 8) {
            max_inodes = BLOCK_SIZE * 8; // 4096 inodes max per bitmap block
        }
        
        for (uint32_t i = 0; i < max_inodes; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = i % 8;
            
            // Safety check
            if (byte_idx >= BLOCK_SIZE) {
                break;
            }
            
            if ((inode_bitmap.buf[byte_idx] & (1 << bit_idx)) == 0) {
                // Mark as used
                inode_bitmap.buf[byte_idx] |= (1 << bit_idx);
                write_blocks(&inode_bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
                
                // Update BGD
                bgd_table.table[bgd_idx].bg_free_inodes_count--;
                superblock.s_free_inodes_count--;
                write_blocks(&bgd_table, 2, 1);
                write_blocks(&superblock, 1, 1);
                
                // Return inode number (1-indexed)
                return bgd_idx * INODES_PER_GROUP + i + 1;
            }
        }
    }
    
    return 0;
}

void deallocate_node(uint32_t inode) {
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t local_idx = inode_to_local(inode);
    
    struct EXT2Inode node;
    read_node(&node, inode);
    
    // Free direct blocks
    uint32_t direct_count = (node.i_blocks > 12) ? 12 : node.i_blocks;
    for (uint32_t i = 0; i < direct_count; i++) {
        if (node.i_block[i] != 0) {
            free_one_block(node.i_block[i]);
            node.i_block[i] = 0;
        }
    }

    // Free indirect blocks if present
    if (node.i_blocks > 12 && node.i_block[12] != 0) {
        struct BlockBuffer indirect_buf;
        read_blocks(&indirect_buf, node.i_block[12], 1);

        uint32_t extra = node.i_blocks - 12;
        uint32_t *entries = (uint32_t *)indirect_buf.buf;
        for (uint32_t i = 0; i < extra; i++) {
            if (entries[i] != 0) {
                free_one_block(entries[i]);
            }
        }

        // Free the indirect block itself
        free_one_block(node.i_block[12]);
        node.i_block[12] = 0;
    }
    
    memset(&node, 0, sizeof(struct EXT2Inode));
    sync_node(&node, inode);
    
    struct BlockBuffer inode_bitmap;
    read_blocks(&inode_bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
    
    uint32_t byte_idx = local_idx / 8;
    uint32_t bit_idx = local_idx % 8;
    inode_bitmap.buf[byte_idx] &= ~(1 << bit_idx);
    
    write_blocks(&inode_bitmap, bgd_table.table[bgd_idx].bg_inode_bitmap, 1);
    
    bgd_table.table[bgd_idx].bg_free_inodes_count++;
    superblock.s_free_inodes_count++;
    
    write_blocks(&bgd_table, 2, 1);
    write_blocks(&superblock, 1, 1);
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *locations = (uint32_t *)loc;
    struct BlockBuffer bitmap;
    uint32_t last_bgd = GROUPS_COUNT;
    bool bgd_loaded = false;
    
    deallocate_block(locations, blocks, &bitmap, 0, &last_bgd, bgd_loaded);
}

uint32_t deallocate_block(uint32_t *locations, uint32_t blocks, struct BlockBuffer *bitmap, 
                          uint32_t depth, uint32_t *last_bgd, bool bgd_loaded) {
    uint32_t blocks_per_indirect = BLOCK_SIZE / sizeof(uint32_t);
    
    for (uint32_t i = 0; i < blocks && i < 12 && locations[i] != 0; i++) {
        if (depth > 0) {
            struct BlockBuffer indirect_buf;
            read_blocks(&indirect_buf, locations[i], 1);
            
            uint32_t sub_blocks = (depth == 1) ? blocks - i : blocks_per_indirect;
            *last_bgd = deallocate_block((uint32_t *)indirect_buf.buf, sub_blocks, 
                                        bitmap, depth - 1, last_bgd, bgd_loaded);
        }
        
        uint32_t block_num = locations[i];
        uint32_t bgd_idx = block_num / BLOCKS_PER_GROUP;
        uint32_t local_block = block_num % BLOCKS_PER_GROUP;
        
        if (!bgd_loaded || bgd_idx != *last_bgd) {
            if (bgd_loaded) {
                write_blocks(bitmap, bgd_table.table[*last_bgd].bg_block_bitmap, 1);
            }
            read_blocks(bitmap, bgd_table.table[bgd_idx].bg_block_bitmap, 1);
            *last_bgd = bgd_idx;
            bgd_loaded = true;
        }
        
        uint32_t byte_idx = local_block / 8;
        uint32_t bit_idx = local_block % 8;
        if (byte_idx < BLOCK_SIZE) {
            bitmap->buf[byte_idx] &= ~(1 << bit_idx);
        }
        
        bgd_table.table[bgd_idx].bg_free_blocks_count++;
        superblock.s_free_blocks_count++;
    }
    
    if (bgd_loaded) {
        write_blocks(bitmap, bgd_table.table[*last_bgd].bg_block_bitmap, 1);
        write_blocks(&bgd_table, 2, 1);
        write_blocks(&superblock, 1, 1);
    }
    
    return *last_bgd;
}

static bool allocate_one_block(uint32_t preferred_bgd, uint32_t *out_block) {
    if (preferred_bgd >= GROUPS_COUNT) preferred_bgd = 0;

    for (uint32_t pass = 0; pass < 2; pass++) {
        for (uint32_t bgd_idx = (pass == 0 ? preferred_bgd : 0);
             bgd_idx < GROUPS_COUNT;
             bgd_idx++) {
            if (pass == 1 && bgd_idx == preferred_bgd) continue;
            if (bgd_table.table[bgd_idx].bg_free_blocks_count == 0) continue;

            struct BlockBuffer block_bitmap;
            read_blocks(&block_bitmap, bgd_table.table[bgd_idx].bg_block_bitmap, 1);

            uint32_t max_blocks = (BLOCKS_PER_GROUP > 4096) ? 4096 : BLOCKS_PER_GROUP;
            for (uint32_t j = 0; j < max_blocks; j++) {
                uint32_t byte_idx = j / 8;
                uint32_t bit_idx = j % 8;
                if (byte_idx >= BLOCK_SIZE) break;
                if ((block_bitmap.buf[byte_idx] & (1 << bit_idx)) == 0) {
                    uint32_t block_num = bgd_idx * BLOCKS_PER_GROUP + j;
                    block_bitmap.buf[byte_idx] |= (1 << bit_idx);
                    write_blocks(&block_bitmap, bgd_table.table[bgd_idx].bg_block_bitmap, 1);
                    bgd_table.table[bgd_idx].bg_free_blocks_count--;
                    superblock.s_free_blocks_count--;
                    write_blocks(&bgd_table, 2, 1);
                    write_blocks(&superblock, 1, 1);
                    *out_block = block_num;
                    return true;
                }
            }
        }
    }
    return false;
}

static void free_one_block(uint32_t block_num) {
    uint32_t bgd_idx = block_num / BLOCKS_PER_GROUP;
    uint32_t local_block = block_num % BLOCKS_PER_GROUP;
    if (bgd_idx >= GROUPS_COUNT) return;

    struct BlockBuffer bitmap;
    read_blocks(&bitmap, bgd_table.table[bgd_idx].bg_block_bitmap, 1);

    uint32_t byte_idx = local_block / 8;
    uint32_t bit_idx = local_block % 8;
    if (byte_idx < BLOCK_SIZE && (bitmap.buf[byte_idx] & (1 << bit_idx))) {
        bitmap.buf[byte_idx] &= ~(1 << bit_idx);
        write_blocks(&bitmap, bgd_table.table[bgd_idx].bg_block_bitmap, 1);
        bgd_table.table[bgd_idx].bg_free_blocks_count++;
        superblock.s_free_blocks_count++;
        write_blocks(&bgd_table, 2, 1);
        write_blocks(&superblock, 1, 1);
    }
}

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t preferred_bgd) {
    uint32_t blocks_needed = node->i_blocks;
    uint32_t blocks_allocated = 0;
    uint8_t *data = (uint8_t *)ptr;
    uint32_t indirect_block = 0;
    uint32_t indirect_entries[BLOCK_SIZE / sizeof(uint32_t)];
    memset(indirect_entries, 0, sizeof(indirect_entries));

    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (i >= 12 && indirect_block == 0) {
            if (!allocate_one_block(preferred_bgd, &indirect_block)) {
                break;
            }
            node->i_block[12] = indirect_block;
        }

        uint32_t block_num = 0;
        if (!allocate_one_block(preferred_bgd, &block_num)) {
            break;
        }

        if (i < 12) {
            node->i_block[i] = block_num;
        } else {
            indirect_entries[i - 12] = block_num;
        }

        struct BlockBuffer data_block;
        memset(&data_block, 0, sizeof(struct BlockBuffer));

        if (data != NULL && blocks_allocated * BLOCK_SIZE < node->i_size) {
            uint32_t copy_size = BLOCK_SIZE;
            if (blocks_allocated * BLOCK_SIZE + BLOCK_SIZE > node->i_size) {
                copy_size = node->i_size - blocks_allocated * BLOCK_SIZE;
            }
            memcpy(data_block.buf, data + blocks_allocated * BLOCK_SIZE, copy_size);
        }
        write_blocks(&data_block, block_num, 1);

        blocks_allocated++;
    }

    // Write indirect block if used
    if (indirect_block != 0) {
        struct BlockBuffer indirect_buf;
        memset(&indirect_buf, 0, sizeof(struct BlockBuffer));
        memcpy(indirect_buf.buf, indirect_entries, sizeof(indirect_entries));
        write_blocks(&indirect_buf, indirect_block, 1);
    }

    // Adjust i_blocks/i_size if we allocated fewer than requested
    if (blocks_allocated < blocks_needed) {
        node->i_blocks = blocks_allocated;
        if (node->i_size > blocks_allocated * BLOCK_SIZE) {
            node->i_size = blocks_allocated * BLOCK_SIZE;
        }
    }
}

/* =================== CRUD OPERATIONS ============================*/

int8_t read_directory(struct EXT2DriverRequest *request) {
    if (request->parent_inode == 0 || request->parent_inode > superblock.s_inodes_count)
        return 3;

    struct EXT2Inode parent_node;
    read_node(&parent_node, request->parent_inode);

    if ((parent_node.i_mode & EXT2_S_IFDIR) == 0)
        return 3;

    // Case: name = ".", return current directory contents
    if (request->name_len == 1 && request->name[0] == '.') {
        read_blocks(request->buf, parent_node.i_block[0], 1);
        return 0;
    }

    // Case: name = ".."
    if (request->name_len == 2 && request->name[0] == '.' && request->name[1] == '.') {
        // find parent inode via ".." entry
        struct BlockBuffer dir_buf;
        read_blocks(&dir_buf, parent_node.i_block[0], 1);

        struct EXT2DirectoryEntry *dot = (void*)dir_buf.buf;
        struct EXT2DirectoryEntry *dotdot = get_next_directory_entry(dot);

        uint32_t parent_inode = dotdot->inode;

        struct EXT2Inode pnode;
        read_node(&pnode, parent_inode);

        read_blocks(request->buf, pnode.i_block[0], 1);
        return 0;
    }

    // Case: reading named directory inside current directory
    struct BlockBuffer dir_buf;
    read_blocks(&dir_buf, parent_node.i_block[0], 1);

    uint32_t offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf.buf, offset);

        if (entry->rec_len == 0) break;
        if (entry->inode == 0) break;

        char *entry_name = get_entry_name(entry);

        if (entry->name_len == request->name_len &&
            memcmp(entry_name, request->name, request->name_len) == 0) {

            struct EXT2Inode node;
            read_node(&node, entry->inode);

            if (entry->file_type != EXT2_FT_DIR)
                return 1;

            read_blocks(request->buf, node.i_block[0], 1);
            return 0;
        }

        offset += entry->rec_len;
    }

    return 2;
}


int8_t read(struct EXT2DriverRequest request) {
    static struct BlockBuffer dir_buf;
    static struct BlockBuffer data_block;
    static struct BlockBuffer indirect_block;
    
    if (request.parent_inode == 0 || request.parent_inode > superblock.s_inodes_count) {
        return 4;
    }
    
    struct EXT2Inode parent_node;
    read_node(&parent_node, request.parent_inode);
    
    if ((parent_node.i_mode & EXT2_S_IFDIR) == 0) {
        return 4;
    }
    
    read_blocks(&dir_buf, parent_node.i_block[0], 1);
    
    uint32_t offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf.buf, offset);
        
        if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) {
            break;
        }
        
        if (entry->inode == 0) {
            break;
        }
        
        char *entry_name = get_entry_name(entry);
        
        if (entry->name_len == request.name_len && 
            memcmp(entry_name, request.name, request.name_len) == 0) {
            
            if (entry->file_type != EXT2_FT_REG_FILE) {
                return 1;
            }
            
            struct EXT2Inode file_node;
            read_node(&file_node, entry->inode);
            
            if (request.buffer_size < file_node.i_size) {
                return 2;
            }
            
            uint8_t *buffer = (uint8_t *)request.buf;
            uint32_t bytes_read = 0;
            
            uint32_t blocks_to_read = file_node.i_blocks;
            uint32_t block_index = 0;

            // Direct blocks
            for (; block_index < blocks_to_read && block_index < 12; block_index++) {
                read_blocks(&data_block, file_node.i_block[block_index], 1);
                
                uint32_t bytes_to_copy = (file_node.i_size - bytes_read > BLOCK_SIZE) ? 
                                         BLOCK_SIZE : (file_node.i_size - bytes_read);
                
                memcpy(buffer + bytes_read, data_block.buf, bytes_to_copy);
                bytes_read += bytes_to_copy;
                if (bytes_read >= file_node.i_size) break;
            }

            // Single indirect
            if (bytes_read < file_node.i_size && block_index < blocks_to_read && block_index >= 12 && file_node.i_block[12] != 0) {
                read_blocks(&indirect_block, file_node.i_block[12], 1);
                uint32_t *entries = (uint32_t *)indirect_block.buf;
                uint32_t indirect_idx = 0;
                while (block_index < blocks_to_read && indirect_idx < BLOCK_SIZE / sizeof(uint32_t)) {
                    uint32_t data_block_num = entries[indirect_idx];
                    if (data_block_num == 0) break;
                    read_blocks(&data_block, data_block_num, 1);
                    uint32_t bytes_to_copy = (file_node.i_size - bytes_read > BLOCK_SIZE) ? 
                                             BLOCK_SIZE : (file_node.i_size - bytes_read);
                    memcpy(buffer + bytes_read, data_block.buf, bytes_to_copy);
                    bytes_read += bytes_to_copy;
                    if (bytes_read >= file_node.i_size) break;
                    block_index++;
                    indirect_idx++;
                }
            }
            
            // Add null terminator at the end
            if (bytes_read < request.buffer_size) {
                buffer[bytes_read] = '\0';
            }
            
            return 0;
        }
        
        offset += entry->rec_len;
    }
    
    return 3;
}

int8_t write(struct EXT2DriverRequest *request) {
    if (request->parent_inode == 0 || request->parent_inode > superblock.s_inodes_count) {
        return 2;
    }
    
    struct EXT2Inode parent_node;
    read_node(&parent_node, request->parent_inode);
    
    if ((parent_node.i_mode & EXT2_S_IFDIR) == 0) {
        return 2;
    }
    
    // Check if parent has blocks allocated
    if (parent_node.i_blocks == 0 || parent_node.i_block[0] == 0) {
        return 2; // Parent folder not properly initialized
    }
    
    struct BlockBuffer dir_buf;
    read_blocks(&dir_buf, parent_node.i_block[0], 1);
    
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *last_entry = NULL;
    
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf.buf, offset);
        
        if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) {
            break;
        }
        
        if (entry->inode == 0) {
            break;
        }
        
        char *entry_name = get_entry_name(entry);
        
        if (entry->name_len == request->name_len && 
            memcmp(entry_name, request->name, request->name_len) == 0) {
            return 1;
        }
        
        last_entry = entry;
        offset += entry->rec_len;
    }
    
    uint32_t new_inode = allocate_node();
    if (new_inode == 0) {
        return -1;
    }
    
    if (last_entry != NULL) {
        uint16_t actual_last_len = get_entry_record_len(last_entry->name_len);
        last_entry->rec_len = actual_last_len;
        offset = (uint32_t)((uint8_t *)last_entry - dir_buf.buf) + actual_last_len;
    }
    
    struct EXT2DirectoryEntry *new_entry = get_directory_entry(dir_buf.buf, offset);
    new_entry->inode = new_inode;
    new_entry->rec_len = BLOCK_SIZE - offset;
    new_entry->name_len = request->name_len;
    new_entry->file_type = request->is_directory ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    
    char *new_name = get_entry_name(new_entry);
    memcpy(new_name, request->name, request->name_len);
    
    write_blocks(&dir_buf, parent_node.i_block[0], 1);
    
    struct EXT2Inode new_node;
    memset(&new_node, 0, sizeof(struct EXT2Inode));
    
    if (request->is_directory) {
        init_directory_table(&new_node, new_inode, request->parent_inode);
        bgd_table.table[inode_to_bgd(new_inode)].bg_used_dirs_count++;
    } else {
        new_node.i_mode = EXT2_S_IFREG;
        new_node.i_size = request->buffer_size;
        new_node.i_blocks = (request->buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        if (request->buffer_size > 0) {
            allocate_node_blocks(request->buf, &new_node, inode_to_bgd(new_inode));
        }
    }
    
    sync_node(&new_node, new_inode);
    
    write_blocks(&bgd_table, 2, 1);
    
    return 0;
}

int8_t delete(struct EXT2DriverRequest request) {
    if (request.parent_inode == 0 || request.parent_inode > superblock.s_inodes_count) {
        return 3;   // invalid parent
    }
    
    struct EXT2Inode parent_node;
    read_node(&parent_node, request.parent_inode);
    
    if ((parent_node.i_mode & EXT2_S_IFDIR) == 0) {
        return 3;   // parent bukan directory
    }
    
    struct BlockBuffer dir_buf;
    read_blocks(&dir_buf, parent_node.i_block[0], 1);
    
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *prev_entry = NULL;
    
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf.buf, offset);
        
        if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) {
            break;
        }
        
        if (entry->inode == 0) {
            break;
        }
        
        char *entry_name = get_entry_name(entry);
        
        if (entry->name_len == request.name_len && 
            memcmp(entry_name, request.name, request.name_len) == 0) {
            
            // Kalau targetnya DIRECTORY
            if (entry->file_type == EXT2_FT_DIR) {

                if (request.is_directory) {
                    // MODE REKURSIF: hapus semua anak dulu

                    struct EXT2Inode dir_inode;
                    read_node(&dir_inode, entry->inode);

                    if (dir_inode.i_blocks > 0 && dir_inode.i_block[0] != 0) {
                        struct BlockBuffer child_buf;

                        bool has_child = true;
                        while (has_child) {
                            has_child = false;

                            read_blocks(&child_buf, dir_inode.i_block[0], 1);
                            uint32_t child_offset = 0;

                            while (child_offset < BLOCK_SIZE) {
                                struct EXT2DirectoryEntry *child =
                                    get_directory_entry(child_buf.buf, child_offset);

                                if (child->rec_len == 0 ||
                                    child->rec_len > BLOCK_SIZE - child_offset)
                                    break;

                                if (child->inode == 0) {
                                    child_offset += child->rec_len;
                                    continue;
                                }

                                char *child_name = get_entry_name(child);

                                // skip "." dan ".."
                                if ((child->name_len == 1 && child_name[0] == '.') ||
                                    (child->name_len == 2 && child_name[0] == '.' && child_name[1] == '.')) {
                                    child_offset += child->rec_len;
                                    continue;
                                }

                                // Delete anak secara rekursif
                                struct EXT2DriverRequest child_req = {
                                    .buf = NULL,
                                    .name = child_name,
                                    .name_len = child->name_len,
                                    .parent_inode = entry->inode,
                                    .buffer_size = 0,
                                    .is_directory = true     // recursive
                                };

                                int8_t child_ret = delete(child_req);
                                // Kalau gagal, bisa di-handle, tapi untuk sekarang kita abaikan
                                (void) child_ret;

                                // setelah delete, ulang scan dari awal
                                has_child = true;
                                break;
                            }
                        }
                    }
                } else {
                    // MODE BIASA: directory harus kosong
                    if (!is_directory_empty(entry->inode)) {
                        return 2;   // directory not empty
                    }
                }

                // Di sini directory sudah kosong (baik dari awal, atau setelah recursive delete)
                bgd_table.table[inode_to_bgd(entry->inode)].bg_used_dirs_count--;
            }
            
            // Hapus inode (baik file maupun directory)
            deallocate_node(entry->inode);
            
            // Hapus entry di parent directory
            if (prev_entry != NULL) {
                prev_entry->rec_len += entry->rec_len;
            } else {
                entry->inode = 0;
            }
            
            write_blocks(&dir_buf, parent_node.i_block[0], 1);
            
            write_blocks(&bgd_table, 2, 1);
            
            return 0;
        }
        
        prev_entry = entry;
        offset += entry->rec_len;
    }
    
    return 1;   // entry tidak ditemukan
}

int8_t move(struct EXT2MoveRequest *request) {
    // Validate parents
    if (request->src_parent_inode == 0 || request->dst_parent_inode == 0 ||
        request->src_parent_inode > superblock.s_inodes_count ||
        request->dst_parent_inode > superblock.s_inodes_count) {
        return 3;
    }

    // No-op rename
    if (request->src_parent_inode == request->dst_parent_inode &&
        request->src_name_len == request->dst_name_len &&
        memcmp(request->src_name, request->dst_name, request->src_name_len) == 0) {
        return 0;
    }

    // Disallow targeting special names
    if ((request->dst_name_len == 1 && request->dst_name[0] == '.') ||
        (request->dst_name_len == 2 && request->dst_name[0] == '.' && request->dst_name[1] == '.')) {
        return 2;
    }

    struct EXT2Inode src_parent_node, dst_parent_node;
    read_node(&src_parent_node, request->src_parent_inode);
    read_node(&dst_parent_node, request->dst_parent_inode);

    if ((src_parent_node.i_mode & EXT2_S_IFDIR) == 0 ||
        (dst_parent_node.i_mode & EXT2_S_IFDIR) == 0 ||
        src_parent_node.i_blocks == 0 || dst_parent_node.i_blocks == 0) {
        return 3;
    }

    struct BlockBuffer src_buf;
    read_blocks(&src_buf, src_parent_node.i_block[0], 1);

    // Find source entry
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *src_entry = NULL;
    struct EXT2DirectoryEntry *src_prev = NULL;

    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(src_buf.buf, offset);

        if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) break;
        if (entry->inode == 0) break;

        char *entry_name = get_entry_name(entry);

        // Disallow moving special entries
        if ((entry->name_len == 1 && entry_name[0] == '.') ||
            (entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
            // Skip special entries
            offset += entry->rec_len;
            continue;
        }

        if (entry->name_len == request->src_name_len &&
            memcmp(entry_name, request->src_name, request->src_name_len) == 0) {
            src_entry = entry;
            break;
        }

        src_prev = entry;
        offset += entry->rec_len;
    }

    if (src_entry == NULL) {
        return 1;   // source not found
    }

    // Prepare destination buffer and ensure name not taken
    struct BlockBuffer dst_buf;
    read_blocks(&dst_buf, dst_parent_node.i_block[0], 1);

    struct EXT2DirectoryEntry *dst_last = NULL;
    uint16_t dst_last_actual_len = 0;
    uint32_t insert_offset = 0;
    offset = 0;
    while (offset < BLOCK_SIZE) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dst_buf.buf, offset);

        if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) break;
        if (entry->inode == 0) break;

        char *entry_name = get_entry_name(entry);
        if (entry->name_len == request->dst_name_len &&
            memcmp(entry_name, request->dst_name, request->dst_name_len) == 0) {
            return 2;   // destination exists
        }

        dst_last = entry;
        offset += entry->rec_len;
    }

    uint16_t min_required = get_entry_record_len(request->dst_name_len);
    if (dst_last != NULL) {
        dst_last_actual_len = get_entry_record_len(dst_last->name_len);
        insert_offset = (uint32_t)((uint8_t *)dst_last - dst_buf.buf) + dst_last_actual_len;
    } else {
        insert_offset = 0;
    }

    if (insert_offset + min_required > BLOCK_SIZE) {
        return 4;   // no space
    }

    uint32_t moving_inode = src_entry->inode;
    uint8_t moving_type = src_entry->file_type;

    // Reject attempts to move special entries (e.g., ".." or ".")
    if (src_prev == NULL) {
        return 1;
    }

    // Prevent moving a directory into itself or its descendants
    if (moving_type == EXT2_FT_DIR &&
        (request->dst_parent_inode == moving_inode ||
         is_ancestor(moving_inode, request->dst_parent_inode))) {
        return 2; // treat as destination exists/invalid
    }

    // Remove from source directory
    if (src_prev != NULL) {
        src_prev->rec_len += src_entry->rec_len;
    } else {
        src_entry->inode = 0;
    }
    write_blocks(&src_buf, src_parent_node.i_block[0], 1);

    // If moving within same directory, re-read destination buffer after removal
    if (request->src_parent_inode == request->dst_parent_inode) {
        read_blocks(&dst_buf, dst_parent_node.i_block[0], 1);
        dst_last = NULL;
        offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = get_directory_entry(dst_buf.buf, offset);

            if (entry->rec_len == 0 || entry->rec_len > BLOCK_SIZE - offset) break;
            if (entry->inode == 0) break;

            char *entry_name = get_entry_name(entry);
            if (entry->name_len == request->dst_name_len &&
                memcmp(entry_name, request->dst_name, request->dst_name_len) == 0) {
                return 2;
            }

            dst_last = entry;
            offset += entry->rec_len;
        }

        dst_last_actual_len = (dst_last != NULL) ? get_entry_record_len(dst_last->name_len) : 0;
        insert_offset = (dst_last != NULL) ?
            (uint32_t)((uint8_t *)dst_last - dst_buf.buf) + dst_last_actual_len : 0;

        if (insert_offset + min_required > BLOCK_SIZE) {
            return 4;
        }
    }

    if (dst_last != NULL) {
        dst_last->rec_len = dst_last_actual_len;
    } else {
        insert_offset = 0;
    }

    struct EXT2DirectoryEntry *new_entry = get_directory_entry(dst_buf.buf, insert_offset);
    new_entry->inode = moving_inode;
    new_entry->rec_len = BLOCK_SIZE - insert_offset;
    new_entry->name_len = request->dst_name_len;
    new_entry->file_type = moving_type;
    memcpy(get_entry_name(new_entry), request->dst_name, request->dst_name_len);

    write_blocks(&dst_buf, dst_parent_node.i_block[0], 1);

    // Update ".." pointer if moving directory across parents
    if (moving_type == EXT2_FT_DIR &&
        request->src_parent_inode != request->dst_parent_inode) {
        update_parent_pointer(moving_inode, request->dst_parent_inode);
    }

    return 0;
}
