![Fushiguro Megumi](mascot.png)

# megumOS - Operating System Project

**IF2130 Sistem Operasi - 2025/2026**

---

## 👥 Kelompok MeguMOS

| Nama                 | NIM      |
| -------------------- | -------- |
| Kevin Wirya Valerian | 13524019 |
| Made Branenda Jordhy | 13524026 |
| Emilio Justin        | 13524043 |
| Farrel Limjaya       | 13524046 |

---

## 📋 Daftar Isi

- [Tentang Proyek](#-tentang-proyek)
- [Fitur](#-fitur)
- [Instalasi](#-instalasi)
- [Cara Menjalankan](#-cara-menjalankan)
- [Struktur Proyek](#-struktur-proyek)
- [Dokumentasi](#-dokumentasi)
- [Maskot](#-maskot)

---

## 📖 Tentang Proyek

megumOS adalah sistem operasi sederhana yang dikembangkan sebagai bagian dari Tugas Besar IF2130 Sistem Operasi. Proyek ini mengimplementasikan konsep-konsep dasar sistem operasi termasuk manajemen memori, filesystem, dan shell interaktif.

---

## ✨ Fitur

### Sistem Dasar

- **Toolchain** - Environment pengembangan lengkap
- **Kernel** - Core sistem operasi
- **GDT (Global Descriptor Table)** - Manajemen segmentasi memori
- **IDT (Interrupt Descriptor Table)** - Handling interrupt
- **Paging** - Virtual memory management
- **Keyboard Driver** - Input handling dengan support Ctrl+D

### Filesystem

- **EXT2 Filesystem** - Implementasi filesystem
- **File Operations** - Read, write, delete, copy, move
- **Directory Management** - Create, navigate, remove

### Shell & Commands

- **Interactive Shell** - Command-line interface
- **File Management**
  - `cat` - Display file contents / Write to file interactively
  - `echo` - Print text / Write to file with `>` redirection
  - `mkdir` - Create directories
  - `ls` - List directory contents
  - `rm` - Remove files/directories
  - `cp` - Copy files
  - `mv` - Move/rename files
- **Navigation**
  - `cd` - Change directory
  - `pwd` - Print working directory (via prompt)
- **Utilities**
  - `grep` - Search pattern in files
  - `find` - Search files by name
  - `clear` - Clear screen
  - `clock` - Display real-time clock (GMT+7)
- **Process Management**
  - `ps` - List running processes
  - `kill` - Terminate processes
  - `exec` - Execute external programs
- **Environment Variables**
  - `setenv` - Set environment variable
  - `getenv` - Get environment variable
  - `printenv` - Print all environment variables

### Graphics

- **Framebuffer** - Text display
- **Graphics Mode** - Custom graphics rendering
- **Real-time Clock** - Display system time in GMT+7 (Jakarta)

---

## 🔧 Instalasi

### Prerequisites

Pastikan sistem Anda memiliki tools berikut:

- **WSL (Windows Subsystem for Linux)** atau **Linux**
- **GCC** - GNU Compiler Collection
- **NASM** - Netwide Assembler
- **Make** - Build automation tool
- **QEMU** - Hardware emulator
- **genisoimage** - ISO image creator

### Langkah Instalasi

1. **Update package manager dan install dependencies**

```bash
sudo apt update
sudo apt install make nasm gcc qemu-system-x86 genisoimage
```

2. **Clone repository**

```bash
git clone https://github.com/labsister23/tugas-besar-os-megumos.git
cd tugas-besar-os-megumos
```

---

## 🚀 Cara Menjalankan

### 1. Build Kernel

Compile kernel dan dependencies:

```bash
make build
```

File hasil kompilasi akan tersimpan di folder `bin/`.

### 2. Jalankan di QEMU

Jalankan OS menggunakan emulator QEMU:

```bash
make run
```

Atau secara manual:

```bash
qemu-system-i386 -s -S -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk -cdrom bin/OS2025.iso
```

### 3. Jalankan dengan Debug

Untuk menjalankan dengan GDB debugger:

```bash
make debug
```

### 4. Clean Build Files

Untuk membersihkan file hasil kompilasi:

```bash
make clean
```

---

## 📁 Struktur Proyek

```
tugas-besar-os-megumos/
├── bin/                        # Output binary files
│   ├── kernel                  # Kernel executable
│   ├── OS2025.iso             # Bootable ISO
│   └── storage.bin            # Disk storage
├── src/                       # Source code
│   ├── kernel-entrypoint.s    # Kernel entry point
│   ├── kernel.c               # Main kernel
│   ├── user-shell.c           # Shell implementation
│   ├── linker.ld              # Linker script
│   ├── menu.lst               # GRUB config
│   ├── app/                   # User applications
│   ├── cpu/                   # CPU management
│   │   ├── gdt.c              # Global Descriptor Table
│   │   ├── idt.c              # Interrupt Descriptor Table
│   │   ├── interrupt.c        # Interrupt handlers
│   │   └── portio.c           # Port I/O operations
│   ├── driver/                # Device drivers
│   │   ├── keyboard.c         # Keyboard driver
│   │   ├── disk.c             # Disk driver
│   │   └── cmos.c             # CMOS/RTC driver
│   ├── filesystem/            # Filesystem implementation
│   │   └── ext2.c             # EXT2 filesystem
│   ├── graphics/              # Graphics subsystem
│   │   └── graphics.c         # Graphics rendering
│   ├── memory/                # Memory management
│   │   └── paging.c           # Paging implementation
│   ├── process/               # Process management
│   │   └── scheduler.c        # Process scheduler
│   ├── stdlib/                # Standard library
│   │   └── string.c           # String operations
│   ├── text/                  # Text rendering
│   │   └── framebuffer.c      # Framebuffer driver
│   └── header/                # Header files
├── tools/                     # Build tools
├── other/                     # Additional resources
│   └── grub1                  # GRUB bootloader
├── Makefile                   # Build configuration
└── README.md                  # This file
```
