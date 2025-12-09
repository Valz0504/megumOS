#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include "header/driver/cmos.h"

#define BLOCK_COUNT 16
#define MAX_CMD_LEN 128
#define MAX_ARGS 16
#define MAX_ENV 8
#define MAX_ENV_KEY 16
#define MAX_ENV_VAL 48
#define APP_LOAD_ADDR ((void *)0x00200000)
#define APP_MAX_SIZE (512 * 1024)
#define PID_SHELL 1
#define PID_CLOCK 2

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

int current_directory = 1;
char current_directory_name[32] = "/";

struct EnvEntry
{
    char key[MAX_ENV_KEY];
    char val[MAX_ENV_VAL];
    bool used;
};
static struct EnvEntry env_table[MAX_ENV];

static bool parse_int(const char *s, int *out)
{
    if (!s || !out || *s == '\0')
        return false;
    int sign = 1;
    if (*s == '-')
    {
        sign = -1;
        s++;
    }
    else if (*s == '+')
    {
        s++;
    }
    int value = 0;
    bool has_digit = false;
    while (*s)
    {
        if (*s < '0' || *s > '9')
            return false;
        has_digit = true;
        value = value * 10 + (*s - '0');
        s++;
    }
    if (!has_digit)
        return false;
    *out = value * sign;
    return true;
}

static void env_set(const char *key, const char *val)
{
    if (!key || !val)
        return;
    for (int i = 0; i < MAX_ENV; i++)
    {
        if (env_table[i].used && strcmp(env_table[i].key, key) == 0)
        {
            strncpy(env_table[i].val, val, MAX_ENV_VAL - 1);
            env_table[i].val[MAX_ENV_VAL - 1] = '\0';
            return;
        }
    }
    for (int i = 0; i < MAX_ENV; i++)
    {
        if (!env_table[i].used)
        {
            env_table[i].used = true;
            strncpy(env_table[i].key, key, MAX_ENV_KEY - 1);
            env_table[i].key[MAX_ENV_KEY - 1] = '\0';
            strncpy(env_table[i].val, val, MAX_ENV_VAL - 1);
            env_table[i].val[MAX_ENV_VAL - 1] = '\0';
            return;
        }
    }
}

static const char *env_get(const char *key)
{
    for (int i = 0; i < MAX_ENV; i++)
    {
        if (env_table[i].used && strcmp(env_table[i].key, key) == 0)
        {
            return env_table[i].val;
        }
    }
    return NULL;
}

static void env_print_all(void)
{
    for (int i = 0; i < MAX_ENV; i++)
    {
        if (env_table[i].used)
        {
            syscall(6, (uint32_t)env_table[i].key, strlen(env_table[i].key), 0x0F);
            syscall(6, (uint32_t)"=", 1, 0x0F);
            syscall(6, (uint32_t)env_table[i].val, strlen(env_table[i].val), 0x0F);
            syscall(6, (uint32_t)"\n", 1, 0x0F);
        }
    }
}

