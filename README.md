![Credit: 朧月](megumi.jpg)

# Template Dasar IF2130 Sistem Operasi - 2025/2026
Template dasar untuk Tugas Besar IF2130 - Sistem Operasi 2025/2026

## README Specification

Pada file README, minimal kalian harus memiliki hal berikut:

Nama Kelompok
- Kevin Wirya Valerian 13524019
- Made Branenda Jordhy 13524026
- Emilio Justin 13524043
- Farrel Limjaya 13524046

- Daftar Isi

- Cara Run
1. Persiapan Awal(Menginstall tools yang dibutuhkan)

Jalankan perintah-perintah ini di WSL

sudo apt update
sudo apt install make nasm gcc qemu genisoimage


2. Struktur Folder

Buat struktur repositori seperti struktur berikut:

/
├─ src/
│  ├─ kernel-entrypoint.s
│  ├─ kernel.c
│  └─ cpu/
│     └─ gdt.c
│  └─ linker.ld
│  └─ menu.lst
├─ bin/
├─ other/
│  └─ grub1
├─ Makefile
└─ README.md

3. Build Kernel

Untuk melakukan kompilasi kernel dan membuat file kernel (ELF32 executable), kita jalankan perintah:

make kernel


Setelah ini, file hasil kompilasi (kernel) akan muncul di folder:

bin/kernel

4. Buat File ISO

Untuk membuat image ISO bootable (menggunakan GRUB sebagai bootloader), kita jalankan perintah:

make iso


File ISO akan terbentuk di:

bin/OS2025.iso

5. Jalankan di QEMU

Gunakan QEMU untuk menjalankan sistem operasi yang sudah dibuat:

make run

ataupun jika make file blm jadi, jalankan perintah ini:

qemu-system-i386 -cdrom bin/OS2025.iso

Jika berhasil, jendela QEMU akan muncul dan menampilkan tampilan awal kernel

- Fitur yang Dibuat : Toolchain, Kernel, GDT
  
- Maskot Kelompok Kalian : Fushiguro Megumi
