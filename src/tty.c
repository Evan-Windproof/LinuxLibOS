/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <abi/keyboard.h>
#include <abi/vga.h>
#include "libos.h"
#include "tty.h"

#define CELLS (VGA_COLUMNS * VGA_ROWS)
#define MAX_CSI_VALUE 999u

static struct resource_handle video, keyboard;
static volatile uint16_t *const screen = (void *)VGA_USER_ADDRESS;
static unsigned row, column, foreground;
static int reverse, wrap;
static unsigned escape, csi_private, csi_index, csi_values[2];
static struct termios mode;
static const struct termios default_mode = {
    .c_iflag = ICRNL, .c_oflag = OPOST, .c_cflag = CS8,
    .c_lflag = ICANON | ECHO, .c_cc = {1,0}
};

static unsigned char queued[4], queue_pos, queue_size;
static unsigned char line[128];
static size_t line_pos, line_size;
static int line_ready;
static unsigned shifts, controls;
static int extended, caps, caps_down;
static unsigned pause_left;
volatile unsigned tty_overflows;

static void discard_input(void)
{
    queue_pos = queue_size = 0;
    line_pos = line_size = 0;
    line_ready = 0;
    shifts = controls = 0;
    extended = caps = caps_down = 0;
    pause_left = 0;
}

static int fail(int error)
{
    errno = error;
    return -1;
}

static uint16_t cell(unsigned byte)
{
    unsigned attribute = reverse ? foreground << 4 : foreground;
    return (uint16_t)(attribute << 8 | byte);
}

static void clear_cells(unsigned first, unsigned end)
{
    for (; first < end; ++first) screen[first] = cell(' ');
}

static void down(void)
{
    if (++row < VGA_ROWS) return;
    row = VGA_ROWS - 1;
    for (unsigned i = 0; i < CELLS - VGA_COLUMNS; ++i) screen[i] = screen[i + VGA_COLUMNS];
    clear_cells(CELLS - VGA_COLUMNS,CELLS);
}

static void put_character(unsigned char byte)
{
    switch (byte) {
    case '\r': column = 0; wrap = 0; return;
    case '\n':
        if (mode.c_oflag & OPOST) column = 0;
        down();
        wrap = 0;
        return;
    case '\b': if (!wrap && column) --column; wrap = 0; return;
    case '\t':
        do { put_character(' '); } while (!wrap && column % 8);
        return;
    default: if (byte < ' ' || byte > '~') return;
    }
    if (wrap) {
        column = 0;
        down();
    }
    screen[row * VGA_COLUMNS + column] = cell(byte);
    wrap = column == VGA_COLUMNS - 1;
    if (!wrap) ++column;
}

static unsigned clamp(unsigned value, unsigned max)
{
    return value > max ? max : value;
}

static void csi(unsigned char command)
{
    static const unsigned ansi_colors[] = {0,4,2,6,1,5,3,7};
    unsigned a = csi_values[0], b = csi_values[1];
    unsigned distance = a ? a : 1;
    unsigned cursor = row * VGA_COLUMNS + column;
    if (csi_private) return; /* ABI exposes cursor position, not visibility. */
    switch (command) {
    case 'H': case 'f':
        row = clamp(distance - 1,VGA_ROWS - 1);
        column = clamp(b ? b - 1 : 0,VGA_COLUMNS - 1);
        break;
    case 'A': row -= clamp(distance,row); break;
    case 'B': row = clamp(row + distance,VGA_ROWS - 1); break;
    case 'C': column = clamp(column + distance,VGA_COLUMNS - 1); break;
    case 'D': column -= clamp(distance,column); break;
    case 'J':
        if (a == 2) clear_cells(0,CELLS);
        else if (a == 0) clear_cells(cursor + (unsigned)wrap,CELLS);
        else if (a == 1) clear_cells(0,cursor + 1);
        break;
    case 'K': {
        unsigned start = row * VGA_COLUMNS;
        if (a == 2) clear_cells(start,start + VGA_COLUMNS);
        else if (a == 0) clear_cells(cursor + (unsigned)wrap,start + VGA_COLUMNS);
        else if (a == 1) clear_cells(start,cursor + 1);
        break;
    }
    case 'm':
        for (unsigned i = 0; i <= csi_index; ++i) {
            unsigned value = csi_values[i];
            if (value == 0 || value == 39) foreground = 7;
            if (value == 0 || value == 27) reverse = 0;
            if (value == 7) reverse = 1;
            if (value >= 30 && value <= 37) foreground = ansi_colors[value - 30];
        }
        return;
    default: return;
    }
    wrap = 0;
}

