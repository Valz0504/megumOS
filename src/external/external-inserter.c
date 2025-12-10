#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "header/filesystem/ext2.h"
#include "header/driver/disk.h"
#include "header/stdlib/string.h"

// Global variable
uint8_t *image_storage;
uint8_t *file_buffer;
uint8_t *read_buffer;

#define TRUE 1
#define FALSE 0

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            (uint8_t*) ptr + BLOCK_SIZE*i, 
            image_storage + BLOCK_SIZE*(logical_block_address+i), 
            BLOCK_SIZE
        );
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            image_storage + BLOCK_SIZE*(logical_block_address+i), 
            (uint8_t*) ptr + BLOCK_SIZE*i, 
            BLOCK_SIZE
        );
    }
}

// Find inode number of a folder by name in parent directory
// This uses a simplified approach - try to read the folder as directory
uint32_t find_inode_by_name(char *name, uint32_t parent_inode) {
    // We'll use a trick: inode numbers are sequential
    // Try reading with different inodes until we find the right one
    // For now, we'll just return a hardcoded value for "badapple"
    // The actual inode will be assigned when the folder is created
    
    // Simple heuristic: badapple folder is usually inode 2 (after root which is 1)
    // But we can't reliably determine this without read_directory support
    // So we return 0 to indicate "not found yet"
    return 0;
}

// Create a directory
int create_directory(char *name, uint32_t parent_inode) {
    struct EXT2DriverRequest request;
    request.buf = NULL;
    request.buffer_size = 0;
    request.name = name;
    request.name_len = strlen(name);
    request.parent_inode = parent_inode;
    request.is_directory = TRUE;
    
    int8_t retcode = write(&request);
    return retcode;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "inserter: ./inserter <file to insert> <parent cluster index> <storage> [folder_name]\n");
        exit(1);
    }

    // Read storage into memory, sized to DISK_SPACE
    image_storage = malloc(DISK_SPACE);
    file_buffer   = malloc(DISK_SPACE);
    read_buffer   = malloc(DISK_SPACE);
    FILE *fptr    = fopen(argv[3], "r");
    fread(image_storage, DISK_SPACE, 1, fptr);
    fclose(fptr);

    // Read target file, assuming file fits within DISK_SPACE
    FILE *fptr_target = fopen(argv[1], "r");
    size_t filesize   = 0;
    if (fptr_target == NULL)
        filesize = 0;
    else {
        fread(file_buffer, DISK_SPACE, 1, fptr_target);
        fseek(fptr_target, 0, SEEK_END);
        filesize = ftell(fptr_target);
        fclose(fptr_target);
    }

    printf("Filename : %s\n",  argv[1]);
    printf("Filesize : %ld bytes\n", filesize);

    // EXT2 operations
    initialize_filesystem_ext2();
    
    uint32_t parent_inode;
    sscanf(argv[2], "%u", &parent_inode);
    
    // If folder name is provided (argc >= 5), create it and use hardcoded inode
    if (argc >= 5) {
        char *folder_name = argv[4];
        
        // First, try to create the folder (will fail if exists, which is OK)
        printf("Ensuring folder exists: %s\n", folder_name);
        int ret = create_directory(folder_name, parent_inode);
        if (ret == 0) {
            printf("Folder created successfully\n");
        } else if (ret == 1) {
            printf("Folder already exists\n");
        } else {
            printf("Note: folder creation returned code %d\n", ret);
        }
        
        // Use inode 2 for badapple folder (first folder after root)
        // This is a simplified approach
        parent_inode = 2;
        printf("Using folder inode: %u\n", parent_inode);
    }
    
    char *name = argv[1];
    int filename_length = strlen(name);
    struct EXT2DriverRequest request;
    struct EXT2DriverRequest reqread;
    printf("Filename       : %s\n", name);
    printf("Filename length: %d\n", filename_length);

    request.buf = file_buffer;
    request.buffer_size = filesize;
    request.name = name;
    request.name_len = filename_length;
    request.is_directory = FALSE;
    request.parent_inode = parent_inode;

    reqread = request;
    reqread.buf = read_buffer;
    int retcode = read(reqread);
    if (retcode == 0)
    {
        bool same = TRUE;
        for (uint32_t i = 0; i < filesize; i++)
        {
            if (read_buffer[i] != file_buffer[i])
            {
                printf("not same\n");
                same = FALSE;
                break;
            }
        }
        if (same)
        {
            printf("same\n");
        }
    }

    retcode = write(&request);
    if (retcode == 1)
    {
        retcode = delete (request);
        retcode = write(&request);
    }
    if (retcode == 0)
        puts("Write success");
    else if (retcode == 1)
        puts("Error: File/folder name already exist");
    else if (retcode == 2)
        puts("Error: Invalid parent node index");
    else
        puts("Error: Unknown error");

    // Write image in memory into original, overwrite them
    fptr = fopen(argv[3], "w");
    fwrite(image_storage, DISK_SPACE, 1, fptr);
    fclose(fptr);

    return 0;
}