// Resolve a path and change current_directory/current_directory_name on success
static bool resolve_and_change_dir(const char *path_input)
{
    // Work on copies so we don't mutate globals until success
    uint32_t inode = (path_input[0] == '/') ? 1 : current_directory;
    char new_path[32];
    if (path_input[0] == '/')
        strcpy(new_path, "/");
    else
        strcpy(new_path, current_directory_name);

    char path_copy[128];
    strncpy(path_copy, path_input, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *token = path_copy;
    if (token[0] == '/')
        token++; // skip leading slash

    while (*token)
    {
        // Find next separator
        char *slash = token;
        while (*slash && *slash != '/')
            slash++;
        bool last = (*slash == '\0');
        char saved = *slash;
        *slash = '\0';

        // Skip empty / .
        if (token[0] == '\0' || (token[0] == '.' && token[1] == '\0'))
        {
            // nothing
        }
        else if (token[0] == '.' && token[1] == '.' && token[2] == '\0')
        {
            // cd ..
            char bufp[1024];
            struct EXT2DriverRequest reqp = {
                .buf = bufp,
                .name = ".",
                .name_len = 1,
                .parent_inode = inode,
                .buffer_size = sizeof(bufp),
                .is_directory = true};
            int8_t retp;
            syscall(1, (uint32_t)&reqp, (uint32_t)&retp, 0);
            if (retp != 0)
                return false;
            struct EXT2DirectoryEntry *dot = (void *)bufp;
            struct EXT2DirectoryEntry *dotdot = (void *)((uint8_t *)dot + dot->rec_len);
            inode = dotdot->inode;

            // pop from path string
            if (strcmp(new_path, "/") != 0)
            {
                char *last_slash = strrchr(new_path, '/');
                if (last_slash)
                {
                    if (last_slash == new_path)
                        new_path[1] = '\0';
                    else
                        *last_slash = '\0';
                }
            }
        }
        else
        {
            // normal component: request directory and read its inode
            char dirbuf[1024];
            struct EXT2DriverRequest req = {
                .buf = dirbuf,
                .name = token,
                .name_len = strlen(token),
                .parent_inode = inode,
                .buffer_size = sizeof(dirbuf),
                .is_directory = true};
            int8_t ret;
            syscall(1, (uint32_t)&req, (uint32_t)&ret, 0);
            if (ret != 0)
                return false;
            struct EXT2DirectoryEntry *dot = (void *)dirbuf;
            inode = dot->inode;

            // append to path string
            if (strcmp(new_path, "/") != 0)
            {
                if (strlen(new_path) + 1 >= sizeof(new_path))
                    return false;
                strcat(new_path, "/");
            }
            if (strlen(new_path) + strlen(token) >= sizeof(new_path))
                return false;
            strcat(new_path, token);
        }

        *slash = saved;
        if (last)
            break;
        token = slash + 1;
    }

    current_directory = inode;
    strcpy(current_directory_name, new_path);
    return true;
}

// Resolve a path to its parent directory inode and final component name.
// Supports absolute and relative paths with '.' and '..'.
static bool resolve_parent_and_name(const char *path_input, uint32_t *parent_inode_out,
                                    char *name_out, uint8_t *name_len_out)
{
    if (!path_input || !parent_inode_out || !name_out || !name_len_out)
        return false;
    if (strcmp(path_input, "/") == 0)
        return false; // cannot use root as name

    uint32_t inode = (path_input[0] == '/') ? 1 : current_directory;

    char path_copy[128];
    strncpy(path_copy, path_input, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *token = path_copy;
    if (token[0] == '/')
        token++; // skip leading slash

    char *last_comp = NULL;
    uint8_t last_len = 0;

    while (true)
    {
        char *slash = token;
        while (*slash && *slash != '/')
            slash++;
        bool last = (*slash == '\0');
        char saved = *slash;
        *slash = '\0';

        if (token[0] == '\0' || (token[0] == '.' && token[1] == '\0'))
        {
            // skip
        }
        else if (token[0] == '.' && token[1] == '.' && token[2] == '\0')
        {
            // go up
            char bufp[1024];
            struct EXT2DriverRequest reqp = {
                .buf = bufp,
                .name = ".",
                .name_len = 1,
                .parent_inode = inode,
                .buffer_size = sizeof(bufp),
                .is_directory = true};
            int8_t retp;
            syscall(1, (uint32_t)&reqp, (uint32_t)&retp, 0);
            if (retp != 0)
                return false;
            struct EXT2DirectoryEntry *dot = (void *)bufp;
            struct EXT2DirectoryEntry *dotdot = (void *)((uint8_t *)dot + dot->rec_len);
            inode = dotdot->inode;
        }
        else
        {
            if (last)
            {
                last_comp = token;
                last_len = (uint8_t)strlen(token);
            }
            else
            {
                char dirbuf[1024];
                struct EXT2DriverRequest req = {
                    .buf = dirbuf,
                    .name = token,
                    .name_len = strlen(token),
                    .parent_inode = inode,
                    .buffer_size = sizeof(dirbuf),
                    .is_directory = true};
                int8_t ret;
                syscall(1, (uint32_t)&req, (uint32_t)&ret, 0);
                if (ret != 0)
                    return false;
                struct EXT2DirectoryEntry *dot = (void *)dirbuf;
                inode = dot->inode;
            }
        }

        *slash = saved;
        if (last)
            break;
        token = slash + 1;
    }

    if (last_comp == NULL || last_len == 0 || last_len > 31)
        return false;

    *parent_inode_out = inode;
    *name_len_out = last_len;
    memcpy(name_out, last_comp, last_len);
    name_out[last_len] = '\0';
    return true;
}

// Resolve a directory path to its inode without changing current state.
static bool resolve_dir_inode(const char *path_input, uint32_t *inode_out)
{
    if (!path_input || !inode_out)
        return false;
    if (strcmp(path_input, "/") == 0)
    {
        *inode_out = 1;
        return true;
    }
    if (strcmp(path_input, ".") == 0)
    {
        *inode_out = current_directory;
        return true;
    }
    uint32_t dummy_parent;
    char name[32];
    uint8_t name_len;
    // Try to split into parent + name and ensure the last component is a directory
    if (!resolve_parent_and_name(path_input, &dummy_parent, name, &name_len))
        return false;

    char dirbuf[1024];
    struct EXT2DriverRequest req = {
        .buf = dirbuf,
        .name = name,
        .name_len = name_len,
        .parent_inode = dummy_parent,
        .buffer_size = sizeof(dirbuf),
        .is_directory = true};
    int8_t ret;
    syscall(1, (uint32_t)&req, (uint32_t)&ret, 0);
    if (ret != 0)
        return false;
    struct EXT2DirectoryEntry *dot = (void *)dirbuf;
    *inode_out = dot->inode;
    return true;
}

// Load binary file into fixed memory and execute it as external app
static bool exec_external(const char *path_input)
{
    uint32_t parent;
    char name[32];
    uint8_t name_len;
    if (!resolve_parent_and_name(path_input, &parent, name, &name_len))
    {
        syscall(6, (uint32_t)"exec: not found\n", strlen("exec: not found\n"), 0x0F);
        return false;
    }

    struct EXT2DriverRequest req = {
        .buf = APP_LOAD_ADDR,
        .name = name,
        .name_len = name_len,
        .parent_inode = parent,
        .buffer_size = APP_MAX_SIZE,
        .is_directory = false};
    int8_t ret = 0;
    syscall(0, (uint32_t)&req, (uint32_t)&ret, 0);
    if (ret != 0)
    {
        syscall(6, (uint32_t)"exec: load failed\n", strlen("exec: load failed\n"), 0x0F);
        return false;
    }

    void (*app_entry)(void) = (void (*)(void))APP_LOAD_ADDR;
    app_entry();
    return true;
}
void find_recursive(uint32_t dir_inode, const char *path, const char *target, int depth)
{
    if (depth > 8)
        return;

    uint32_t base_addr = 0x220000 + (depth * 0x800); // 2KB per level
    char *find_buf = (char *)base_addr;
    char *find_name_buf = (char *)(base_addr + 0x400); // +1KB
    char *find_path_buf = (char *)(base_addr + 0x600); // +1.5KB

    struct EXT2DriverRequest req = {
        .buf = (void *)find_buf,
        .name = ".",
        .name_len = 1,
        .parent_inode = dir_inode,
        .buffer_size = 1024,
        .is_directory = true};

    int8_t retcode;
    syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);
    if (retcode != 0)
        return;

    uint32_t offset = 0;

    while (offset < 1024)
    {
        struct EXT2DirectoryEntry *e = (struct EXT2DirectoryEntry *)(find_buf + offset);
        if (e->rec_len == 0)
            break;
        if (e->inode == 0)
        {
            offset += e->rec_len;
            continue;
        }

        char *name = (char *)(find_buf + offset + sizeof(struct EXT2DirectoryEntry));

        // Copy name safely
        uint32_t name_len = e->name_len;
        if (name_len > 200)
            name_len = 200;
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
        if (path_len > 200)
        {
            offset += e->rec_len;
            continue;
        }

        memcpy(find_path_buf, path, path_len);
        find_path_buf[path_len] = '\0';

        if (path_len > 0 && find_path_buf[path_len - 1] != '/')
        {
            find_path_buf[path_len] = '/';
            find_path_buf[path_len + 1] = '\0';
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
            if (buf == '\b')
            {
                if (idx > 0)
                {
                    syscall(5, (uint32_t)buf, 0x0F, 0);
                    idx--;
                }
                continue;
            }
            syscall(5, (uint32_t)buf, 0xF, 0);

            if (buf == '\n')
                break;
            if (buf == '\0')
                break;

            if (idx < MAX_CMD_LEN - 1)
            {
                line[idx++] = buf;
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

            // Check for quotes
            if (*p == '"' || *p == '\'')
            {
                char quote = *p;
                p++; // skip opening quote
                argv[argc++] = p;
                // Find closing quote
                while (*p && *p != quote)
                    p++;
                if (*p == quote)
                {
                    *p++ = '\0'; // replace closing quote with null
                }
            }
            else
            {
                argv[argc++] = p;
                while (*p && *p != ' ')
                    p++;
                if (*p)
                    *p++ = '\0';
            }
        }

        if (argc == 0)
        {
            syscall(6, (uint32_t)"Empty command\n", 15, 0x0F);
            continue;
        }

        if (strcmp(argv[0], "echo") == 0)
        {
            // Cek apakah ada redirection
            bool has_redirect = false;
            char *redirect_file = NULL;
            int content_end = argc;

            // Cari operator >
            for (int i = 1; i < argc; i++)
            {
                if (strcmp(argv[i], ">") == 0)
                {
                    has_redirect = true;
                    if (i + 1 < argc)
                    {
                        redirect_file = argv[i + 1];
                        content_end = i;
                    }
                    break;
                }
            }

            if (has_redirect && redirect_file)
            {
                // Mode write ke file
                char content[1024];
                content[0] = '\0';

                // Gabungkan semua argumen sebelum operator redirection
                for (int i = 1; i < content_end; i++)
                {
                    strcat(content, argv[i]);
                    if (i + 1 < content_end)
                        strcat(content, " ");
                }
                strcat(content, "\n");

                uint32_t parent_inode;
                char name[32];
                uint8_t name_len;
                if (!resolve_parent_and_name(redirect_file, &parent_inode, name, &name_len))
                {
                    syscall(6, (uint32_t)"echo: invalid path\n", strlen("echo: invalid path\n"), 0x0F);
                    continue;
                }

                // Hapus file lama jika ada (untuk memastikan overwrite)
                struct EXT2DriverRequest del_req = {
                    .buf = NULL,
                    .name = name,
                    .name_len = name_len,
                    .parent_inode = parent_inode,
                    .buffer_size = 0,
                    .is_directory = false};
                uint32_t del_ret;
                syscall(3, (uint32_t)&del_req, (uint32_t)&del_ret, 0); // syscall delete (ignore error)

                // Mode overwrite - tulis file baru
                struct EXT2DriverRequest write_req = {
                    .buf = content,
                    .name = name,
                    .name_len = name_len,
                    .parent_inode = parent_inode,
                    .buffer_size = (uint32_t)strlen(content),
                    .is_directory = false};
                uint32_t ret = 0;
                syscall(2, (uint32_t)&write_req, (uint32_t)&ret, 0);
            }
            else
            {
                // Mode normal: print ke stdout
                for (int i = 1; i < argc; i++)
                {
                    size_t len = strlen(argv[i]);
                    syscall(6, (uint32_t)argv[i], (uint32_t)len, 0x0F);
                    if (i + 1 < argc)
                        syscall(6, (uint32_t)" ", 1, 0x0F);
                }
                syscall(5, (uint32_t)'\n', 0x0F, 0);
            }
            continue;
        }
        else if (strcmp(argv[0], "mkdir") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"mkdir: missing argument\n", strlen("mkdir: missing argument\n"), 0xF);
                continue;
            }
            uint32_t parent_inode;
            char name[32];
            uint8_t name_len;
            if (!resolve_parent_and_name(argv[1], &parent_inode, name, &name_len))
            {
                syscall(6, (uint32_t)"mkdir: invalid path\n", strlen("mkdir: invalid path\n"), 0xF);
                continue;
            }
            struct EXT2DriverRequest req = {
                .buf = NULL,
                .name = name,
                .name_len = name_len,
                .parent_inode = parent_inode,
                .buffer_size = 0,
                .is_directory = true};
            uint32_t retcode = 0;
            syscall(2, (uint32_t)&req, (uint32_t)&retcode, 0);
            continue;
        }
        else if (strcmp(argv[0], "ls") == 0)
        {
            uint32_t target_inode = current_directory;
            if (argc >= 2)
            {
                if (!resolve_dir_inode(argv[1], &target_inode))
                {
                    syscall(6, (uint32_t)"ls: directory not found\n",
                            strlen("ls: directory not found\n"), 0x0F);
                    continue;
                }
            }

            struct EXT2DriverRequest req = {
                .buf = (void *)0x210000,
                .name = ".",
                .name_len = 1,
                .parent_inode = target_inode,
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
            if (argc < 2)
            {
                syscall(6, (uint32_t)"cd: missing argument\n", 23, 0x0F);
                continue;
            }

            if (!resolve_and_change_dir(argv[1]))
            {
                syscall(6, (uint32_t)"cd: directory not found\n",
                        strlen("cd: directory not found\n"), 0x0F);
            }
            continue;
        }
        else if (strcmp(argv[0], "cat") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"cat: usage: cat <file> or cat > <file>\n",
                        strlen("cat: usage: cat <file> or cat > <file>\n"), 0x0F);
                continue;
            }

            // Cek apakah mode write: cat > file.txt
            if (argc >= 3 && strcmp(argv[1], ">") == 0)
            {
                // Mode write interaktif
                char *target_file = argv[2];
                char input_buffer[2048];
                int buf_idx = 0;

                syscall(6, (uint32_t)"(Type text, press Ctrl+D on new line to finish)\n",
                        strlen("(Type text, press Ctrl+D on new line to finish)\n"), 0x0F);

                // Baca input dari user sampai Ctrl+D (End of Transmission = 0x04)
                while (buf_idx < 2047)
                {
                    char ch;
                    syscall(4, (uint32_t)&ch, 0, 0);

                    // Ctrl+D detection
                    if (ch == 0x04)
                    {
                        break;
                    }

                    if (ch == '\b')
                    {
                        if (buf_idx > 0)
                        {
                            syscall(5, (uint32_t)ch, 0x0F, 0);
                            buf_idx--;
                        }
                        continue;
                    }

                    syscall(5, (uint32_t)ch, 0x0F, 0);
                    input_buffer[buf_idx++] = ch;

                    if (ch == '\n' && buf_idx >= 2 && input_buffer[buf_idx - 2] == '\n')
                    {
                        // Double newline bisa jadi tanda selesai (opsional)
                        // Tapi kita tetap tunggu Ctrl+D
                    }
                }
                input_buffer[buf_idx] = '\0';

                // Tulis ke file
                uint32_t parent_inode;
                char name[32];
                uint8_t name_len;
                if (!resolve_parent_and_name(target_file, &parent_inode, name, &name_len))
                {
                    syscall(6, (uint32_t)"cat: invalid path\n", strlen("cat: invalid path\n"), 0x0F);
                    continue;
                }

                struct EXT2DriverRequest write_req = {
                    .buf = input_buffer,
                    .name = name,
                    .name_len = name_len,
                    .parent_inode = parent_inode,
                    .buffer_size = (uint32_t)strlen(input_buffer),
                    .is_directory = false};
                uint32_t ret = 0;
                syscall(2, (uint32_t)&write_req, (uint32_t)&ret, 0);

                if ((int32_t)ret == 0)
                {
                    syscall(6, (uint32_t)"\n", 1, 0x0F);
                }
                else
                {
                    syscall(6, (uint32_t)"cat: write failed\n", strlen("cat: write failed\n"), 0x0F);
                }
                continue;
            }

            // Mode read: cat file.txt
            uint32_t parent_inode;
            char name[32];
            uint8_t name_len;
            if (!resolve_parent_and_name(argv[1], &parent_inode, name, &name_len))
            {
                syscall(6, (uint32_t)"cat: file not found\n", 21, 0x0F);
                continue;
            }
            struct EXT2DriverRequest req = {
                .buf = (void *)0x200000,
                .name = name,
                .name_len = name_len,
                .parent_inode = parent_inode,
                .buffer_size = 8192,
                .is_directory = false};

            int8_t retcode;
            syscall(0, (uint32_t)&req, (uint32_t)&retcode, 0);

            if (retcode == 0)
            {
                char *content = (char *)req.buf;
                uint32_t len = strlen(content);
                syscall(6, (uint32_t)content, len, 0x0F);
                if (len == 0 || content[len - 1] != '\n')
                {
                    syscall(5, (uint32_t)'\n', 0x0F, 0);
                }
            }
            else
            {
                syscall(6, (uint32_t)"cat: file not found\n", 21, 0x0F);
            }
            continue;
        }
        else if (strcmp(argv[0], "grep") == 0)
        {
            if (argc < 3)
            {
                syscall(6, (uint32_t)"grep: usage: grep <pattern> <file>\n",
                        strlen("grep: usage: grep <pattern> <file>\n"), 0x0F);
                continue;
            }

            const char *pattern = argv[1];
            uint32_t pattern_len = strlen(pattern);
            if (pattern_len == 0)
            {
                syscall(6, (uint32_t)"grep: empty pattern\n",
                        strlen("grep: empty pattern\n"), 0x0F);
                continue;
            }

            uint32_t parent_inode;
            char name[32];
            uint8_t name_len;
            if (!resolve_parent_and_name(argv[2], &parent_inode, name, &name_len))
            {
                syscall(6, (uint32_t)"grep: file not found\n",
                        strlen("grep: file not found\n"), 0x0F);
                continue;
            }

            struct EXT2DriverRequest req = {
                .buf = (void *)0x200000,
                .name = name,
                .name_len = name_len,
                .parent_inode = parent_inode,
                .buffer_size = 8192,
                .is_directory = false};

            int8_t retcode;
            syscall(0, (uint32_t)&req, (uint32_t)&retcode, 0);
            if (retcode != 0)
            {
                syscall(6, (uint32_t)"grep: file not found\n",
                        strlen("grep: file not found\n"), 0x0F);
                continue;
            }

            char *content = (char *)req.buf;
            uint32_t len = strlen(content);
            char *line_start = content;

            for (uint32_t i = 0; i <= len; i++)
            {
                if (content[i] == '\n' || content[i] == '\0')
                {
                    char saved = content[i];
                    content[i] = '\0';
                    if (strstr(line_start, pattern) != NULL)
                    {
                        syscall(6, (uint32_t)line_start, strlen(line_start), 0x0F);
                        syscall(6, (uint32_t)"\n", 1, 0x0F);
                    }
                    content[i] = saved;
                    if (saved == '\0')
                        break;
                    line_start = content + i + 1;
                }
            }
            continue;
        }
        else if (strcmp(argv[0], "clear") == 0)
        {
            syscall(8, 0, 0, 0);
            continue;
        }
        else if (strcmp(argv[0], "rm") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"rm: usage: rm [-r] <name>\n",
                        strlen("rm: usage: rm [-r] <name>\n"), 0x0F);
                continue;
            }

            bool recursive = false;
            char *target_path = NULL;

            if (argc >= 3 && strcmp(argv[1], "-r") == 0)
            {
                recursive = true;
                target_path = argv[2];
            }
            else
            {
                target_path = argv[1];
            }

            if (target_path == NULL)
            {
                syscall(6, (uint32_t)"rm: missing name\n",
                        strlen("rm: missing name\n"), 0x0F);
                continue;
            }

            uint32_t parent_inode;
            char name[32];
            uint8_t name_len;
            if (!resolve_parent_and_name(target_path, &parent_inode, name, &name_len))
            {
                syscall(6, (uint32_t)"rm: not found\n",
                        strlen("rm: not found\n"), 0x0F);
                continue;
            }

            struct EXT2DriverRequest req = {
                .buf = NULL,
                .name = name,
                .name_len = name_len,
                .parent_inode = parent_inode,
                .buffer_size = 0,
                .is_directory = recursive ? true : false // dipakai sebagai "recursive" flag
            };

            uint32_t retcode;
            syscall(3, (uint32_t)&req, (uint32_t)&retcode, 0);

            if (retcode == 0)
            {
                // sukses, diam aja
            }
            else if (retcode == 1)
            {
                syscall(6, (uint32_t)"rm: not found\n",
                        strlen("rm: not found\n"), 0x0F);
            }
            else if (retcode == 2)
            {
                // mode non-recursive, directory not empty
                syscall(6, (uint32_t)"rm: directory not empty (use rm -r)\n",
                        strlen("rm: directory not empty (use rm -r)\n"), 0x0F);
            }
            else
            {
                syscall(6, (uint32_t)"rm: error\n",
                        strlen("rm: error\n"), 0x0F);
            }

            continue;
        }
        else if (strcmp(argv[0], "find") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"find: missing name\n",
                        strlen("find: missing name\n"), 0x0F);
                continue;
            }
            uint32_t start_inode = current_directory;
            char start_path[32];
            strcpy(start_path, current_directory_name);
            if (argc >= 3 && strcmp(argv[1], "/") == 0)
            {
                start_inode = 1;
                strcpy(start_path, "/");
                find_recursive(start_inode, start_path, argv[2], 0);
            }
            else
            {
                find_recursive(start_inode, start_path, argv[1], 0);
            }
            continue;
        }
        else if (strcmp(argv[0], "cp") == 0)
        {
            if (argc < 3)
            {
                syscall(6, (uint32_t)"cp: usage: cp <src> <dst>\n",
                        strlen("cp: usage: cp <src> <dst>\n"), 0x0F);
                continue;
            }

            uint32_t src_parent, dst_parent;
            char src_name[32], dst_name[32];
            uint8_t src_name_len, dst_name_len;

            if (!resolve_parent_and_name(argv[1], &src_parent, src_name, &src_name_len))
            {
                syscall(6, (uint32_t)"cp: source not found\n",
                        strlen("cp: source not found\n"), 0x0F);
                continue;
            }
            if (!resolve_parent_and_name(argv[2], &dst_parent, dst_name, &dst_name_len))
            {
                syscall(6, (uint32_t)"cp: invalid destination\n",
                        strlen("cp: invalid destination\n"), 0x0F);
                continue;
            }

            uint8_t *file_buf = (uint8_t *)0x230000; // 4KB buffer at fixed address
            int8_t retcode;

            struct EXT2DriverRequest rreq = {
                .buf = file_buf,
                .name = src_name,
                .name_len = src_name_len,
                .parent_inode = src_parent,
                .buffer_size = 4096,
                .is_directory = false};

            syscall(0, (uint32_t)&rreq, (uint32_t)&retcode, 0);
            if (retcode != 0)
            {
                syscall(6, (uint32_t)"cp: source not found or not a file\n",
                        strlen("cp: source not found or not a file\n"), 0x0F);
                continue;
            }

            uint32_t filesize = strlen((char *)file_buf);

            struct EXT2DriverRequest wreq = {
                .buf = file_buf,
                .name = dst_name,
                .name_len = dst_name_len,
                .parent_inode = dst_parent,
                .buffer_size = filesize,
                .is_directory = false};

            syscall(2, (uint32_t)&wreq, (uint32_t)&retcode, 0);
            if (retcode != 0)
            {
                syscall(6, (uint32_t)"cp: write failed\n",
                        strlen("cp: write failed\n"), 0x0F);
                continue;
            }

            continue;
        }
        else if (strcmp(argv[0], "mv") == 0)
        {
            if (argc < 3)
            {
                syscall(6, (uint32_t)"mv: usage: mv <src> <dst>\n",
                        strlen("mv: usage: mv <src> <dst>\n"), 0x0F);
                continue;
            }

            uint32_t src_parent, dst_parent;
            char src_name[32], dst_name[32];
            uint8_t src_name_len, dst_name_len;

            if (!resolve_parent_and_name(argv[1], &src_parent, src_name, &src_name_len))
            {
                syscall(6, (uint32_t)"mv: source not found\n",
                        strlen("mv: source not found\n"), 0x0F);
                continue;
            }

            uint32_t dst_dir_inode = 0;
            if (resolve_dir_inode(argv[2], &dst_dir_inode))
            {
                dst_parent = dst_dir_inode;
                dst_name_len = src_name_len;
                memcpy(dst_name, src_name, src_name_len);
                dst_name[src_name_len] = '\0';
            }
            else
            {
                if (!resolve_parent_and_name(argv[2], &dst_parent, dst_name, &dst_name_len))
                {
                    syscall(6, (uint32_t)"mv: invalid destination\n",
                            strlen("mv: invalid destination\n"), 0x0F);
                    continue;
                }
            }

            struct EXT2MoveRequest mreq = {
                .src_name = src_name,
                .src_name_len = src_name_len,
                .src_parent_inode = src_parent,
                .dst_name = dst_name,
                .dst_name_len = dst_name_len,
                .dst_parent_inode = dst_parent};

            int8_t retcode = 0;
            syscall(9, (uint32_t)&mreq, (uint32_t)&retcode, 0);

            if (retcode == 0)
            {
                // success
            }
            else if (retcode == 1)
            {
                syscall(6, (uint32_t)"mv: source not found\n",
                        strlen("mv: source not found\n"), 0x0F);
            }
            else if (retcode == 2)
            {
                syscall(6, (uint32_t)"mv: destination exists\n",
                        strlen("mv: destination exists\n"), 0x0F);
            }
            else if (retcode == 4)
            {
                syscall(6, (uint32_t)"mv: no space in destination directory\n",
                        strlen("mv: no space in destination directory\n"), 0x0F);
            }
            else
            {
                syscall(6, (uint32_t)"mv: error\n",
                        strlen("mv: error\n"), 0x0F);
            }
            continue;
        }
        else if (strcmp(argv[0], "clock") == 0)
        {
            syscall(11, 0, 0, 0); // start clock drawing in kernel scheduler
            syscall(6, (uint32_t)"Clock started (bottom-right)\n",
                    strlen("Clock started (bottom-right)\n"), 0x0F);
            continue;
        }
        else if (strcmp(argv[0], "ps") == 0)
        {
            syscall(6, (uint32_t)"PID  NAME   STATE\n", strlen("PID  NAME   STATE\n"), 0x0F);
            syscall(6, (uint32_t)"1    shell  RUNNING\n", strlen("1    shell  RUNNING\n"), 0x0F);
            bool clock_on = false;
            syscall(13, 0, (uint32_t)&clock_on, 0);
            if (clock_on)
            {
                syscall(6, (uint32_t)"2    clock  RUNNING\n", strlen("2    clock  RUNNING\n"), 0x0F);
            }
            continue;
        }
        else if (strcmp(argv[0], "kill") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"kill: usage: kill <pid>\n",
                        strlen("kill: usage: kill <pid>\n"), 0x0F);
                continue;
            }
            int pid;
            if (!parse_int(argv[1], &pid) || pid <= 0)
            {
                syscall(6, (uint32_t)"kill: invalid pid\n",
                        strlen("kill: invalid pid\n"), 0x0F);
                continue;
            }
            if (pid == PID_SHELL)
            {
                syscall(6, (uint32_t)"kill: cannot terminate shell\n",
                        strlen("kill: cannot terminate shell\n"), 0x0F);
                continue;
            }
            if (pid == PID_CLOCK)
            {
                bool clock_on = false;
                syscall(13, 0, (uint32_t)&clock_on, 0);
                if (!clock_on)
                {
                    syscall(6, (uint32_t)"kill: process not found\n",
                            strlen("kill: process not found\n"), 0x0F);
                    continue;
                }
                syscall(12, 0, 0, 0);
                syscall(6, (uint32_t)"killed pid 2 (clock)\n",
                        strlen("killed pid 2 (clock)\n"), 0x0F);
                continue;
            }
            syscall(6, (uint32_t)"kill: process not found\n",
                    strlen("kill: process not found\n"), 0x0F);
            continue;
        }
        else if (strcmp(argv[0], "exec") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"exec: usage: exec <program>\n",
                        strlen("exec: usage: exec <program>\n"), 0x0F);
            }
            else
            {
                exec_external(argv[1]);
            }
            continue;
        }
        else if (strcmp(argv[0], "setenv") == 0)
        {
            if (argc < 3)
            {
                syscall(6, (uint32_t)"setenv: usage: setenv KEY VALUE\n",
                        strlen("setenv: usage: setenv KEY VALUE\n"), 0x0F);
            }
            else
            {
                env_set(argv[1], argv[2]);
            }
            continue;
        }
        else if (strcmp(argv[0], "getenv") == 0)
        {
            if (argc < 2)
            {
                syscall(6, (uint32_t)"getenv: usage: getenv KEY\n",
                        strlen("getenv: usage: getenv KEY\n"), 0x0F);
            }
            else
            {
                const char *val = env_get(argv[1]);
                if (val)
                {
                    syscall(6, (uint32_t)val, strlen(val), 0x0F);
                    syscall(6, (uint32_t)"\n", 1, 0x0F);
                }
                else
                {
                    syscall(6, (uint32_t)"(not set)\n", strlen("(not set)\n"), 0x0F);
                }
            }
            continue;
        }
        else if (strcmp(argv[0], "printenv") == 0)
        {
            env_print_all();
            continue;
        }

        else
        {
            syscall(6, (uint32_t)"Unknown command: ", 17, 0x0F);
            syscall(6, (uint32_t)argv[0], strlen(argv[0]), 0x0F);
            syscall(6, (uint32_t)"\n", 1, 0x0F);
            continue;
        }
    }
    return 0;
}