static void output(unsigned char byte)
{
    if (byte == 27) {
        escape = 1;
        return;
    }
    if (escape == 1) {
        escape = byte == '[' ? 2 : 0;
        csi_index = csi_private = csi_values[0] = csi_values[1] = 0;
    } else if (escape >= 2) {
        if (byte >= 0x40 && byte <= 0x7e) {
            if (escape == 2) csi(byte);
            escape = 0;
        } else if (escape == 2 && isdigit(byte)) {
            unsigned *value = &csi_values[csi_index];
            *value = clamp(*value * 10 + byte - '0',MAX_CSI_VALUE);
        } else if (escape == 2 && byte == ';' && !csi_index) {
            ++csi_index;
        } else if (escape == 2 && byte == '?' && !csi_index && !csi_values[0]) {
            csi_private = 1;
        } else {
            escape = 3;
        }
    } else {
        put_character(byte);
    }
}

static int update_cursor(void)
{
    int status = liberums_call(SYS_VGA_CURSOR,video.index,video.generation,
                               row * VGA_COLUMNS + column,0,0);
    return status < 0 ? fail(-status) : 0;
}

int tty_open(void)
{
    if (video.index || keyboard.index) return fail(EBUSY);
    uint32_t extra;
    int status = liberums_call(SYS_VGA_INFO,VGA_DEVICE_TEXT,0,0,0,&extra);
    if (status < 0) return fail(-status);
    if ((unsigned)status != (VGA_COLUMNS | VGA_ROWS << 16) || extra != VGA_MAPPING_BYTES)
        return fail(EIO);
    status = liberums_call(SYS_KBD_INFO,KBD_DEVICE_PS2,0,0,0,&extra);
    if (status < 0) return fail(-status);
    if (extra != KBD_SCANSET1 || status != KBD_QUEUE_CAPACITY) return fail(EIO);

    status = liberums_call(SYS_VGA_ACQUIRE,VGA_DEVICE_TEXT,RESOURCE_READ | RESOURCE_WRITE,
                           0,0,&video.generation);
    if (status < 0) return fail(-status);
    video.index = (uint32_t)status;
    status = liberums_call(SYS_VGA_MAP,video.index,video.generation,VGA_USER_ADDRESS,
                           RESOURCE_READ | RESOURCE_WRITE,0);
    if (!status) {
        status = liberums_call(SYS_KBD_ACQUIRE,KBD_DEVICE_PS2,0,0,0,&keyboard.generation);
        if (status > 0) {
            keyboard.index = (uint32_t)status;
            row = column = escape = 0;
            foreground = 7;
            reverse = wrap = 0;
            mode = default_mode;
            discard_input();
            clear_cells(0,CELLS);
            if (!update_cursor()) return 0;
            status = -errno;
        }
    }
    int error = status < 0 ? -status : EIO;
    if (tty_close()) return -1;
    return fail(error);
}

int tty_close(void)
{
    int status = 0;
    if (keyboard.index) {
        status = liberums_call(SYS_KBD_RELEASE,keyboard.index,keyboard.generation,0,0,0);
        if (!status) keyboard = (struct resource_handle){0};
    }
    if (video.index) {
        int release = liberums_call(SYS_VGA_RELEASE,video.index,video.generation,0,0,0);
        if (!release) video = (struct resource_handle){0};
        if (!status) status = release;
    }
    discard_input();
    return status < 0 ? fail(-status) : 0;
}

