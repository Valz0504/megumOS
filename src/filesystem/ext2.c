#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

static struct EXT2Superblock superblock;
struct EXT2BlockGroupDescriptorTable bgdt;

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/**
 *  REGULAR function
 */
char *get_entry_name(void *entry) {
    struct EXT2DirectoryEntry *dir_entry = (struct EXT2DirectoryEntry *)entry;
    return (char *)(dir_entry + 1);
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset) {
    return (struct EXT2DirectoryEntry *)((char *)ptr + offset);
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry) {
    return (struct EXT2DirectoryEntry *)((char *)entry + entry->rec_len);
}

uint16_t get_entry_record_len(uint8_t name_len) {
    uint16_t base_len = sizeof(struct EXT2DirectoryEntry) + name_len;
    return (base_len + 3) & ~3;
}

uint32_t get_dir_first_child_offset(void *ptr) {
    struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)ptr;
    
    entry = get_next_directory_entry(entry);
    uint32_t offset = (char *)entry - (char *)ptr + entry->rec_len;
    return offset;
}

/* =================== MAIN FUNCTION OF EXT32 FILESYSTEM ============================*/

uint32_t inode_to_bgd(uint32_t inode) {
    return (inode - 1) / INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode) {
    return (inode - 1) % INODES_PER_GROUP;
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode) {
    struct BlockBuffer dir_data;
    memset(&dir_data, 0, BLOCK_SIZE);
    
    struct EXT2DirectoryEntry *entry = get_directory_entry(dir_data.buf, 0);
    entry->inode = inode;
    entry->rec_len = get_entry_record_len(1);
    entry->name_len = 1;
    entry->file_type = EXT2_FT_DIR;
    char *name = get_entry_name(entry);
    name[0] = '.';
    
    entry = get_next_directory_entry(entry);
    entry->inode = parent_inode;
    entry->rec_len = BLOCK_SIZE - get_entry_record_len(1);
    entry->name_len = 2;
    entry->file_type = EXT2_FT_DIR;
    name = get_entry_name(entry);
    name[0] = '.';
    name[1] = '.';
    
    write_blocks(node->i_block[0], 1, &dir_data);
}

void create_ext2(void) {
    write_blocks(BOOT_SECTOR, 1, fs_signature);

    superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
    superblock.s_blocks_count = BLOCKS_PER_GROUP * GROUPS_COUNT;
    superblock.s_r_blocks_count = 0;
    superblock.s_free_blocks_count = superblock.s_blocks_count - 3;
    superblock.s_free_inodes_count = superblock.s_inodes_count - 1;
    superblock.s_first_data_block = 1;
    superblock.s_first_ino = 1;
    superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
    superblock.s_frags_per_group = BLOCKS_PER_GROUP;
    superblock.s_inodes_per_group = INODES_PER_GROUP;
    superblock.s_magic = EXT2_SUPER_MAGIC;
    superblock.s_prealloc_blocks = 0;
    superblock.s_prealloc_dir_blocks = 0;

    write_blocks(1, 1, &superblock);

    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        uint32_t base = i * BLOCKS_PER_GROUP;
        bgdt.table[i].bg_block_bitmap = 3 + base;
        bgdt.table[i].bg_inode_bitmap = 4 + base;
        bgdt.table[i].bg_inode_table = 5 + base;
        bgdt.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - (5 + INODES_TABLE_BLOCK_COUNT);
        bgdt.table[i].bg_free_inodes_count = INODES_PER_GROUP;
        bgdt.table[i].bg_used_dirs_count = 0;
        bgdt.table[i].bg_pad = 0;
        memset(bgdt.table[i].bg_reserved, 0, sizeof(bgdt.table[i].bg_reserved));
    }
    bgdt.table[0].bg_free_blocks_count -= 3;

    struct BlockBuffer bitmap_buffer;
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        memset(&bitmap_buffer, 0, BLOCK_SIZE);
        if (i == 0) {
            bitmap_buffer.buf[0] = 0xFF;
        }
        write_blocks(bgdt.table[i].bg_block_bitmap, 1, &bitmap_buffer);

        memset(&bitmap_buffer, 0, BLOCK_SIZE);
        if (i == 0) {
            bitmap_buffer.buf[0] = 0x01;
        }
        write_blocks(bgdt.table[i].bg_inode_bitmap, 1, &bitmap_buffer);
    }

    struct EXT2Inode root;
    memset(&root, 0, sizeof(struct EXT2Inode));
    root.i_mode = EXT2_S_IFDIR;
    root.i_size = BLOCK_SIZE;
    root.i_blocks = 1;
    root.i_block[0] = 21; 

    read_blocks(&bitmap_buffer, bgdt.table[0].bg_block_bitmap, 1);
    uint32_t block_num = 21;
    bitmap_buffer.buf[block_num / 8] |= (1 << (block_num % 8));
    write_blocks(bgdt.table[0].bg_block_bitmap, 1, &bitmap_buffer);

    bgdt.table[0].bg_free_blocks_count--;
    bgdt.table[0].bg_free_inodes_count--;
    bgdt.table[0].bg_used_dirs_count = 1;
    write_blocks(2, 1, &bgdt);

    superblock.s_free_blocks_count--;
    superblock.s_free_inodes_count--;
    write_blocks(1, 1, &superblock);

    init_directory_table(&root, 1, 1);
    
    struct EXT2InodeTable inode_tbl;
    memset(&inode_tbl, 0, sizeof(struct EXT2InodeTable));
    inode_tbl.table[0] = root;
    write_blocks(bgdt.table[0].bg_inode_table, 1, &inode_tbl);
}

bool is_empty_storage(void) {
    struct BlockBuffer boot_data;
    read_blocks(&boot_data, BOOT_SECTOR, 1);
    
    return memcmp(boot_data.buf, fs_signature, BLOCK_SIZE) != 0;
}

