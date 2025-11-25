#include <stdint.h>
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"

#define BLOCK_COUNT 16
#define MAX_CMD_LEN 128
#define MAX_ARGS 16

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

uint32_t current_directory = 1;
char current_directory_name[32] = "/";

void find_recursive(uint32_t dir_inode, const char *path, const char *target, int depth)
{
    if (depth > 8) return;
    
    uint32_t base_addr = 0x220000 + (depth * 0x800);  // 2KB per level
    char *find_buf = (char *)base_addr;
    char *find_name_buf = (char *)(base_addr + 0x400);  // +1KB
    char *find_path_buf = (char *)(base_addr + 0x600);  // +1.5KB
    
    struct EXT2DriverRequest req = {
        .buf = (void *)find_buf,
        .name = ".",
        .name_len = 1,
        .parent_inode = dir_inode,
        .buffer_size = 1024,
        .is_directory = true
    };

    int8_t retcode;
    syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);
    if (retcode != 0) return;

    uint32_t offset = 0;
    
    while (offset < 1024)
    {
        struct EXT2DirectoryEntry *e = (struct EXT2DirectoryEntry *)(find_buf + offset);
        if (e->rec_len == 0) break;
        if (e->inode == 0)
        {
            offset += e->rec_len;
            continue;
        }

        char *name = (char *)(find_buf + offset + sizeof(struct EXT2DirectoryEntry));
        
        // Copy name safely
        uint32_t name_len = e->name_len;
        if (name_len > 200) name_len = 200;
        memcpy(find_name_buf, name, name_len);
        find_name_buf[name_len] = '\0';

        // skip "." dan ".."
        if ((name_len == 1 && find_name_buf[0] == '.') ||
            (name_len == 2 && find_name_buf[0] == '.' && find_name_buf[1] == '.'))
        {
            offset += e->rec_len;
            continue;
        }

        // Build full path
        uint32_t path_len = strlen(path);
        if (path_len > 200) {
            offset += e->rec_len;
            continue;
        }
        
        memcpy(find_path_buf, path, path_len);
        find_path_buf[path_len] = '\0';
        
        if (path_len > 0 && find_path_buf[path_len-1] != '/') {
            find_path_buf[path_len] = '/';
            find_path_buf[path_len+1] = '\0';
        }
        
        strcat(find_path_buf, find_name_buf);

        // print jika cocok
        if (strcmp(find_name_buf, target) == 0)
        {
            syscall(6, (uint32_t)find_path_buf, strlen(find_path_buf), 0xF);
            syscall(6, (uint32_t)"\n", 1, 0xF);
        }

        // recursive jika directory
        if (e->file_type == EXT2_FT_DIR)
        {
            find_recursive(e->inode, find_path_buf, target, depth + 1);
        }

        offset += e->rec_len;
    }
}