int tty_ready(void) { return video.index && keyboard.index; }

ssize_t tty_write(const void *buffer, size_t count)
{
    if (!tty_ready()) return fail(ENOTTY);
    const unsigned char *bytes = buffer;
    for (size_t i = 0; i < count; ++i) output(bytes[i]);
    if (update_cursor()) return -1;
    return (ssize_t)count;
}

/* Translated set 1, US layout. Zero entries are modifier or unsupported keys. */
static const unsigned char keys[] = {
    [0x01] = 27,
    [0x02] = '1','2','3','4','5','6','7','8','9','0','-','=',127,'\t',
    [0x10] = 'q','w','e','r','t','y','u','i','o','p','[',']','\r',
    [0x1e] = 'a','s','d','f','g','h','j','k','l',';','\'','`',
    [0x2b] = '\\','z','x','c','v','b','n','m',',','.','/',
    [0x39] = ' '
};

static const unsigned char shifted[] = {
    [0x02] = '!','@','#','$','%','^','&','*','(',')','_','+',
    [0x1a] = '{','}',
    [0x27] = ':','"','~',
    [0x2b] = '|',
    [0x33] = '<','>','?'
};

static void emit_sequence(unsigned char third, unsigned char fourth)
{
    queued[0] = 27;
    queued[1] = '[';
    queued[2] = third;
    queued[3] = fourth;
    queue_size = fourth ? 4 : 3;
}

static void decode(unsigned raw)
{
    if (pause_left) { --pause_left; return; }
    if (raw == 0xe1) { pause_left = 5; extended = 0; return; }
    if (raw == 0xe0) { extended = 1; return; }
    int ext = extended;
    extended = 0;
    unsigned code = raw & 0x7f;
    int down_key = !(raw & 0x80);
    unsigned bit = code == 0x2a ? 1u : 2u;
    if (!ext && (code == 0x2a || code == 0x36)) {
        shifts = down_key ? shifts | bit : shifts & ~bit;
        return;
    }
    if (code == 0x1d) {
        bit = ext ? 2u : 1u;
        controls = down_key ? controls | bit : controls & ~bit;
        return;
    }
    if (!ext && code == 0x3a) {
        if (down_key && !caps_down) caps = !caps;
        caps_down = down_key;
        return;
    }
    if (!down_key) return;
    unsigned char byte = code < sizeof(keys) ? keys[code] : 0;
    if (ext) {
        /* Canonical input has no cursor-key line editing. */
        if (mode.c_lflag & ICANON) return;
        switch (code) {
        case 0x48: emit_sequence('A',0); return;
        case 0x50: emit_sequence('B',0); return;
        case 0x4d: emit_sequence('C',0); return;
        case 0x4b: emit_sequence('D',0); return;
        case 0x47: emit_sequence('H',0); return;
        case 0x4f: emit_sequence('F',0); return;
        case 0x53: emit_sequence('3','~'); return;
        case 0x49: emit_sequence('5','~'); return;
        case 0x51: emit_sequence('6','~'); return;
        case 0x1c: byte = '\r'; break;
        default: return;
        }
    }
    if (!byte) return;
    if (byte >= 'a' && byte <= 'z') {
        if (controls) byte -= 'a' - 1;
        else if ((shifts != 0) != caps) byte -= 'a' - 'A';
    } else if (shifts && code < sizeof(shifted) && shifted[code]) {
        byte = shifted[code];
    }
    if (byte == '\r' && (mode.c_iflag & ICRNL)) byte = '\n';
    queued[0] = byte;
    queue_size = 1;
}

