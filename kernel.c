/* ==========================================================================
   1. Port I/O Helpers
   ========================================================================== */
static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile("in %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ==========================================================================
   2. Global Descriptor Table (GDT) Setup
   ========================================================================== */
struct gdt_entry_struct {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char  base_middle;
    unsigned char  access;
    unsigned char  granularity;
    unsigned char  base_high;
} __attribute__((packed));
typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));
typedef struct gdt_ptr_struct gdt_ptr_t;

gdt_entry_t gdt_entries[5];
gdt_ptr_t   gdt_ptr;

void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void gdt_flush(unsigned int gdt_ptr_addr) {
    __asm__ volatile (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushl $0x08\n\t"
        "pushl $1f\n\t"
        "lretl\n\t"
        "1:\n\t"
        : : "r"(gdt_ptr_addr) : "eax", "memory"
    );
}

void init_gdt(void) {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base  = (unsigned int)&gdt_entries;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((unsigned int)&gdt_ptr);
}

/* ==========================================================================
   3. VGA Video Terminal Driver (With Hardware Cursor support)
   ========================================================================== */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static inline unsigned char make_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline unsigned short make_vga_entry(unsigned char uc, unsigned char color) {
    return (unsigned short) uc | (unsigned short) color << 8;
}

int terminal_row = 0;
int terminal_column = 0;
unsigned char terminal_color;
volatile unsigned short* terminal_buffer = (volatile unsigned short*) 0xB8000;

void update_cursor(int x, int y) {
    unsigned short pos = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void enable_cursor(unsigned char cursor_start, unsigned char cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void terminal_scroll(void) {
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            terminal_buffer[y * 80 + x] = terminal_buffer[(y + 1) * 80 + x];
        }
    }
    for (int x = 0; x < 80; x++) {
        terminal_buffer[24 * 80 + x] = make_vga_entry(' ', terminal_color);
    }
    terminal_row = 24;
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            terminal_buffer[y * 80 + x] = make_vga_entry(' ', terminal_color);
        }
    }
    enable_cursor(14, 15);
    update_cursor(0, 0);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row >= 25) {
            terminal_scroll();
        }
        update_cursor(terminal_column, terminal_row);
        return;
    }
    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_buffer[terminal_row * 80 + terminal_column] = make_vga_entry(' ', terminal_color);
        }
        update_cursor(terminal_column, terminal_row);
        return;
    }

    const int index = terminal_row * 80 + terminal_column;
    terminal_buffer[index] = make_vga_entry(c, terminal_color);
    if (++terminal_column >= 80) {
        terminal_column = 0;
        if (++terminal_row >= 25) {
            terminal_scroll();
        }
    }
    update_cursor(terminal_column, terminal_row);
}

void terminal_writestring(const char* data) {
    for (int i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

/* ==========================================================================
   4. Polled Keyboard Driver
   ========================================================================== */
char scancode_to_ascii(unsigned char scancode) {
    static const char kbd_us[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
      '9', '0', '-', '=', '\b',
      '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
     '\'', '`',   0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
        0, '*',   0, ' '
    };
    
    if (scancode < 128) {
        return kbd_us[scancode];
    }
    return 0;
}

unsigned char keyboard_read_scancode(void) {
    while ((inb(0x64) & 1) == 0);
    return inb(0x60);
}

char keyboard_getchar(void) {
    while (1) {
        unsigned char scancode = keyboard_read_scancode();
        if (scancode & 0x80) {
            continue;
        }
        char ascii = scancode_to_ascii(scancode);
        if (ascii != 0) {
            return ascii;
        }
    }
}

/* ==========================================================================
   5. Basic Helper Functions
   ========================================================================== */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int str_startswith(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

/* ==========================================================================
   6. Shell & Kernel Entry
   ========================================================================== */
void read_line(char* buffer, int max_len) {
    int index = 0;
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            terminal_putchar('\n');
            buffer[index] = '\0';
            return;
        }
        else if (c == '\b') {
            if (index > 0) {
                index--;
                terminal_putchar('\b');
            }
        }
        else {
            if (index < max_len - 1) {
                buffer[index++] = c;
                terminal_putchar(c);
            }
        }
    }
}

void execute_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("Available commands:\n");
        terminal_writestring("  help         - Show this list\n");
        terminal_writestring("  about        - Information about openOS\n");
        terminal_writestring("  echo [text]  - Repeat the input text\n");
        terminal_writestring("  clear        - Clear the screen\n");
        terminal_writestring("  halt         - Turn off / stop CPU\n");
    } 
    else if (strcmp(cmd, "about") == 0) {
        terminal_writestring("openOS Kernel v0.1.0\n");
        terminal_writestring("A tiny x86 hobby operating system.\n");
    } 
    else if (strcmp(cmd, "echo") == 0) {
        terminal_putchar('\n');
    }
    else if (str_startswith(cmd, "echo ")) {
        terminal_writestring(cmd + 5);
        terminal_putchar('\n');
    }
    else if (strcmp(cmd, "clear") == 0) {
        terminal_initialize();
    } 
    else if (strcmp(cmd, "halt") == 0) {
        terminal_writestring("Halting system. Goodbye!\n");
        __asm__ volatile("cli; hlt");
    } 
    else if (strcmp(cmd, "") == 0) {
        /* Do nothing */
    } 
    else {
        terminal_writestring("Command not found: ");
        terminal_writestring(cmd);
        terminal_writestring("\nType 'help' for a list of commands.\n");
    }
}

void kernel_main(void) {
    terminal_initialize();

    terminal_writestring("[ OK ] Initializing VGA Text Terminal\n");
    terminal_writestring("[ OK ] Loading Polled PS/2 Keyboard Driver\n");
    
    terminal_writestring("[INFO] Setting up GDT segments...\n");
    init_gdt();
    terminal_writestring("[ OK ] Global Descriptor Table active (Kernel and User segments)\n");

    terminal_writestring("[SUCCESS] Boot Phase Completed!\n\n");
    terminal_writestring("Welcome to openOS! Type 'help' to get started.\n\n");

    char input_buffer[256];

    while (1) {
        terminal_writestring("openOS> ");
        read_line(input_buffer, 256);
        execute_command(input_buffer);
    }
}