void initialize_filesystem_ext2(void) {
    if (is_empty_storage()) {
        create_ext2();
    } else {
        read_blocks(&superblock, 1, 1);
        read_blocks(&bgdt, 2, 1);
    }
}

bool is_directory_empty(uint32_t inode) {
    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t local_index = inode_to_local(inode);
    
    struct EXT2InodeTable inode_table;
    uint32_t inode_table_block = bgdt.table[bgd_index].bg_inode_table;
    
    uint32_t block_offset = local_index / INODES_PER_TABLE;
    uint32_t table_index = local_index % INODES_PER_TABLE;
    
    read_blocks(&inode_table, inode_table_block + block_offset, 1);
    struct EXT2Inode *dir_inode = &inode_table.table[table_index];
    
    struct BlockBuffer dir_data;
    read_blocks(&dir_data, dir_inode->i_block[0], 1);
    
    uint32_t first_child_offset = get_dir_first_child_offset(dir_data.buf);
    
    struct EXT2DirectoryEntry *first_child_entry = get_directory_entry(dir_data.buf, first_child_offset);
    
    return first_child_entry->inode == 0;
}

/* =============================== CRUD FUNC ======================================== */

int8_t read(struct EXT2DriverRequest request) {
    // Validasi input dasar
    if (request.buf == NULL || request.name == NULL || request.name_len == 0 || 
        request.parent_inode == 0 || request.parent_inode > superblock.s_inodes_count) {
        return -1; // Error: parameter invalid
    }
    
    // 1. Validasi parent inode (harus directory)
    uint32_t parent_bgd = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    
    // Validasi bgd
    if (parent_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    // Baca parent inode table
    struct EXT2InodeTable parent_inode_table;
    uint32_t parent_inode_table_block = bgdt.table[parent_bgd].bg_inode_table;
    uint32_t parent_block_offset = parent_local / INODES_PER_TABLE;
    uint32_t parent_table_index = parent_local % INODES_PER_TABLE;
    
    read_blocks(&parent_inode_table, parent_inode_table_block + parent_block_offset, 1);
    struct EXT2Inode *parent_inode = &parent_inode_table.table[parent_table_index];
    
    // Cek apakah parent adalah directory
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 4; // Parent folder invalid
    }
    
    // Cek apakah parent memiliki blocks
    if (parent_inode->i_blocks == 0 || parent_inode->i_block[0] == 0) {
        return 4; // Parent folder invalid
    }
    
    // 2. Cari file dengan nama request.name dalam parent directory
    struct BlockBuffer dir_data;
    read_blocks(&dir_data, parent_inode->i_block[0], 1);
    
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry = NULL;
    bool found = false;
    
    // Loop melalui semua entry dalam directory
    while (offset < BLOCK_SIZE) {
        entry = get_directory_entry(dir_data.buf, offset);
        
        // Jika inode 0, entry tidak digunakan, lanjut
        if (entry->inode == 0) {
            break;
        }
        
        // Dapatkan nama entry
        char *entry_name = get_entry_name(entry);
        
        // Bandingkan nama
        if (entry->name_len == request.name_len && 
            memcmp(entry_name, request.name, request.name_len) == 0) {
            found = true;
            break;
        }
        
        offset += entry->rec_len;
    }
    
    // Jika file tidak ditemukan
    if (!found) {
        return 3; // File tidak ditemukan
    }
    
    // 3. Cek apakah entry adalah file (bukan directory)
    if (entry->file_type != EXT2_FT_REG_FILE) {
        return 1; // Bukan sebuah file
    }
    
    // 4. Baca inode dari file yang ditemukan
    uint32_t file_inode_num = entry->inode;
    
    // Validasi file inode
    if (file_inode_num == 0 || file_inode_num > superblock.s_inodes_count) {
        return -1; // Error: inode number invalid
    }
    
    uint32_t file_bgd = inode_to_bgd(file_inode_num);
    uint32_t file_local = inode_to_local(file_inode_num);
    
    // Validasi bgd
    if (file_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    struct EXT2InodeTable file_inode_table;
    uint32_t file_inode_table_block = bgdt.table[file_bgd].bg_inode_table;
    uint32_t file_block_offset = file_local / INODES_PER_TABLE;
    uint32_t file_table_index = file_local % INODES_PER_TABLE;
    
    read_blocks(&file_inode_table, file_inode_table_block + file_block_offset, 1);
    struct EXT2Inode *file_inode = &file_inode_table.table[file_table_index];
    
    // Validasi inode mode
    if (!(file_inode->i_mode & EXT2_S_IFREG)) {
        return -1; // Error: inode corrupted (filetype tidak match)
    }
    
    // 5. Cek apakah buffer cukup untuk menampung file
    if (request.buffer_size < file_inode->i_size) {
        return 2; // Buffer tidak cukup
    }
    
    // 6. Baca semua blok file dan salin ke buffer
    uint32_t bytes_read = 0;
    uint32_t bytes_remaining = file_inode->i_size;
    uint32_t blocks_to_read = file_inode->i_blocks;
    
    struct BlockBuffer block_buf;
    
    // Baca direct blocks (maksimal 12 blok pertama)
    for (uint32_t i = 0; i < 12 && i < blocks_to_read && bytes_remaining > 0; i++) {
        read_blocks(&block_buf, file_inode->i_block[i], 1);
        
        uint32_t bytes_to_copy = (bytes_remaining > BLOCK_SIZE) ? BLOCK_SIZE : bytes_remaining;
        memcpy((uint8_t *)request.buf + bytes_read, block_buf.buf, bytes_to_copy);
        
        bytes_read += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;
    }
    
    // Jika masih ada blok yang perlu dibaca, gunakan indirect blocks
    if (blocks_to_read > 12 && bytes_remaining > 0) {
        // Single indirect block (blok ke-13)
        struct BlockBuffer indirect_buf;
        read_blocks(&indirect_buf, file_inode->i_block[12], 1);
        uint32_t *indirect_blocks = (uint32_t *)indirect_buf.buf;
        
        uint32_t indirect_count = blocks_to_read - 12;
        if (indirect_count > (BLOCK_SIZE / sizeof(uint32_t))) {
            indirect_count = BLOCK_SIZE / sizeof(uint32_t);
        }
        
        for (uint32_t i = 0; i < indirect_count && bytes_remaining > 0; i++) {
            read_blocks(&block_buf, indirect_blocks[i], 1);
            
            uint32_t bytes_to_copy = (bytes_remaining > BLOCK_SIZE) ? BLOCK_SIZE : bytes_remaining;
            memcpy((uint8_t *)request.buf + bytes_read, block_buf.buf, bytes_to_copy);
            
            bytes_read += bytes_to_copy;
            bytes_remaining -= bytes_to_copy;
        }
    }
    
    // Jika masih ada blok yang perlu dibaca, gunakan doubly indirect blocks
    uint32_t blocks_in_single_indirect = BLOCK_SIZE / sizeof(uint32_t);
    if (blocks_to_read > 12 + blocks_in_single_indirect && bytes_remaining > 0) {
        // Double indirect block (blok ke-14)
        struct BlockBuffer double_indirect_buf;
        read_blocks(&double_indirect_buf, file_inode->i_block[13], 1);
        uint32_t *double_indirect_blocks = (uint32_t *)double_indirect_buf.buf;
        
        uint32_t blocks_processed = 12 + blocks_in_single_indirect;
        uint32_t double_indirect_count = blocks_to_read - blocks_processed;
        
        for (uint32_t i = 0; i < (BLOCK_SIZE / sizeof(uint32_t)) && bytes_remaining > 0; i++) {
            struct BlockBuffer indirect_buf;
            read_blocks(&indirect_buf, double_indirect_blocks[i], 1);
            uint32_t *indirect_blocks = (uint32_t *)indirect_buf.buf;
            
            for (uint32_t j = 0; j < (BLOCK_SIZE / sizeof(uint32_t)) && bytes_remaining > 0; j++) {
                read_blocks(&block_buf, indirect_blocks[j], 1);
                
                uint32_t bytes_to_copy = (bytes_remaining > BLOCK_SIZE) ? BLOCK_SIZE : bytes_remaining;
                memcpy((uint8_t *)request.buf + bytes_read, block_buf.buf, bytes_to_copy);
                
                bytes_read += bytes_to_copy;
                bytes_remaining -= bytes_to_copy;
            }
        }
    }
    
    return 0; // Operasi berhasil
}

int8_t read_directory(struct EXT2DriverRequest *prequest) {
    // Validasi input dasar
    if (prequest == NULL || prequest->buf == NULL || prequest->name == NULL || 
        prequest->name_len == 0 || prequest->parent_inode == 0 || 
        prequest->parent_inode > superblock.s_inodes_count) {
        return -1; // Error: parameter invalid
    }
    
    // 1. Validasi parent inode (harus directory)
    uint32_t parent_bgd = inode_to_bgd(prequest->parent_inode);
    uint32_t parent_local = inode_to_local(prequest->parent_inode);
    
    // Validasi bgd
    if (parent_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    // Baca parent inode table
    struct EXT2InodeTable parent_inode_table;
    uint32_t parent_inode_table_block = bgdt.table[parent_bgd].bg_inode_table;
    uint32_t parent_block_offset = parent_local / INODES_PER_TABLE;
    uint32_t parent_table_index = parent_local % INODES_PER_TABLE;
    
    read_blocks(&parent_inode_table, parent_inode_table_block + parent_block_offset, 1);
    struct EXT2Inode *parent_inode = &parent_inode_table.table[parent_table_index];
    
    // Cek apakah parent adalah directory
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 3; // Parent folder invalid
    }
    
    // Cek apakah parent memiliki blocks
    if (parent_inode->i_blocks == 0 || parent_inode->i_block[0] == 0) {
        return 3; // Parent folder invalid
    }
    
    // 2. Cari directory dengan nama prequest->name dalam parent directory
    struct BlockBuffer dir_data;
    read_blocks(&dir_data, parent_inode->i_block[0], 1);
    
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry = NULL;
    bool found = false;
    
    // Loop melalui semua entry dalam directory
    while (offset < BLOCK_SIZE) {
        entry = get_directory_entry(dir_data.buf, offset);
        
        // Jika inode 0, entry tidak digunakan, lanjut
        if (entry->inode == 0) {
            break;
        }
        
        // Dapatkan nama entry
        char *entry_name = get_entry_name(entry);
        
        // Bandingkan nama
        if (entry->name_len == prequest->name_len && 
            memcmp(entry_name, prequest->name, prequest->name_len) == 0) {
            found = true;
            break;
        }
        
        offset += entry->rec_len;
    }
    
    // Jika directory tidak ditemukan
    if (!found) {
        return 2; // Folder tidak ditemukan
    }
    
    // 3. Cek apakah entry adalah directory (bukan file)
    if (entry->file_type != EXT2_FT_DIR) {
        return 1; // Bukan sebuah folder
    }
    
    // 4. Baca inode dari directory yang ditemukan
    uint32_t dir_inode_num = entry->inode;
    
    // Validasi dir inode
    if (dir_inode_num == 0 || dir_inode_num > superblock.s_inodes_count) {
        return -1; // Error: inode number invalid
    }
    
    uint32_t dir_bgd = inode_to_bgd(dir_inode_num);
    uint32_t dir_local = inode_to_local(dir_inode_num);
    
    // Validasi bgd
    if (dir_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    struct EXT2InodeTable dir_inode_table;
    uint32_t dir_inode_table_block = bgdt.table[dir_bgd].bg_inode_table;
    uint32_t dir_block_offset = dir_local / INODES_PER_TABLE;
    uint32_t dir_table_index = dir_local % INODES_PER_TABLE;
    
    read_blocks(&dir_inode_table, dir_inode_table_block + dir_block_offset, 1);
    struct EXT2Inode *dir_inode = &dir_inode_table.table[dir_table_index];
    
    // Validasi inode mode
    if (!(dir_inode->i_mode & EXT2_S_IFDIR)) {
        return -1; // Error: inode corrupted (filetype tidak match)
    }
    
    // Validasi dir memiliki blocks
    if (dir_inode->i_blocks == 0 || dir_inode->i_block[0] == 0) {
        return -1; // Error: directory corrupted (tidak ada blocks)
    }
    
    // 5. Baca directory table dan salin ke buffer
    struct BlockBuffer dir_content;
    read_blocks(&dir_content, dir_inode->i_block[0], 1);
    
    memcpy(prequest->buf, dir_content.buf, BLOCK_SIZE);
    
    return 0; // Operasi berhasil
}

int8_t write(struct EXT2DriverRequest *request) {
    // Validasi input dasar
    if (request == NULL || request->name == NULL || request->name_len == 0 || 
        request->parent_inode == 0 || request->parent_inode > superblock.s_inodes_count) {
        return -1; // Error: parameter invalid
    }
    
    // Validasi buffer untuk file (jika buffer_size > 0, buf tidak boleh NULL)
    if (request->buffer_size > 0 && request->buf == NULL) {
        return -1; // Error: buf NULL padahal buffer_size > 0
    }
    
    // 1. Validasi parent inode (harus directory)
    uint32_t parent_bgd = inode_to_bgd(request->parent_inode);
    uint32_t parent_local = inode_to_local(request->parent_inode);
    
    // Validasi bgd
    if (parent_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    // Baca parent inode table
    struct EXT2InodeTable parent_inode_table;
    uint32_t parent_inode_table_block = bgdt.table[parent_bgd].bg_inode_table;
    uint32_t parent_block_offset = parent_local / INODES_PER_TABLE;
    uint32_t parent_table_index = parent_local % INODES_PER_TABLE;
    
    read_blocks(&parent_inode_table, parent_inode_table_block + parent_block_offset, 1);
    struct EXT2Inode *parent_inode = &parent_inode_table.table[parent_table_index];
    
    // Cek apakah parent adalah directory
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 2; // Parent folder invalid
    }
    
    // Cek apakah parent memiliki datablock
    if (parent_inode->i_blocks == 0 || parent_inode->i_block[0] == 0) {
        return 2; // Parent folder invalid (tidak memiliki datablock)
    }
    
    // 2. Cek apakah nama sudah digunakan di parent directory
    struct BlockBuffer dir_data;
    read_blocks(&dir_data, parent_inode->i_block[0], 1);
    
    uint32_t offset = 0;
    struct EXT2DirectoryEntry *entry = NULL;
    struct EXT2DirectoryEntry *last_entry = NULL;
    
    // Loop melalui semua entry dalam directory
    while (offset < BLOCK_SIZE) {
        entry = get_directory_entry(dir_data.buf, offset);
        
        // Jika inode 0, entry tidak digunakan, ini tempat kosong
        if (entry->inode == 0) {
            break;
        }
        
        // Dapatkan nama entry
        char *entry_name = get_entry_name(entry);
        
        // Bandingkan nama
        if (entry->name_len == request->name_len && 
            memcmp(entry_name, request->name, request->name_len) == 0) {
            return 1; // Entry dengan nama sudah ada
        }
        
        last_entry = entry;
        offset += entry->rec_len;
    }
    
    // 3. Alokasi inode baru untuk file/folder
    uint32_t new_inode_num = allocate_node();
    
    // Validasi alokasi berhasil
    if (new_inode_num == 0) {
        return -1; // Error: tidak bisa allocate inode (disk penuh atau error lain)
    }
    
    // 4. Setup inode baru
    uint32_t new_bgd = inode_to_bgd(new_inode_num);
    uint32_t new_local = inode_to_local(new_inode_num);
    
    // Validasi bgd
    if (new_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    struct EXT2InodeTable new_inode_table;
    uint32_t new_inode_table_block = bgdt.table[new_bgd].bg_inode_table;
    uint32_t new_block_offset = new_local / INODES_PER_TABLE;
    uint32_t new_table_index = new_local % INODES_PER_TABLE;
    
    read_blocks(&new_inode_table, new_inode_table_block + new_block_offset, 1);
    struct EXT2Inode *new_inode = &new_inode_table.table[new_table_index];
    
    // Clear inode
    memset(new_inode, 0, sizeof(struct EXT2Inode));
    
    // 5. Tulis data (jika file) atau init directory table (jika folder)
    if (request->buffer_size == 0) {
        // Ini adalah folder
        new_inode->i_mode = EXT2_S_IFDIR;
        new_inode->i_size = BLOCK_SIZE;
        new_inode->i_blocks = 1;
        
        // Alokasi satu block untuk directory table
        allocate_node_blocks(NULL, new_inode, new_bgd);
        
        // Init directory table
        init_directory_table(new_inode, new_inode_num, request->parent_inode);
        
        // Update bgdt used_dirs_count
        read_blocks(&bgdt, 2, 1);
        bgdt.table[new_bgd].bg_used_dirs_count++;
        write_blocks(2, 1, &bgdt);
    } else {
        // Ini adalah file
        new_inode->i_mode = EXT2_S_IFREG;
        new_inode->i_size = request->buffer_size;
        
        // Hitung jumlah block yang dibutuhkan
        uint32_t blocks_needed = (request->buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        new_inode->i_blocks = blocks_needed;
        
        // Alokasi blocks dan tulis data
        allocate_node_blocks(request->buf, new_inode, new_bgd);
    }
    
    // Sync inode ke disk
    sync_node(new_inode, new_inode_num);
    
    // 6. Tambahkan entry baru ke parent directory
    // Hitung ukuran entry baru
    uint16_t new_entry_len = get_entry_record_len(request->name_len);
    
    // Jika last_entry ada, adjust rec_len-nya
    if (last_entry != NULL) {
        // Hitung actual size dari last_entry
        uint16_t last_entry_actual_size = get_entry_record_len(last_entry->name_len);
        uint16_t available_space = last_entry->rec_len - last_entry_actual_size;
        
        if (available_space >= new_entry_len) {
            // Ada cukup ruang setelah last_entry
            last_entry->rec_len = last_entry_actual_size;
            offset = (char *)last_entry - (char *)dir_data.buf + last_entry_actual_size;
        } else {
            // Tidak cukup ruang, gunakan space di akhir
            offset = (char *)last_entry - (char *)dir_data.buf + last_entry->rec_len;
        }
    } else {
        // Directory kosong (selain . dan ..)
        offset = get_dir_first_child_offset(dir_data.buf);
    }
    
    // Setup entry baru
    entry = get_directory_entry(dir_data.buf, offset);
    entry->inode = new_inode_num;
    entry->rec_len = BLOCK_SIZE - offset;
    entry->name_len = request->name_len;
    entry->file_type = (request->buffer_size == 0) ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    
    // Salin nama
    char *entry_name = get_entry_name(entry);
    memcpy(entry_name, request->name, request->name_len);
    
    // Tulis kembali directory table ke disk
    write_blocks(parent_inode->i_block[0], 1, &dir_data);
    
    return 0; // Operasi berhasil
}

int8_t delete(struct EXT2DriverRequest request) {
    // Validasi input dasar
    if (request.name == NULL || request.name_len == 0 || 
        request.parent_inode == 0 || request.parent_inode > superblock.s_inodes_count) {
        return -1; // Error: parameter invalid
    }
    
    // 1. Validasi parent inode (harus directory)
    uint32_t parent_bgd = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    
    // Validasi bgd
    if (parent_bgd >= GROUPS_COUNT) {
        return -1; // Error: bgd out of bounds
    }
    
    // Baca parent inode table
    struct EXT2InodeTable parent_inode_table;
    uint32_t parent_inode_table_block = bgdt.table[parent_bgd].bg_inode_table;
    uint32_t parent_block_offset = parent_local / INODES_PER_TABLE;
    uint32_t parent_table_index = parent_local % INODES_PER_TABLE;
    
    read_blocks(&parent_inode_table, parent_inode_table_block + parent_block_offset, 1);
    struct EXT2Inode *parent_inode = &parent_inode_table.table[parent_table_index];
    
    // Cek apakah parent adalah directory
    if (!(parent_inode->i_mode & EXT2_S_IFDIR)) {
        return 3; // Parent folder invalid
    }
    
    // Cek apakah parent memiliki blocks
    if (parent_inode->i_blocks == 0 || parent_inode->i_block[0] == 0) {
        return 3; // Parent folder invalid
    }
    
    // 2. Cari entry dengan nama request.name dalam parent directory
    struct BlockBuffer dir_data;
    read_blocks(&dir_data, parent_inode->i_block[0], 1);
    
    uint32_t offset = 0;
    uint32_t prev_offset = 0;
    struct EXT2DirectoryEntry *entry = NULL;
    struct EXT2DirectoryEntry *prev_entry = NULL;
    bool found = false;
    
    // Loop melalui semua entry dalam directory
    while (offset < BLOCK_SIZE) {
        entry = get_directory_entry(dir_data.buf, offset);
        
        // Jika inode 0, entry tidak digunakan, lanjut
        if (entry->inode == 0) {
            break;
        }
        
        // Dapatkan nama entry
        char *entry_name = get_entry_name(entry);
        
        // Bandingkan nama
        if (entry->name_len == request.name_len && 
            memcmp(entry_name, request.name, request.name_len) == 0) {
            found = true;
            break;
        }
        
        prev_entry = entry;
        prev_offset = offset;
        offset += entry->rec_len;
    }
    
    // Jika entry tidak ditemukan
    if (!found) {
        return 1; // Entry tidak ditemukan
    }
    
    // 3. Jika entry adalah directory, cek apakah kosong
    if (entry->file_type == EXT2_FT_DIR) {
        // Validasi entry inode
        if (entry->inode == 0 || entry->inode > superblock.s_inodes_count) {
            return -1; // Error: inode number invalid
        }
        
        if (!is_directory_empty(entry->inode)) {
            return 2; // Folder yang akan dihapus tidak kosong
        }
        
        // Update bgdt used_dirs_count
        uint32_t entry_bgd = inode_to_bgd(entry->inode);
        
        // Validasi bgd
        if (entry_bgd >= GROUPS_COUNT) {
            return -1; // Error: bgd out of bounds
        }
        
        read_blocks(&bgdt, 2, 1);
        bgdt.table[entry_bgd].bg_used_dirs_count--;
        write_blocks(2, 1, &bgdt);
    } else {
        // Validasi entry inode untuk file juga
        if (entry->inode == 0 || entry->inode > superblock.s_inodes_count) {
            return -1; // Error: inode number invalid
        }
    }
    
    // 4. Dealokasi inode dan blocks
    deallocate_node(entry->inode);
    
    // 5. Hapus entry dari directory table
    // Jika ada entry sebelumnya, merge rec_len
    if (prev_entry != NULL) {
        prev_entry->rec_len += entry->rec_len;
    } else {
        // Entry pertama (setelah . dan ..), set inode ke 0
        entry->inode = 0;
    }
    
    // Tulis kembali directory table ke disk
    write_blocks(parent_inode->i_block[0], 1, &dir_data);
    
    return 0; // Operasi berhasil
}

/* =============================== MEMORY ==========================================*/

uint32_t allocate_node(void) {
    // Baca superblock dan bgdt
    read_blocks(&superblock, 1, 1);
    read_blocks(&bgdt, 2, 1);
    
    // Cari group dengan inode kosong
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        if (bgdt.table[i].bg_free_inodes_count > 0) {
            // Baca inode bitmap
            struct BlockBuffer bitmap;
            read_blocks(&bitmap, bgdt.table[i].bg_inode_bitmap, 1);
            
            // Cari bit kosong (0) dalam bitmap
            for (uint32_t byte_idx = 0; byte_idx < BLOCK_SIZE; byte_idx++) {
                if (bitmap.buf[byte_idx] != 0xFF) {
                    // Ada bit kosong dalam byte ini
                    for (uint32_t bit_idx = 0; bit_idx < 8; bit_idx++) {
                        if (!(bitmap.buf[byte_idx] & (1 << bit_idx))) {
                            // Bit ini kosong, alokasikan
                            bitmap.buf[byte_idx] |= (1 << bit_idx);
                            
                            // Tulis kembali bitmap
                            write_blocks(bgdt.table[i].bg_inode_bitmap, 1, &bitmap);
                            
                            // Update bgdt
                            bgdt.table[i].bg_free_inodes_count--;
                            write_blocks(2, 1, &bgdt);
                            
                            // Update superblock
                            superblock.s_free_inodes_count--;
                            write_blocks(1, 1, &superblock);
                            
                            // Hitung nomor inode (1-based)
                            uint32_t local_inode = byte_idx * 8 + bit_idx;
                            uint32_t inode_num = i * INODES_PER_GROUP + local_inode + 1;
                            
                            return inode_num;
                        }
                    }
                }
            }
        }
    }
    
    // Tidak ada inode kosong (seharusnya tidak terjadi)
    return 0;
}

void deallocate_node(uint32_t inode) {
    // Baca inode untuk mendapatkan informasi blocks
    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t local_index = inode_to_local(inode);
    
    struct EXT2InodeTable inode_table;
    uint32_t inode_table_block = bgdt.table[bgd_index].bg_inode_table;
    uint32_t block_offset = local_index / INODES_PER_TABLE;
    uint32_t table_index = local_index % INODES_PER_TABLE;
    
    read_blocks(&inode_table, inode_table_block + block_offset, 1);
    struct EXT2Inode *node = &inode_table.table[table_index];
    
    // Dealokasi semua blocks yang digunakan
    if (node->i_blocks > 0) {
        deallocate_blocks(node->i_block, node->i_blocks);
    }
    
    // Clear inode
    memset(node, 0, sizeof(struct EXT2Inode));
    write_blocks(inode_table_block + block_offset, 1, &inode_table);
    
    // Update inode bitmap
    struct BlockBuffer bitmap;
    read_blocks(&bitmap, bgdt.table[bgd_index].bg_inode_bitmap, 1);
    
    uint32_t byte_idx = local_index / 8;
    uint32_t bit_idx = local_index % 8;
    bitmap.buf[byte_idx] &= ~(1 << bit_idx);
    
    write_blocks(bgdt.table[bgd_index].bg_inode_bitmap, 1, &bitmap);
    
    // Update bgdt
    read_blocks(&bgdt, 2, 1);
    bgdt.table[bgd_index].bg_free_inodes_count++;
    write_blocks(2, 1, &bgdt);
    
    // Update superblock
    read_blocks(&superblock, 1, 1);
    superblock.s_free_inodes_count++;
    write_blocks(1, 1, &superblock);
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *locations = (uint32_t *)loc;
    
    // Baca superblock dan bgdt
    read_blocks(&superblock, 1, 1);
    read_blocks(&bgdt, 2, 1);
    
    // Dealokasi direct blocks (maksimal 12 blok pertama)
    uint32_t direct_blocks = (blocks > 12) ? 12 : blocks;
    for (uint32_t i = 0; i < direct_blocks; i++) {
        uint32_t block_num = locations[i];
        if (block_num == 0) continue;
        
        // Tentukan block ada di group mana
        uint32_t bgd_index = block_num / BLOCKS_PER_GROUP;
        uint32_t local_block = block_num % BLOCKS_PER_GROUP;
        
        // Baca block bitmap
        struct BlockBuffer bitmap;
        read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
        
        // Clear bit
        uint32_t byte_idx = local_block / 8;
        uint32_t bit_idx = local_block % 8;
        bitmap.buf[byte_idx] &= ~(1 << bit_idx);
        
        write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
        
        // Update bgdt
        bgdt.table[bgd_index].bg_free_blocks_count++;
        
        // Update superblock
        superblock.s_free_blocks_count++;
    }
    
    // Dealokasi single indirect blocks (jika ada)
    if (blocks > 12) {
        uint32_t indirect_block = locations[12];
        if (indirect_block != 0) {
            struct BlockBuffer indirect_buf;
            read_blocks(&indirect_buf, indirect_block, 1);
            uint32_t *indirect_blocks = (uint32_t *)indirect_buf.buf;
            
            uint32_t indirect_count = blocks - 12;
            if (indirect_count > (BLOCK_SIZE / sizeof(uint32_t))) {
                indirect_count = BLOCK_SIZE / sizeof(uint32_t);
            }
            
            // Dealokasi blocks yang direferensi oleh indirect block
            for (uint32_t i = 0; i < indirect_count; i++) {
                uint32_t block_num = indirect_blocks[i];
                if (block_num == 0) continue;
                
                uint32_t bgd_index = block_num / BLOCKS_PER_GROUP;
                uint32_t local_block = block_num % BLOCKS_PER_GROUP;
                
                struct BlockBuffer bitmap;
                read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
                
                uint32_t byte_idx = local_block / 8;
                uint32_t bit_idx = local_block % 8;
                bitmap.buf[byte_idx] &= ~(1 << bit_idx);
                
                write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
                
                bgdt.table[bgd_index].bg_free_blocks_count++;
                superblock.s_free_blocks_count++;
            }
            
            // Dealokasi indirect block itu sendiri
            uint32_t bgd_index = indirect_block / BLOCKS_PER_GROUP;
            uint32_t local_block = indirect_block % BLOCKS_PER_GROUP;
            
            struct BlockBuffer bitmap;
            read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
            
            uint32_t byte_idx = local_block / 8;
            uint32_t bit_idx = local_block % 8;
            bitmap.buf[byte_idx] &= ~(1 << bit_idx);
            
            write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
            
            bgdt.table[bgd_index].bg_free_blocks_count++;
            superblock.s_free_blocks_count++;
        }
    }
    
    // Dealokasi doubly indirect blocks (jika ada)
    uint32_t blocks_in_single_indirect = BLOCK_SIZE / sizeof(uint32_t);
    if (blocks > 12 + blocks_in_single_indirect) {
        uint32_t double_indirect_block = locations[13];
        if (double_indirect_block != 0) {
            struct BlockBuffer double_indirect_buf;
            read_blocks(&double_indirect_buf, double_indirect_block, 1);
            uint32_t *double_indirect_blocks = (uint32_t *)double_indirect_buf.buf;
            
            uint32_t remaining_blocks = blocks - 12 - blocks_in_single_indirect;
            
            // Loop through indirect blocks
            for (uint32_t i = 0; i < (BLOCK_SIZE / sizeof(uint32_t)) && remaining_blocks > 0; i++) {
                uint32_t indirect_block = double_indirect_blocks[i];
                if (indirect_block == 0) continue;
                
                struct BlockBuffer indirect_buf;
                read_blocks(&indirect_buf, indirect_block, 1);
                uint32_t *indirect_blocks = (uint32_t *)indirect_buf.buf;
                
                uint32_t count = remaining_blocks;
                if (count > blocks_in_single_indirect) {
                    count = blocks_in_single_indirect;
                }
                
                // Dealokasi blocks
                for (uint32_t j = 0; j < count; j++) {
                    uint32_t block_num = indirect_blocks[j];
                    if (block_num == 0) continue;
                    
                    uint32_t bgd_index = block_num / BLOCKS_PER_GROUP;
                    uint32_t local_block = block_num % BLOCKS_PER_GROUP;
                    
                    struct BlockBuffer bitmap;
                    read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
                    
                    uint32_t byte_idx = local_block / 8;
                    uint32_t bit_idx = local_block % 8;
                    bitmap.buf[byte_idx] &= ~(1 << bit_idx);
                    
                    write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
                    
                    bgdt.table[bgd_index].bg_free_blocks_count++;
                    superblock.s_free_blocks_count++;
                }
                
                // Dealokasi indirect block
                uint32_t bgd_index = indirect_block / BLOCKS_PER_GROUP;
                uint32_t local_block = indirect_block % BLOCKS_PER_GROUP;
                
                struct BlockBuffer bitmap;
                read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
                
                uint32_t byte_idx = local_block / 8;
                uint32_t bit_idx = local_block % 8;
                bitmap.buf[byte_idx] &= ~(1 << bit_idx);
                
                write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
                
                bgdt.table[bgd_index].bg_free_blocks_count++;
                superblock.s_free_blocks_count++;
                
                remaining_blocks -= count;
            }
            
            // Dealokasi doubly indirect block itu sendiri
            uint32_t bgd_index = double_indirect_block / BLOCKS_PER_GROUP;
            uint32_t local_block = double_indirect_block % BLOCKS_PER_GROUP;
            
            struct BlockBuffer bitmap;
            read_blocks(&bitmap, bgdt.table[bgd_index].bg_block_bitmap, 1);
            
            uint32_t byte_idx = local_block / 8;
            uint32_t bit_idx = local_block % 8;
            bitmap.buf[byte_idx] &= ~(1 << bit_idx);
            
            write_blocks(bgdt.table[bgd_index].bg_block_bitmap, 1, &bitmap);
            
            bgdt.table[bgd_index].bg_free_blocks_count++;
            superblock.s_free_blocks_count++;
        }
    }
    
    // Tulis kembali bgdt dan superblock
    write_blocks(2, 1, &bgdt);
    write_blocks(1, 1, &superblock);
}

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t preferred_bgd) {
    // Baca superblock dan bgdt
    read_blocks(&superblock, 1, 1);
    read_blocks(&bgdt, 2, 1);
    
    uint32_t blocks_needed = node->i_blocks;
    uint32_t blocks_allocated = 0;
    uint8_t *data_ptr = (uint8_t *)ptr;
    
    // Alokasi direct blocks (maksimal 12 blok pertama)
    for (uint32_t i = 0; i < 12 && blocks_allocated < blocks_needed; i++) {
        // Cari block kosong menggunakan First Fit Algorithm
        bool allocated = false;
        
        // Mulai dari preferred_bgd
        for (uint32_t bgd_idx = preferred_bgd; bgd_idx < GROUPS_COUNT && !allocated; bgd_idx++) {
            if (bgdt.table[bgd_idx].bg_free_blocks_count > 0) {
                struct BlockBuffer bitmap;
                read_blocks(&bitmap, bgdt.table[bgd_idx].bg_block_bitmap, 1);
                
                // Cari bit kosong
                for (uint32_t byte_idx = 0; byte_idx < BLOCK_SIZE && !allocated; byte_idx++) {
                    if (bitmap.buf[byte_idx] != 0xFF) {
                        for (uint32_t bit_idx = 0; bit_idx < 8 && !allocated; bit_idx++) {
                            if (!(bitmap.buf[byte_idx] & (1 << bit_idx))) {
                                // Alokasikan block
                                bitmap.buf[byte_idx] |= (1 << bit_idx);
                                
                                uint32_t local_block = byte_idx * 8 + bit_idx;
                                uint32_t block_num = bgd_idx * BLOCKS_PER_GROUP + local_block;
                                
                                node->i_block[i] = block_num;
                                
                                // Tulis data ke block (jika ptr tidak NULL)
                                if (ptr != NULL) {
                                    struct BlockBuffer block_data;
                                    memset(&block_data, 0, BLOCK_SIZE);
                                    
                                    uint32_t bytes_to_copy = (node->i_size - blocks_allocated * BLOCK_SIZE);
                                    if (bytes_to_copy > BLOCK_SIZE) {
                                        bytes_to_copy = BLOCK_SIZE;
                                    }
                                    
                                    memcpy(block_data.buf, data_ptr + blocks_allocated * BLOCK_SIZE, bytes_to_copy);
                                    write_blocks(block_num, 1, &block_data);
                                }
                                
                                // Update bitmap
                                write_blocks(bgdt.table[bgd_idx].bg_block_bitmap, 1, &bitmap);
                                
                                // Update bgdt
                                bgdt.table[bgd_idx].bg_free_blocks_count--;
                                
                                // Update superblock
                                superblock.s_free_blocks_count--;
                                
                                blocks_allocated++;
                                allocated = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Jika masih ada blocks yang perlu dialokasikan, gunakan single indirect
    if (blocks_allocated < blocks_needed) {
        // Alokasikan block untuk single indirect
        bool indirect_allocated = false;
        
        for (uint32_t bgd_idx = preferred_bgd; bgd_idx < GROUPS_COUNT && !indirect_allocated; bgd_idx++) {
            if (bgdt.table[bgd_idx].bg_free_blocks_count > 0) {
                struct BlockBuffer bitmap;
                read_blocks(&bitmap, bgdt.table[bgd_idx].bg_block_bitmap, 1);
                
                for (uint32_t byte_idx = 0; byte_idx < BLOCK_SIZE && !indirect_allocated; byte_idx++) {
                    if (bitmap.buf[byte_idx] != 0xFF) {
                        for (uint32_t bit_idx = 0; bit_idx < 8 && !indirect_allocated; bit_idx++) {
                            if (!(bitmap.buf[byte_idx] & (1 << bit_idx))) {
                                bitmap.buf[byte_idx] |= (1 << bit_idx);
                                
                                uint32_t local_block = byte_idx * 8 + bit_idx;
                                uint32_t block_num = bgd_idx * BLOCKS_PER_GROUP + local_block;
                                
                                node->i_block[12] = block_num;
                                
                                write_blocks(bgdt.table[bgd_idx].bg_block_bitmap, 1, &bitmap);
                                
                                bgdt.table[bgd_idx].bg_free_blocks_count--;
                                superblock.s_free_blocks_count--;
                                
                                indirect_allocated = true;
                            }
                        }
                    }
                }
            }
        }
        
        // Alokasi blocks yang direferensi oleh single indirect
        struct BlockBuffer indirect_buf;
        memset(&indirect_buf, 0, BLOCK_SIZE);
        uint32_t *indirect_blocks = (uint32_t *)indirect_buf.buf;
        
        uint32_t indirect_index = 0;
        while (blocks_allocated < blocks_needed && indirect_index < (BLOCK_SIZE / sizeof(uint32_t))) {
            bool allocated = false;
            
            for (uint32_t bgd_idx = preferred_bgd; bgd_idx < GROUPS_COUNT && !allocated; bgd_idx++) {
                if (bgdt.table[bgd_idx].bg_free_blocks_count > 0) {
                    struct BlockBuffer bitmap;
                    read_blocks(&bitmap, bgdt.table[bgd_idx].bg_block_bitmap, 1);
                    
                    for (uint32_t byte_idx = 0; byte_idx < BLOCK_SIZE && !allocated; byte_idx++) {
                        if (bitmap.buf[byte_idx] != 0xFF) {
                            for (uint32_t bit_idx = 0; bit_idx < 8 && !allocated; bit_idx++) {
                                if (!(bitmap.buf[byte_idx] & (1 << bit_idx))) {
                                    bitmap.buf[byte_idx] |= (1 << bit_idx);
                                    
                                    uint32_t local_block = byte_idx * 8 + bit_idx;
                                    uint32_t block_num = bgd_idx * BLOCKS_PER_GROUP + local_block;
                                    
                                    indirect_blocks[indirect_index] = block_num;
                                    
                                    if (ptr != NULL) {
                                        struct BlockBuffer block_data;
                                        memset(&block_data, 0, BLOCK_SIZE);
                                        
                                        uint32_t bytes_to_copy = (node->i_size - blocks_allocated * BLOCK_SIZE);
                                        if (bytes_to_copy > BLOCK_SIZE) {
                                            bytes_to_copy = BLOCK_SIZE;
                                        }
                                        
                                        memcpy(block_data.buf, data_ptr + blocks_allocated * BLOCK_SIZE, bytes_to_copy);
                                        write_blocks(block_num, 1, &block_data);
                                    }
                                    
                                    write_blocks(bgdt.table[bgd_idx].bg_block_bitmap, 1, &bitmap);
                                    
                                    bgdt.table[bgd_idx].bg_free_blocks_count--;
                                    superblock.s_free_blocks_count--;
                                    
                                    blocks_allocated++;
                                    indirect_index++;
                                    allocated = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Tulis indirect block
        write_blocks(node->i_block[12], 1, &indirect_buf);
    }
    
    // TODO: Implement doubly indirect jika diperlukan untuk file besar
    
    // Tulis kembali bgdt dan superblock
    write_blocks(2, 1, &bgdt);
    write_blocks(1, 1, &superblock);
}

void sync_node(struct EXT2Inode *node, uint32_t inode) {
    uint32_t bgd_index = inode_to_bgd(inode);
    uint32_t local_index = inode_to_local(inode);
    
    struct EXT2InodeTable inode_table;
    uint32_t inode_table_block = bgdt.table[bgd_index].bg_inode_table;
    uint32_t block_offset = local_index / INODES_PER_TABLE;
    uint32_t table_index = local_index % INODES_PER_TABLE;
    
    // Baca inode table
    read_blocks(&inode_table, inode_table_block + block_offset, 1);
    
    // Update inode
    inode_table.table[table_index] = *node;
    
    // Tulis kembali ke disk
    write_blocks(inode_table_block + block_offset, 1, &inode_table);
}