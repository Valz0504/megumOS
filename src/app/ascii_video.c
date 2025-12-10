#include <stdint.h>
#include <stdbool.h>
#include "../header/stdlib/string.h"
#include "../header/filesystem/ext2.h"
#include "../header/graphics/graphics.h"

// Minimal syscall wrapper
static void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

#define COLOR_BG        DEFAULT_COLOR_BG      // palette index for pure black
#define TEXT_COLOR      COLOR_ASCII_WHITE   // dedicated white slot
#define MAX_FRAME_CELLS (64 * 25)
#define VIDEO_MAGIC     0x41534349u  // "ASCI"
#define VIDEO_PREFIX    "badapple_"
#define VIDEO_EXT       ".avf"

struct AsciiVideoHeader {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint16_t reserved;
    uint32_t frame_count;
} __attribute__((packed));

static char frame_buf[MAX_FRAME_CELLS];

static void print(const char *text)
{
    if (!text) return;
    syscall(6, (uint32_t)text, strlen(text), TEXT_COLOR);
}

static bool read_chunk(const char *name, uint32_t offset, uint32_t length, void *buf, uint32_t *out_read)
{
    struct EXT2PartialRequest preq = {
        .driver = {
            .buf = buf,
            .name = (char *)name,
            .name_len = (uint8_t)strlen(name),
            .parent_inode = 1,  // root directory
            .buffer_size = length,
            .is_directory = false
        },
        .offset = offset,
        .length = length
    };
    int8_t ret = 0;
    uint32_t bytes_read = 0;
    syscall(16, (uint32_t)&preq, (uint32_t)&ret, (uint32_t)&bytes_read);
    if (out_read) *out_read = bytes_read;
    return ret == 0;
}

static bool parse_header(const uint8_t *buf, uint32_t buf_size, struct AsciiVideoHeader *out)
{
    if (buf_size < sizeof(struct AsciiVideoHeader)) return false;
    const struct AsciiVideoHeader *hdr = (const struct AsciiVideoHeader *)buf;
    if (hdr->magic != VIDEO_MAGIC) return false;
    if (hdr->width == 0 || hdr->height == 0) return false;
    if (hdr->width > 64 || hdr->height > 25) return false;
    if (out) *out = *hdr;
    return true;
}

static void format_chunk_name(uint32_t idx, char *out)
{
    // Produce badapple_000.avf style names
    const char prefix[] = VIDEO_PREFIX;
    const char ext[] = VIDEO_EXT;
    uint32_t p = 0;
    for (; prefix[p]; ++p) out[p] = prefix[p];
    // three digits zero-padded
    out[p + 0] = (char)('0' + ((idx / 100) % 10));
    out[p + 1] = (char)('0' + ((idx / 10) % 10));
    out[p + 2] = (char)('0' + (idx % 10));
    uint32_t e = 0;
    while (ext[e]) {
        out[p + 3 + e] = ext[e];
        ++e;
    }
    out[p + 3 + e] = '\0';
}

int main(void)
{
    print("ASCII video player starting...\n");
    syscall(7, 0, 0, 0);
    syscall(14, COLOR_BG, 0, 0);

    uint32_t chunk_index = 0;
    bool any_played = false;

    while (1) {
        char fname[32];
        format_chunk_name(chunk_index, fname);

        uint8_t header_buf[sizeof(struct AsciiVideoHeader)] = {0};
        uint32_t header_read = 0;
        if (!read_chunk(fname, 0, sizeof(header_buf), header_buf, &header_read) ||
            header_read < sizeof(header_buf)) {
            break; // no more chunks
        }

        struct AsciiVideoHeader hdr;
        if (!parse_header(header_buf, header_read, &hdr)) {
            break;
        }

        uint32_t frame_bytes = (uint32_t)hdr.width * (uint32_t)hdr.height;
        if (frame_bytes > sizeof(frame_buf)) {
            print("Frame too large for buffer\n");
            break;
        }

        if (!any_played) {
            print("Playing ASCII video (Ctrl+C to stop)\n");
            any_played = true;
        }

        uint32_t frame_offset = sizeof(struct AsciiVideoHeader);
        for (uint32_t idx = 0; idx < hdr.frame_count; ++idx) {
            char key = 0;
            syscall(4, (uint32_t)&key, 1, 1);
            if (key == 3 || key == 'q' || key == 'Q') {
                print("\nStopped by user\n");
                syscall(8, 0, 0, 0);
                return 0;
            }

            uint32_t bytes_read = 0;
            if (!read_chunk(fname, frame_offset, frame_bytes, frame_buf, &bytes_read) ||
                bytes_read < frame_bytes) {
                print("\nStopped (incomplete frame)\n");
                syscall(8, 0, 0, 0);
                return 0;
            }

            uint32_t packed = ((uint32_t)TEXT_COLOR << 8) | hdr.height;
            syscall(17, (uint32_t)frame_buf, hdr.width, packed);
            frame_offset += frame_bytes;
        }

        chunk_index++;
    }

    if (!any_played) {
        print("Missing badapple_000.avf in root\n");
    } else {
        print("\nDone\n");
    }
    syscall(8, 0, 0, 0); // restore wallpaper/text background for shell
    return 0;
}