int main(void)
{

    char buf;
    syscall(7, 0, 0, 0);

    char line[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    int argc;

    while (true)
    {
        syscall(6, (uint32_t)"megumOS:", strlen("megumOS:"), 0xF);
        syscall(6, (uint32_t)current_directory_name, strlen(current_directory_name), 0xF);
        syscall(6, (uint32_t)"$ ", strlen("$ "), 0xF);

        int idx = 0;
        while (true)
        {
            syscall(4, (uint32_t)&buf, 0, 0);
            
            if (buf == '\n')
            {
                syscall(5, (uint32_t)'\n', 0x0F, 0);
                break;
            }
            
            if (buf == '\0')
                break;
            
            if (buf == '\b')
            {
                if (idx > 0)
                {
                    syscall(5, (uint32_t)buf, 0x0F, 0);
                    idx--;
                }
                continue;
            }

            if (idx < MAX_CMD_LEN - 1)
            {
                line[idx++] = buf;
                syscall(5, (uint32_t)buf, 0x0F, 0);
            }
        }

        line[idx] = '\0';

        argc = 0;
        char *p = line;
        while (*p && argc < MAX_ARGS)
        {
            while (*p == ' ')
                p++;
            if (!*p)
                break;
            argv[argc++] = p;
            while (*p && *p != ' ')
                p++;
            if (*p)
                *p++ = '\0';
        }

        if (argc == 0)
        {
            syscall(6, (uint32_t)"Empty command\n", 15, 0x0F);
            continue;
        }

        if (strcmp(argv[0], "echo") == 0)
        {
            for (int i = 1; i < argc; i++)
            {
                size_t len = strlen(argv[i]);
                syscall(6, (uint32_t)argv[i], (uint32_t)len, 0x0F);
                if (i + 1 < argc)
                    syscall(6, (uint32_t)" ", 1, 0x0F);
            }
            syscall(5, (uint32_t)'\n', 0x0F, 0);
            continue;
        }
        else if (strcmp(argv[0], "mkdir") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"mkdir: missing argument\n", strlen("mkdir: missing argument\n"), 0xF);
                continue;
            }
            struct EXT2DriverRequest req = {
                .buf = NULL,
                .name = argv[1],
                .name_len = (uint32_t)strlen(argv[1]),
                .parent_inode = current_directory,
                .buffer_size = 0,
                .is_directory = true};
            uint32_t retcode = 0;
            syscall(2, (uint32_t)&req, (uint32_t)&retcode, 0);
            continue;
        }
        else if (strcmp(argv[0], "ls") == 0)
        {
            struct EXT2DriverRequest req = {
                .buf = (void *)0x210000,
                .name = ".",
                .name_len = 1,
                .parent_inode = current_directory,
                .buffer_size = 1024,
                .is_directory = true};
            uint32_t retcode = 0;
            syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);
            if ((int8_t)retcode == 0)
            {
                uint16_t offset = 0;
                uint8_t *block = (uint8_t *)req.buf;

                while (offset < req.buffer_size)
                {
                    struct EXT2DirectoryEntry *e = (void *)(block + offset);

                    if (e->rec_len == 0)
                        break;
                    if (e->inode == 0)
                    {
                        offset += e->rec_len;
                        continue;
                    }

                    char *name = (char *)(block + offset + sizeof(*e));

                    // Skip ".", ".."
                    if ((e->name_len == 1 && name[0] == '.') ||
                        (e->name_len == 2 && name[0] == '.' && name[1] == '.'))
                    {
                        offset += e->rec_len;
                        continue;
                    }

                    // Skip "shell"
                    if (e->name_len == 5 && memcmp(name, "shell", 5) == 0)
                    {
                        offset += e->rec_len;
                        continue;
                    }

                    // Print name
                    for (uint16_t i = 0; i < e->name_len; i++)
                    {
                        syscall(5, (uint32_t)name[i], 0x0F, 0);
                    }
                    syscall(5, (uint32_t)'\n', 0x0F, 0);

                    offset += e->rec_len;
                }
            }
            continue;
        }
        else if (strcmp(argv[0], "cd") == 0)
        {
            if (argc < 2) {
                syscall(6,(uint32_t)"cd: missing argument\n",23,0x0F);
                continue;
            }

            // cd /
            if (strcmp(argv[1], "/") == 0) {
                current_directory = 1;
                strcpy(current_directory_name, "/");
                continue;
            }

            // cd ..
            if (strcmp(argv[1], "..") == 0) {

                // 1. baca inode parent dari entry ".."
                char bufp[1024];
                struct EXT2DriverRequest reqp = {
                    .buf = bufp,
                    .name = "..",
                    .name_len = 2,
                    .parent_inode = current_directory,
                    .buffer_size = 1024,
                    .is_directory = true
                };

                int8_t retp;
                syscall(1,(uint32_t)&reqp,(uint32_t)&retp,0);

                if (retp == 0) {
                    struct EXT2DirectoryEntry *e = (void*)(bufp + 0);
                    current_directory = e->inode;
                }

                // 2. update path string
                if (strcmp(current_directory_name, "/") != 0) {
                    char *last = strrchr(current_directory_name, '/');

                    if (last) {
                        if (last == current_directory_name) {
                            current_directory_name[1] = '\0';
                        } else {
                            *last = '\0';
                        }
                    }
                }

                continue;
            }

            // cd directory normal
            char buf[1024] = {0};
            struct EXT2DriverRequest req = {
                .buf = buf,
                .name = argv[1],
                .name_len = strlen(argv[1]),
                .parent_inode = current_directory,
                .buffer_size = 1024,
                .is_directory = true
            };

            int8_t ret;
            syscall(1,(uint32_t)&req,(uint32_t)&ret,0);

            if (ret == 0) {
                struct EXT2DirectoryEntry *e = (void*)(buf + 0);
                current_directory = e->inode;

                // update path
                if (strcmp(current_directory_name, "/") != 0)
                    strcat(current_directory_name, "/");
                strcat(current_directory_name, argv[1]);

            } else {
                syscall(6,(uint32_t)"cd: directory not found\n",
                        strlen("cd: directory not found\n"),0x0F);
            }

            continue;
        }
        else if (strcmp(argv[0], "cat") == 0)
        {
            if (argc < 2) {
                syscall(6,(uint32_t)"cd: missing argument\n",23,0x0F);
                continue;
            }

            struct EXT2DriverRequest req = {
                .buf = (void *)0x200000,
                .name = argv[1],
                .name_len = (uint32_t)strlen(argv[1]),
                .parent_inode = current_directory,
                .buffer_size = 8192,
                .is_directory = false};

            int8_t retcode;
            syscall(0, (uint32_t)&req, (uint32_t)&retcode, 0);

            if (retcode == 0)
            {
                char *content = (char *)req.buf;
                uint32_t len = strlen(content);
                syscall(6, (uint32_t)content, len, 0x0F);
                syscall(5, (uint32_t)'\n', 0x0F, 0);
            }
            else
            {
                syscall(6, (uint32_t)"cat: file not found\n", 21, 0x0F);
            }
            continue;
        }
        else if (strcmp(argv[0], "write") == 0) 
        {
            if (argc < 3)
            {
                syscall(6, (uint32_t)"write: missing arguments\n", 26, 0x0F);
                continue;
            }

            char content[1024];
            content[0] = '\0';

            for (int i = 2; i < argc; i++)
            {
                strcat(content, argv[i]);
                if (i + 1 < argc)
                    strcat(content, " ");
            }

            struct EXT2DriverRequest req = {
                .buf = content,
                .name = argv[1],
                .name_len = (uint32_t)strlen(argv[1]),
                .parent_inode = current_directory,
                .buffer_size = (uint32_t)strlen(content),
                .is_directory = false};

            uint32_t ret = 0;
            syscall(2, (uint32_t)&req, (uint32_t)&ret, 0); // syscall write

            if ((int32_t)ret == 0)
                syscall(6, (uint32_t)"File written.\n", 14, 0x0F);
            else
                syscall(6, (uint32_t)"write: failed.\n", 15, 0x0F);

            continue;
        }
        else if (strcmp(argv[0], "clear") == 0) {
            syscall(8, 0, 0, 0);
            continue;
        }
        else if (strcmp(argv[0], "rm") == 0)
        {
            if (argc < 2) {
                syscall(6,(uint32_t)"rm: usage: rm [-r] <name>\n",
                        strlen("rm: usage: rm [-r] <name>\n"),0x0F);
                continue;
            }

            bool recursive = false;
            char *target = NULL;

            if (argc >= 3 && strcmp(argv[1], "-r") == 0) {
                recursive = true;
                target = argv[2];
            } else {
                target = argv[1];
            }

            if (target == NULL) {
                syscall(6,(uint32_t)"rm: missing name\n",
                        strlen("rm: missing name\n"),0x0F);
                continue;
            }

            struct EXT2DriverRequest req = {
                .buf = NULL,
                .name = target,
                .name_len = strlen(target),
                .parent_inode = current_directory,
                .buffer_size = 0,
                .is_directory = recursive ? true : false   // dipakai sebagai "recursive" flag
            };

            uint32_t retcode;
            syscall(3,(uint32_t)&req,(uint32_t)&retcode,0);

            if (retcode == 0) {
                // sukses, diam aja
            }
            else if (retcode == 1) {
                syscall(6,(uint32_t)"rm: not found\n",
                        strlen("rm: not found\n"),0x0F);
            }
            else if (retcode == 2) {
                // mode non-recursive, directory not empty
                syscall(6,(uint32_t)"rm: directory not empty (use rm -r)\n",
                        strlen("rm: directory not empty (use rm -r)\n"),0x0F);
            }
            else {
                syscall(6,(uint32_t)"rm: error\n",
                        strlen("rm: error\n"),0x0F);
            }

            continue;
        }
        else if (strcmp(argv[0], "find") == 0)
        {
            if (argc < 2) {
                syscall(6,(uint32_t)"find: missing name\n",
                        strlen("find: missing name\n"),0x0F);
                continue;
            }
            find_recursive(1, "/", argv[1], 0);
            continue;
        }
        else if (strcmp(argv[0], "cp") == 0) {
            if (argc < 3) {
                syscall(6, (uint32_t)"cp: usage: cp <src> <dst>\n",
                        strlen("cp: usage: cp <src> <dst>\n"), 0x0F);
                continue;
            }

            char *src = argv[1];
            char *dst = argv[2];

            uint8_t *file_buf = (uint8_t *)0x230000;  // 4KB buffer at fixed address
            int8_t retcode;

            struct EXT2DriverRequest rreq = {
                .buf = file_buf,
                .name = src,
                .name_len = strlen(src),
                .parent_inode = current_directory,
                .buffer_size = 4096,
                .is_directory = false
            };

            syscall(0, (uint32_t)&rreq, (uint32_t)&retcode, 0);
            if (retcode != 0) {
                syscall(6, (uint32_t)"cp: source not found or not a file\n",
                        strlen("cp: source not found or not a file\n"), 0x0F);
                continue;
            }

            uint32_t filesize = strlen((char *)file_buf);

            struct EXT2DriverRequest wreq = {
                .buf = file_buf,
                .name = dst,
                .name_len = strlen(dst),
                .parent_inode = current_directory,
                .buffer_size = filesize,
                .is_directory = false
            };

            syscall(2, (uint32_t)&wreq, (uint32_t)&retcode, 0);
            if (retcode != 0) {
                syscall(6, (uint32_t)"cp: write failed\n",
                        strlen("cp: write failed\n"), 0x0F);
                continue;
            }

            continue;
        }
        else
        {
            char u[] = "Unknown command: ";
            int i = 0;
            while (u[i] != '\0') {
                syscall(5, (uint32_t)u[i], 0x0F, 0);
                i++;
            }
            
            syscall(6, (uint32_t)argv[0], strlen(argv[0]), 0x0F);
            syscall(5, (uint32_t)'\n', 0x0F, 0);
        }
    }
    return 0;
}