static int wait_key(void)
{
    while (queue_pos == queue_size) {
        queue_pos = queue_size = 0;
        libos_safe_point();
        uint32_t raw;
        int status = liberums_call(SYS_KBD_POLL,keyboard.index,keyboard.generation,0,0,&raw);
        if (status == RES_EAGAIN) {
            liberums_call(SYS_YIELD,0,0,0,0,0);
        } else if (status == RES_EOVERFLOW) {
            discard_input();
            ++tty_overflows;
            return fail(EOVERFLOW);
        } else if (status < 0) {
            return fail(-status);
        } else {
            decode(raw);
        }
    }
    return 0;
}

static void echo_backspace(void)
{
    put_character('\b');
    put_character(' ');
    put_character('\b');
}

static int collect_line(void)
{
    while (!line_ready) {
        if (wait_key()) return -1;
        unsigned char byte = queued[queue_pos++];
        int echo = mode.c_lflag & ECHO;
        if (byte == 127 || byte == '\b') {
            if (line_size) {
                --line_size;
                if (echo) echo_backspace();
            }
        } else if (byte == '\n' || byte == '\r') {
            line[line_size++] = byte;
            line_ready = 1;
            if (echo) {
                put_character('\r');
                put_character('\n');
            }
        } else if (isprint(byte) && line_size < sizeof(line) - 1) {
            line[line_size++] = byte;
            if (echo) put_character(byte);
        }
        if (echo && update_cursor()) return -1;
    }
    return 0;
}

ssize_t tty_read(void *buffer, size_t count)
{
    if (!tty_ready()) return fail(ENOTTY);
    if (mode.c_lflag & ICANON) {
        if (collect_line()) return -1;
        size_t size = line_size - line_pos;
        if (count > size) count = size;
        memcpy(buffer,line + line_pos,count);
        line_pos += count;
        if (line_pos == line_size) {
            line_pos = line_size = 0;
            line_ready = 0;
        }
        return (ssize_t)count;
    }
    if (wait_key()) return -1;
    size_t available = queue_size - queue_pos;
    if (count > available) count = available;
    memcpy(buffer,queued + queue_pos,count);
    if (mode.c_lflag & ECHO) {
        for (size_t i = 0; i < count; ++i) put_character(queued[queue_pos + i]);
        if (update_cursor()) return -1;
    }
    queue_pos += (unsigned)count;
    return (ssize_t)count;
}

ssize_t tty_escape_byte(int fd, char *byte)
{
    /* Continuations were generated from the same physical key. A lone
       Escape cannot consume a later character from the hardware queue. */
    if (fd != STDIN_FILENO || !isatty(fd) || (mode.c_lflag & ICANON))
        return fail(ENOTTY);
    if (queue_pos == queue_size) return 0;
    *byte = (char)queued[queue_pos++];
    return 1;
}

int tcgetattr(int fd, struct termios *attributes)
{
    if (!isatty(fd)) return -1;
    if (!attributes) return fail(EINVAL);
    *attributes = mode;
    return 0;
}

int tcsetattr(int fd, int action, const struct termios *attributes)
{
    if (!isatty(fd)) return -1;
    if (!attributes || (action != TCSANOW && action != TCSAFLUSH) ||
        (attributes->c_iflag & ~ICRNL) || (attributes->c_oflag & ~OPOST) ||
        attributes->c_cflag != CS8 || (attributes->c_lflag & ~(ICANON | ECHO)) ||
        attributes->c_cc[VMIN] != 1 || attributes->c_cc[VTIME]) return fail(EINVAL);

    if (action == TCSAFLUSH) {
        for (;;) {
            libos_safe_point();
            int status = liberums_call(SYS_KBD_POLL,keyboard.index,keyboard.generation,0,0,0);
            if (status == RES_EAGAIN) break;
            if (status == RES_EOVERFLOW) {
                ++tty_overflows;
                continue;
            }
            if (status < 0) return fail(-status);
        }
        discard_input();
    } else if ((mode.c_lflag ^ attributes->c_lflag) & ICANON) {
        if (line_size || queue_pos != queue_size) return fail(EBUSY);
    }
    mode = *attributes;
    return 0;
}
