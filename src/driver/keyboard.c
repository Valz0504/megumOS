#include "../header/driver/keyboard.h"
#include "../header/cpu/portio.h"

static struct KeyboardDriverState keyboard_state = {
  .read_extended_mode = false,
  .keyboard_input_on = false,
  .keyboard_buffer = '\0',
  .shift_pressed = false
};

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

const char keyboard_scancode_shift_to_ascii_map[256] = {
      0, 0x1B, '!', '@', '#', '$', '%', '^',  '&', '*', '(',  ')',  '_', '+', '\b', '\t',
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',  'O', 'P', '{',  '}', '\n',   0,  'A',  'S',
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':', '"',  '~',  0,   '|',  'Z',  'X',  'C',  'V',
    'B',  'N', 'M', '<', '>', '?',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
};

void keyboard_isr(void) {
    if (!keyboard_state.keyboard_input_on) {
        pic_ack(IRQ_KEYBOARD);
        return;
    }

    uint8_t scancode = in(KEYBOARD_DATA_PORT);

    // SHIFT pressed?
    if (scancode == 0x2A || scancode == 0x36) {   // LShift / RShift down
        keyboard_state.shift_pressed = true;
        pic_ack(IRQ_KEYBOARD);
        return;
    }

    // SHIFT released?
    if (scancode == 0xAA || scancode == 0xB6) {   // LShift / RShift up
        keyboard_state.shift_pressed = false;
        pic_ack(IRQ_KEYBOARD);
        return;
    }

    // Ignore release scancodes (bit 7 = 1)
    if (scancode & 0x80) {
        pic_ack(IRQ_KEYBOARD);
        return;
    }

    char ascii;

    if (keyboard_state.shift_pressed)
        ascii = keyboard_scancode_shift_to_ascii_map[scancode];
    else
        ascii = keyboard_scancode_1_to_ascii_map[scancode];

    if (ascii != '\0') {
        keyboard_state.keyboard_buffer = ascii;
    }

    pic_ack(IRQ_KEYBOARD);
}


void keyboard_state_activate(void) {
  keyboard_state.keyboard_input_on = true;
}

void keyboard_state_deactivate(void) {
  keyboard_state.keyboard_input_on = false;
}

void get_keyboard_buffer(char *buf) {
    uint32_t saved_flags;
    __asm__ volatile("pushf\n\tpop %0" : "=r"(saved_flags) :: "memory");

    while (keyboard_state.keyboard_buffer == '\0') {
        __asm__ volatile("hlt" ::: "memory");
    }

    *buf = keyboard_state.keyboard_buffer;
    keyboard_state.keyboard_buffer = '\0';

    if ((saved_flags & 0x200) == 0) {
        __asm__ volatile("cli" ::: "memory");
    }
}
