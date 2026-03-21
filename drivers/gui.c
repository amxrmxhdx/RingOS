#include "../include/gui.h"
#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/fat32.h"
#include "../include/vga.h"
#include "../include/types.h"
#include "../include/string.h"
#include "../include/desk/desk.h"

#define KBD_STATUS_PORT 0x64
#define KBD_DATA_PORT   0x60

/* Bochs/QEMU VBE DISPI registers */
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID          0x0
#define VBE_DISPI_INDEX_XRES        0x1
#define VBE_DISPI_INDEX_YRES        0x2
#define VBE_DISPI_INDEX_BPP         0x3
#define VBE_DISPI_INDEX_ENABLE      0x4
#define VBE_DISPI_INDEX_VIRT_WIDTH  0x6

#define VBE_DISPI_DISABLED    0x00
#define VBE_DISPI_ENABLED     0x01
#define VBE_DISPI_LFB_ENABLED 0x40

#define FB_PHYS_ADDR 0xE0000000u
#define FB_W 800
#define FB_H 600

#define BACKBUF_MAX_W 1024
#define BACKBUF_MAX_H 768

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define CURSOR_HALF 6
#define CURSOR_SIZE (CURSOR_HALF * 2 + 1)
#define MAX_BPP_BYTES 4

#define TASKBAR_H 36
#define START_W 80

#define WIN_BORDER 1
#define WIN_TITLE_H 24
#define WIN_MIN_W 180
#define WIN_MIN_H 120
#define WIN_CLOSE_W 16
#define WIN_CLOSE_H 14

#define MAX_WINDOWS 16
#define MAX_APPS 10
#define TASK_BUTTON_MAX 6

#define TERM_MAX_LINES 160
#define TERM_MAX_COLS  110
#define TERM_INPUT_MAX 120

#define EXPL_MAX_ITEMS   128
#define EXPL_NAME_MAX    20
#define EXPL_PREVIEW_MAX 900

#define EXPL_BTN_UP_X    8
#define EXPL_BTN_REF_X   48
#define EXPL_BTN_OPEN_X  94
#define EXPL_BTN_REN_X   146
#define EXPL_BTN_NEWF_X  202
#define EXPL_BTN_NEWD_X  260
#define EXPL_BTN_DEL_X   318
#define EXPL_BTN_Y       8
#define EXPL_BTN_W       36
#define EXPL_BTN_H       16
#define EXPL_LIST_X      8
#define EXPL_LIST_Y      30
#define EXPL_LIST_W      290
#define EXPL_ROW_H       13
#define EXPL_MAX_ROWS    18
#define EXPL_PREVIEW_X   308
#define EXPL_PREVIEW_Y   30

#define MOUSE_INVERT_X 0
#define MOUSE_INVERT_Y 0

#define IDE_MAX_LINES 128
#define IDE_MAX_COLS  96
#define IDE_CMD_MAX   40

#define IDE_TEXT_X    44
#define IDE_TEXT_Y    30
#define IDE_LINE_H    9

#define IDE_BTN_OPEN_X  8
#define IDE_BTN_SAVE_X  58
#define IDE_BTN_NEW_X   108
#define IDE_BTN_Y       4
#define IDE_BTN_W       44
#define IDE_BTN_H       16

enum hit_part_t {
    HIT_NONE,
    HIT_CLIENT,
    HIT_TITLE,
    HIT_CLOSE
};

typedef struct {
    bool used;
    char name[28];
    desk_app_entry_fn entry;
} desk_app_t;

typedef struct {
    desk_window_t* win;
    bool initialized;
    char lines[TERM_MAX_LINES][TERM_MAX_COLS + 1];
    uint32_t fg[TERM_MAX_LINES][TERM_MAX_COLS];
    uint32_t bg[TERM_MAX_LINES][TERM_MAX_COLS];
    uint32_t current_fg;
    uint32_t current_bg;
    int line_count;
    int cursor_col;
    char input[TERM_INPUT_MAX + 1];
    int input_len;
} terminal_state_t;

typedef struct {
    char fat_name[12];
    char display[EXPL_NAME_MAX + 1];
    uint32_t size;
    uint8_t attr;
} explorer_item_t;

typedef struct {
    desk_window_t* win;
    bool initialized;
    int item_count;
    int selected;
    int scroll;
    explorer_item_t items[EXPL_MAX_ITEMS];
    char preview[EXPL_PREVIEW_MAX + 1];
    char status[96];
    bool rename_mode;
    char rename_buf[EXPL_NAME_MAX + 1];
    int rename_len;
    int rename_idx;
    int last_click_idx;
    uint32_t last_click_tick;
} explorer_state_t;

typedef struct {
    desk_window_t* win;
    bool initialized;
    char filename[24];
    char lines[IDE_MAX_LINES][IDE_MAX_COLS + 1];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_y;
    bool insert_mode;
    bool cmd_mode;
    char cmd[IDE_CMD_MAX + 1];
    int cmd_len;
    bool open_dialog;
    char open_name[24];
    int open_len;
    char status[96];
} ide_state_t;

struct desk_window {
    bool used;
    bool visible;
    bool dirty;
    int x;
    int y;
    int w;
    int h;
    uint32_t flags;
    char title[40];
    desk_paint_fn paint;
    void* user_data;
};

static volatile uint8_t* g_fb = (volatile uint8_t*)FB_PHYS_ADDR;
static uint32_t g_fb_phys = FB_PHYS_ADDR;
static uint16_t g_fb_w = FB_W;
static uint16_t g_fb_h = FB_H;
static uint16_t g_fb_bpp = 32;
static uint16_t g_fb_virt_w = FB_W;
static uint32_t g_fb_pitch = FB_W * 4u;
static int g_draw_w = FB_W;
static int g_draw_h = FB_H;
static uint32_t g_backbuf[BACKBUF_MAX_W * BACKBUF_MAX_H];

static int g_mouse_x = FB_W / 2;
static int g_mouse_y = FB_H / 2;
static bool g_mouse_left_down = false;

static uint8_t g_cursor_under[CURSOR_SIZE * CURSOR_SIZE * MAX_BPP_BYTES];
static int g_cursor_prev_x = 0;
static int g_cursor_prev_y = 0;
static bool g_cursor_visible = false;

static struct desk_window g_windows[MAX_WINDOWS];
static int g_zorder[MAX_WINDOWS];
static int g_zcount = 0;
static int g_active_window = -1;

static desk_app_t g_apps[MAX_APPS];
static int g_app_count = 0;

static bool g_start_menu_open = false;
static bool g_desktop_needs_redraw = true;
static bool g_request_shutdown = false;

static int g_drag_window = -1;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;
static int g_spawn_offset = 0;
static uint32_t g_ui_tick = 0;
static terminal_state_t g_terminal;
static explorer_state_t g_explorer;
static explorer_state_t* g_explorer_fill_ctx = NULL;
static ide_state_t g_ide;

/* Colors in 0x00RRGGBB */
#define C_DESK_TOP       0x0d4fa3
#define C_DESK_BOTTOM    0x083b7d
#define C_DESK_SHAPE     0x2e7fd8
#define C_TASKBAR        0x111722
#define C_TASKBAR_TOP    0x27384f
#define C_TASK_BTN       0x1b2433
#define C_TASK_BTN_ACT   0x214a8e
#define C_START_BTN      0x19622f
#define C_START_BTN_OPEN 0x25853f
#define C_MENU_BG        0x0f1825
#define C_MENU_HOVER     0x1c406f
#define C_WINDOW         0xf1f3f6
#define C_WINDOW_BORDER  0x6a778a
#define C_TITLE_ACTIVE   0x0078d7
#define C_TITLE_INACT    0x7a8697
#define C_CLIENT_BG      0xffffff
#define C_CLOSE_BG       0xd14c4c
#define C_TEXT           0xe9eef7
#define C_TEXT_DARK      0x1a232f
#define C_TEXT_MUTED     0x495667
#define C_WHITE          0xffffff

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void str_copy(char* dst, int cap, const char* src) {
    int i = 0;
    if (!dst || cap <= 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool str_equal(const char* a, const char* b) {
    int i = 0;
    if (!a || !b) return false;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static void str_append(char* dst, int cap, const char* src) {
    int di = 0;
    int si = 0;
    while (di < cap - 1 && dst[di]) di++;
    while (di < cap - 1 && src && src[si]) {
        dst[di++] = src[si++];
    }
    dst[di] = '\0';
}

static void u32_to_dec(uint32_t v, char* out, int cap) {
    char tmp[16];
    int n = 0;
    int i;

    if (cap <= 0) return;
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v && n < 15) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    i = 0;
    while (n > 0 && i < cap - 1) {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';
}

static void to_fat83(const char* src, char out[12]) {
    int bi = 0;
    int si = 0;
    int ei = 8;

    for (int i = 0; i < 11; i++) out[i] = ' ';
    out[11] = '\0';

    while (src && src[si] && src[si] != '.' && bi < 8) {
        char c = src[si++];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[bi++] = c;
    }

    while (src && src[si] && src[si] != '.') si++;
    if (src && src[si] == '.') {
        si++;
        while (src[si] && ei < 11) {
            char c = src[si++];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            out[ei++] = c;
        }
    }
}

static void fat83_to_display(const char fat[12], uint8_t attr, char* out, int cap) {
    int di = 0;
    int i;

    if (cap <= 0) return;
    out[0] = '\0';

    for (i = 0; i < 8 && di < cap - 1; i++) {
        if (fat[i] == ' ') break;
        out[di++] = fat[i];
    }

    if ((attr & ATTR_DIRECTORY) == 0) {
        int has_ext = 0;
        for (i = 8; i < 11; i++) {
            if (fat[i] != ' ') {
                has_ext = 1;
                break;
            }
        }
        if (has_ext && di < cap - 1) out[di++] = '.';
        for (i = 8; i < 11 && di < cap - 1; i++) {
            if (fat[i] == ' ') break;
            out[di++] = fat[i];
        }
    }

    out[di] = '\0';
}

static uint32_t vga_color_to_rgb(uint8_t c) {
    static const uint32_t pal[16] = {
        0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
        0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
        0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
        0xff5555, 0xff55ff, 0xffff55, 0xffffff
    };
    return pal[c & 0x0f];
}

static void term_reset(terminal_state_t* t) {
    t->line_count = 1;
    t->cursor_col = 0;
    t->input_len = 0;
    t->current_fg = vga_color_to_rgb(VGA_WHITE);
    t->current_bg = vga_color_to_rgb(VGA_BLACK);
    t->input[0] = '\0';
    t->lines[0][0] = '\0';
    for (int i = 0; i < TERM_MAX_LINES; i++) {
        t->lines[i][0] = '\0';
        for (int j = 0; j < TERM_MAX_COLS; j++) {
            t->fg[i][j] = t->current_fg;
            t->bg[i][j] = t->current_bg;
        }
    }
}

static void term_scroll_if_needed(terminal_state_t* t) {
    if (t->line_count < TERM_MAX_LINES) return;
    for (int i = 1; i < TERM_MAX_LINES; i++) {
        str_copy(t->lines[i - 1], TERM_MAX_COLS + 1, t->lines[i]);
        for (int j = 0; j < TERM_MAX_COLS; j++) {
            t->fg[i - 1][j] = t->fg[i][j];
            t->bg[i - 1][j] = t->bg[i][j];
        }
    }
    t->line_count = TERM_MAX_LINES;
    t->lines[t->line_count - 1][0] = '\0';
    for (int j = 0; j < TERM_MAX_COLS; j++) {
        t->fg[t->line_count - 1][j] = t->current_fg;
        t->bg[t->line_count - 1][j] = t->current_bg;
    }
    t->cursor_col = 0;
}

static void term_newline(terminal_state_t* t) {
    t->line_count++;
    term_scroll_if_needed(t);
    t->lines[t->line_count - 1][0] = '\0';
    for (int j = 0; j < TERM_MAX_COLS; j++) {
        t->fg[t->line_count - 1][j] = t->current_fg;
        t->bg[t->line_count - 1][j] = t->current_bg;
    }
    t->cursor_col = 0;
}

static void term_putc(terminal_state_t* t, char c) {
    if (c == '\r') return;

    if (c == '\n') {
        term_newline(t);
        return;
    }

    if (c == '\b') {
        if (t->cursor_col > 0) {
            t->cursor_col--;
            t->lines[t->line_count - 1][t->cursor_col] = '\0';
            t->fg[t->line_count - 1][t->cursor_col] = t->current_fg;
            t->bg[t->line_count - 1][t->cursor_col] = t->current_bg;
        }
        return;
    }

    if ((uint8_t)c < 32) return;

    if (t->cursor_col >= TERM_MAX_COLS) {
        term_newline(t);
    }

    t->lines[t->line_count - 1][t->cursor_col++] = c;
    t->fg[t->line_count - 1][t->cursor_col - 1] = t->current_fg;
    t->bg[t->line_count - 1][t->cursor_col - 1] = t->current_bg;
    t->lines[t->line_count - 1][t->cursor_col] = '\0';
}

static void term_puts(terminal_state_t* t, const char* s) {
    int i = 0;
    if (!s) return;
    while (s[i]) {
        term_putc(t, s[i]);
        i++;
    }
}

static void term_write_prompt(terminal_state_t* t) {
    term_puts(t, "user@");
    term_puts(t, fat32_get_current_path());
    term_puts(t, "> ");
}

static void explorer_set_status(const char* msg) {
    str_copy(g_explorer.status, (int)sizeof(g_explorer.status), msg ? msg : "");
}

static void explorer_mark_dirty(void) {
    if (g_explorer.win) desk_window_request_repaint(g_explorer.win);
}

static void explorer_select_clamp(void) {
    if (g_explorer.item_count <= 0) {
        g_explorer.selected = -1;
        g_explorer.scroll = 0;
        return;
    }
    if (g_explorer.selected < 0) g_explorer.selected = 0;
    if (g_explorer.selected >= g_explorer.item_count) g_explorer.selected = g_explorer.item_count - 1;
    if (g_explorer.scroll < 0) g_explorer.scroll = 0;
    if (g_explorer.scroll > g_explorer.selected) g_explorer.scroll = g_explorer.selected;
    if (g_explorer.selected >= g_explorer.scroll + EXPL_MAX_ROWS) {
        g_explorer.scroll = g_explorer.selected - EXPL_MAX_ROWS + 1;
    }
}

static void explorer_list_callback(const char* name, uint32_t size, uint8_t attr) {
    explorer_state_t* ex = g_explorer_fill_ctx;
    explorer_item_t* it;
    if (!ex) return;
    if (ex->item_count >= EXPL_MAX_ITEMS) return;

    it = &ex->items[ex->item_count++];
    for (int i = 0; i < 11; i++) it->fat_name[i] = name[i];
    it->fat_name[11] = '\0';
    it->size = size;
    it->attr = attr;
    fat83_to_display(it->fat_name, it->attr, it->display, EXPL_NAME_MAX + 1);
}

static void explorer_load_preview(void) {
    uint32_t sz = EXPL_PREVIEW_MAX;
    explorer_item_t* it;
    char size_buf[16];
    int start;
    int end;

    g_explorer.preview[0] = '\0';
    if (g_explorer.selected < 0 || g_explorer.selected >= g_explorer.item_count) return;

    it = &g_explorer.items[g_explorer.selected];
    str_copy(g_explorer.preview, EXPL_PREVIEW_MAX + 1, it->display);
    str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, "\n");

    if (it->attr & ATTR_DIRECTORY) {
        str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, "<DIRECTORY>\nClick OPEN to enter.");
        return;
    }

    str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, "SIZE: ");
    u32_to_dec(it->size, size_buf, (int)sizeof(size_buf));
    str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, size_buf);
    str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, " bytes\n\n");

    start = strlen(g_explorer.preview);
    if (!fat32_read_file(it->display, g_explorer.preview + start, &sz)) {
        str_append(g_explorer.preview, EXPL_PREVIEW_MAX + 1, "(Preview unavailable)");
        return;
    }

    end = start + (int)sz;
    if (end > EXPL_PREVIEW_MAX) end = EXPL_PREVIEW_MAX;
    for (int i = start; i < end; i++) {
        unsigned char c = (unsigned char)g_explorer.preview[i];
        if (c == '\r') g_explorer.preview[i] = '\n';
        else if (c < 32 && c != '\n' && c != '\t') g_explorer.preview[i] = '.';
    }
    g_explorer.preview[end] = '\0';
}

static void explorer_refresh(void) {
    g_explorer.item_count = 0;
    g_explorer_fill_ctx = &g_explorer;
    if (!fat32_list_directory(explorer_list_callback)) {
        explorer_set_status("list failed");
    } else {
        explorer_set_status("ready");
    }
    g_explorer_fill_ctx = NULL;
    explorer_select_clamp();
    explorer_load_preview();
    explorer_mark_dirty();
}

static int explorer_find_click_row(int x, int y) {
    int ry = y - EXPL_LIST_Y;
    if (x < EXPL_LIST_X || x >= EXPL_LIST_X + EXPL_LIST_W) return -1;
    if (ry < 0) return -1;
    ry /= EXPL_ROW_H;
    if (ry < 0 || ry >= EXPL_MAX_ROWS) return -1;
    return g_explorer.scroll + ry;
}

static void explorer_open_selected(void) {
    explorer_item_t* it;
    if (g_explorer.selected < 0 || g_explorer.selected >= g_explorer.item_count) return;
    it = &g_explorer.items[g_explorer.selected];

    if (it->attr & ATTR_DIRECTORY) {
        if (fat32_change_directory(it->display)) {
            g_explorer.selected = 0;
            g_explorer.scroll = 0;
            explorer_refresh();
        } else {
            explorer_set_status("open dir failed");
        }
    } else {
        explorer_load_preview();
        explorer_mark_dirty();
    }
}

static void explorer_create_unique(bool dir_mode) {
    char name[16];
    char fat[12];
    char nbuf[8];
    bool ok = false;

    for (int i = 1; i < 1000; i++) {
        str_copy(name, (int)sizeof(name), dir_mode ? "DIR" : "NEW");
        u32_to_dec((uint32_t)i, nbuf, (int)sizeof(nbuf));
        str_append(name, (int)sizeof(name), nbuf);
        if (!dir_mode) str_append(name, (int)sizeof(name), ".TXT");
        to_fat83(name, fat);
        ok = dir_mode ? fat32_create_directory(fat) : fat32_create_file(fat);
        if (ok) break;
    }

    if (ok) {
        explorer_set_status(dir_mode ? "folder created" : "file created");
        explorer_refresh();
    } else {
        explorer_set_status(dir_mode ? "mkdir failed" : "create failed");
    }
}

static void explorer_delete_selected(void) {
    explorer_item_t* it;
    if (g_explorer.selected < 0 || g_explorer.selected >= g_explorer.item_count) return;
    it = &g_explorer.items[g_explorer.selected];

    if (fat32_delete_file(it->fat_name)) {
        explorer_set_status("deleted");
        if (g_explorer.selected > 0) g_explorer.selected--;
        explorer_refresh();
    } else {
        explorer_set_status("delete failed");
    }
}

static void explorer_start_rename(void) {
    explorer_item_t* it;
    if (g_explorer.selected < 0 || g_explorer.selected >= g_explorer.item_count) return;

    it = &g_explorer.items[g_explorer.selected];
    if ((it->attr & ATTR_DIRECTORY) && str_equal(it->display, "..")) {
        explorer_set_status("cannot rename parent");
        explorer_mark_dirty();
        return;
    }

    str_copy(g_explorer.rename_buf, (int)sizeof(g_explorer.rename_buf), it->display);
    g_explorer.rename_len = strlen(g_explorer.rename_buf);
    g_explorer.rename_idx = g_explorer.selected;
    g_explorer.rename_mode = true;
    explorer_set_status("rename: ENTER save, ESC cancel");
    explorer_mark_dirty();
}

static void explorer_cancel_rename(void) {
    g_explorer.rename_mode = false;
    g_explorer.rename_idx = -1;
    g_explorer.rename_len = 0;
    g_explorer.rename_buf[0] = '\0';
    explorer_set_status("rename canceled");
    explorer_mark_dirty();
}

static void explorer_confirm_rename(void) {
    explorer_item_t* it;
    char fat_new[12];

    if (!g_explorer.rename_mode) return;
    if (g_explorer.rename_idx < 0 || g_explorer.rename_idx >= g_explorer.item_count) {
        explorer_cancel_rename();
        return;
    }
    if (g_explorer.rename_len <= 0) {
        explorer_set_status("empty name");
        explorer_mark_dirty();
        return;
    }

    it = &g_explorer.items[g_explorer.rename_idx];
    to_fat83(g_explorer.rename_buf, fat_new);

    if (fat32_rename_file(it->fat_name, fat_new)) {
        g_explorer.rename_mode = false;
        g_explorer.rename_idx = -1;
        g_explorer.rename_len = 0;
        g_explorer.rename_buf[0] = '\0';
        explorer_set_status("renamed");
        explorer_refresh();
    } else {
        explorer_set_status("rename failed");
        explorer_mark_dirty();
    }
}

static bool explorer_handle_click(int cx, int cy) {
    int idx;

    if (!g_explorer.win) return false;

    if (cy >= EXPL_BTN_Y && cy < EXPL_BTN_Y + EXPL_BTN_H) {
        if (cx >= EXPL_BTN_UP_X && cx < EXPL_BTN_UP_X + EXPL_BTN_W) {
            if (fat32_change_directory("..")) explorer_refresh();
            else explorer_set_status("already root");
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_REF_X && cx < EXPL_BTN_REF_X + EXPL_BTN_W) {
            explorer_refresh();
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_OPEN_X && cx < EXPL_BTN_OPEN_X + EXPL_BTN_W + 12) {
            explorer_open_selected();
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_REN_X && cx < EXPL_BTN_REN_X + EXPL_BTN_W + 12) {
            explorer_start_rename();
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_NEWF_X && cx < EXPL_BTN_NEWF_X + EXPL_BTN_W + 10) {
            explorer_create_unique(false);
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_NEWD_X && cx < EXPL_BTN_NEWD_X + EXPL_BTN_W + 10) {
            explorer_create_unique(true);
            g_explorer.last_click_idx = -1;
            return true;
        }
        if (cx >= EXPL_BTN_DEL_X && cx < EXPL_BTN_DEL_X + EXPL_BTN_W + 8) {
            explorer_delete_selected();
            g_explorer.last_click_idx = -1;
            return true;
        }
    }

    idx = explorer_find_click_row(cx, cy);
    if (idx >= 0 && idx < g_explorer.item_count) {
        bool is_double = (idx == g_explorer.last_click_idx) && ((g_ui_tick - g_explorer.last_click_tick) <= 1200u);
        g_explorer.selected = idx;
        explorer_select_clamp();
        explorer_load_preview();
        if (is_double && !g_explorer.rename_mode) {
            explorer_open_selected();
            g_explorer.last_click_idx = -1;
            g_explorer.last_click_tick = 0;
        } else {
            g_explorer.last_click_idx = idx;
            g_explorer.last_click_tick = g_ui_tick;
        }
        explorer_mark_dirty();
        return true;
    }

    g_explorer.last_click_idx = -1;
    g_explorer.last_click_tick = 0;

    return false;
}

static void vbe_write(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static bool vbe_is_available(void) {
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    return id >= 0xB0C0 && id <= 0xB0C5;
}

static void vbe_set_mode(uint16_t w, uint16_t h, uint16_t bpp) {
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, w);
    vbe_write(VBE_DISPI_INDEX_YRES, h);
    vbe_write(VBE_DISPI_INDEX_BPP, bpp);
    vbe_write(VBE_DISPI_INDEX_ENABLE, (uint16_t)(VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED));
}

static bool vbe_try_mode(uint16_t w, uint16_t h, uint16_t bpp) {
    vbe_set_mode(w, h, bpp);
    g_fb_w = vbe_read(VBE_DISPI_INDEX_XRES);
    g_fb_h = vbe_read(VBE_DISPI_INDEX_YRES);
    g_fb_bpp = vbe_read(VBE_DISPI_INDEX_BPP);
    g_fb_virt_w = vbe_read(VBE_DISPI_INDEX_VIRT_WIDTH);
    if (g_fb_virt_w == 0) g_fb_virt_w = g_fb_w;
    g_fb_pitch = (uint32_t)g_fb_virt_w * ((uint32_t)g_fb_bpp / 8u);

    return g_fb_w >= w && g_fb_h >= h && (g_fb_bpp == 16 || g_fb_bpp == 24 || g_fb_bpp == 32);
}

static void vbe_disable(void) {
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, 0);
    vbe_write(VBE_DISPI_INDEX_YRES, 0);
    vbe_write(VBE_DISPI_INDEX_BPP, 0);
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address =
        0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint32_t detect_lfb_phys_addr(void) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id = pci_read32(0, slot, 0, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFFu);
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFFu);
        if (vendor == 0x1234 && (device == 0x1111 || device == 0x1112)) {
            uint32_t bar0 = pci_read32(0, slot, 0, 0x10);
            if ((bar0 & 0x1u) == 0) {
                uint32_t addr = bar0 & 0xFFFFFFF0u;
                if (addr != 0) return addr;
            }
        }
    }
    return FB_PHYS_ADDR;
}

static inline void pset(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)g_draw_w && (unsigned)y < (unsigned)g_draw_h) {
        g_backbuf[(uint32_t)y * (uint32_t)g_draw_w + (uint32_t)x] = c;
    }
}

static void blit_backbuffer(void) {
    for (int y = 0; y < g_draw_h; y++) {
        for (int x = 0; x < g_draw_w; x++) {
            uint32_t c = g_backbuf[(uint32_t)y * (uint32_t)g_draw_w + (uint32_t)x];
            uint8_t r = (uint8_t)((c >> 16) & 0xFFu);
            uint8_t g = (uint8_t)((c >> 8) & 0xFFu);
            uint8_t b = (uint8_t)(c & 0xFFu);
            volatile uint8_t* p = g_fb + (uint32_t)y * g_fb_pitch + (uint32_t)x * ((uint32_t)g_fb_bpp / 8u);

            if (g_fb_bpp == 32) {
                p[0] = b; p[1] = g; p[2] = r; p[3] = 0;
            } else if (g_fb_bpp == 24) {
                p[0] = b; p[1] = g; p[2] = r;
            } else {
                uint16_t c565 =
                    (uint16_t)(((r >> 3) & 0x1F) << 11) |
                    (uint16_t)(((g >> 2) & 0x3F) << 5) |
                    (uint16_t)((b >> 3) & 0x1F);
                p[0] = (uint8_t)(c565 & 0xFF);
                p[1] = (uint8_t)(c565 >> 8);
            }
        }
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            pset(xx, yy, c);
        }
    }
}

static void hline(int x, int y, int w, uint32_t c) {
    if (w <= 0) return;
    for (int i = 0; i < w; i++) pset(x + i, y, c);
}

static void vline(int x, int y, int h, uint32_t c) {
    if (h <= 0) return;
    for (int i = 0; i < h; i++) pset(x, y + i, c);
}

static void frame(int x, int y, int w, int h, uint32_t c) {
    if (w <= 1 || h <= 1) return;
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    vline(x, y, h, c);
    vline(x + w - 1, y, h, c);
}

static void dot(int cx, int cy, int r, uint32_t c) {
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) pset(cx + dx, cy + dy, c);
        }
    }
}

static uint8_t glyph_row(char c, int row) {
    switch (c) {
        case 'a': { const uint8_t g[7] = {0,0,14,1,15,17,15}; return g[row]; }
        case 'b': { const uint8_t g[7] = {16,16,30,17,17,17,30}; return g[row]; }
        case 'c': { const uint8_t g[7] = {0,0,14,17,16,17,14}; return g[row]; }
        case 'd': { const uint8_t g[7] = {1,1,15,17,17,17,15}; return g[row]; }
        case 'e': { const uint8_t g[7] = {0,0,14,17,31,16,14}; return g[row]; }
        case 'f': { const uint8_t g[7] = {6,8,30,8,8,8,8}; return g[row]; }
        case 'g': { const uint8_t g[7] = {0,0,15,17,15,1,14}; return g[row]; }
        case 'h': { const uint8_t g[7] = {16,16,30,17,17,17,17}; return g[row]; }
        case 'i': { const uint8_t g[7] = {4,0,12,4,4,4,14}; return g[row]; }
        case 'j': { const uint8_t g[7] = {2,0,6,2,2,18,12}; return g[row]; }
        case 'k': { const uint8_t g[7] = {16,16,18,20,24,20,18}; return g[row]; }
        case 'l': { const uint8_t g[7] = {12,4,4,4,4,4,14}; return g[row]; }
        case 'm': { const uint8_t g[7] = {0,0,26,21,21,17,17}; return g[row]; }
        case 'n': { const uint8_t g[7] = {0,0,30,17,17,17,17}; return g[row]; }
        case 'o': { const uint8_t g[7] = {0,0,14,17,17,17,14}; return g[row]; }
        case 'p': { const uint8_t g[7] = {0,0,30,17,30,16,16}; return g[row]; }
        case 'q': { const uint8_t g[7] = {0,0,15,17,15,1,1}; return g[row]; }
        case 'r': { const uint8_t g[7] = {0,0,22,24,16,16,16}; return g[row]; }
        case 's': { const uint8_t g[7] = {0,0,15,16,14,1,30}; return g[row]; }
        case 't': { const uint8_t g[7] = {8,8,30,8,8,8,6}; return g[row]; }
        case 'u': { const uint8_t g[7] = {0,0,17,17,17,19,13}; return g[row]; }
        case 'v': { const uint8_t g[7] = {0,0,17,17,17,10,4}; return g[row]; }
        case 'w': { const uint8_t g[7] = {0,0,17,17,21,21,10}; return g[row]; }
        case 'x': { const uint8_t g[7] = {0,0,17,10,4,10,17}; return g[row]; }
        case 'y': { const uint8_t g[7] = {0,0,17,17,15,1,14}; return g[row]; }
        case 'z': { const uint8_t g[7] = {0,0,31,2,4,8,31}; return g[row]; }
        case 'A': { const uint8_t g[7] = {14,17,17,31,17,17,17}; return g[row]; }
        case 'B': { const uint8_t g[7] = {30,17,17,30,17,17,30}; return g[row]; }
        case 'C': { const uint8_t g[7] = {14,17,16,16,16,17,14}; return g[row]; }
        case 'D': { const uint8_t g[7] = {30,17,17,17,17,17,30}; return g[row]; }
        case 'E': { const uint8_t g[7] = {31,16,16,30,16,16,31}; return g[row]; }
        case 'F': { const uint8_t g[7] = {31,16,16,30,16,16,16}; return g[row]; }
        case 'G': { const uint8_t g[7] = {14,17,16,23,17,17,14}; return g[row]; }
        case 'H': { const uint8_t g[7] = {17,17,17,31,17,17,17}; return g[row]; }
        case 'I': { const uint8_t g[7] = {31,4,4,4,4,4,31}; return g[row]; }
        case 'J': { const uint8_t g[7] = {7,2,2,2,18,18,12}; return g[row]; }
        case 'K': { const uint8_t g[7] = {17,18,20,24,20,18,17}; return g[row]; }
        case 'L': { const uint8_t g[7] = {16,16,16,16,16,16,31}; return g[row]; }
        case 'M': { const uint8_t g[7] = {17,27,21,21,17,17,17}; return g[row]; }
        case 'N': { const uint8_t g[7] = {17,25,21,19,17,17,17}; return g[row]; }
        case 'O': { const uint8_t g[7] = {14,17,17,17,17,17,14}; return g[row]; }
        case 'P': { const uint8_t g[7] = {30,17,17,30,16,16,16}; return g[row]; }
        case 'Q': { const uint8_t g[7] = {14,17,17,17,21,18,13}; return g[row]; }
        case 'R': { const uint8_t g[7] = {30,17,17,30,20,18,17}; return g[row]; }
        case 'S': { const uint8_t g[7] = {15,16,16,14,1,1,30}; return g[row]; }
        case 'T': { const uint8_t g[7] = {31,4,4,4,4,4,4}; return g[row]; }
        case 'U': { const uint8_t g[7] = {17,17,17,17,17,17,14}; return g[row]; }
        case 'V': { const uint8_t g[7] = {17,17,17,17,17,10,4}; return g[row]; }
        case 'W': { const uint8_t g[7] = {17,17,17,21,21,27,17}; return g[row]; }
        case 'X': { const uint8_t g[7] = {17,17,10,4,10,17,17}; return g[row]; }
        case 'Y': { const uint8_t g[7] = {17,17,10,4,4,4,4}; return g[row]; }
        case 'Z': { const uint8_t g[7] = {31,1,2,4,8,16,31}; return g[row]; }
        case '0': { const uint8_t g[7] = {14,17,19,21,25,17,14}; return g[row]; }
        case '1': { const uint8_t g[7] = {4,12,4,4,4,4,14}; return g[row]; }
        case '2': { const uint8_t g[7] = {14,17,1,2,4,8,31}; return g[row]; }
        case '3': { const uint8_t g[7] = {30,1,1,14,1,1,30}; return g[row]; }
        case '4': { const uint8_t g[7] = {2,6,10,18,31,2,2}; return g[row]; }
        case '5': { const uint8_t g[7] = {31,16,16,30,1,1,30}; return g[row]; }
        case '6': { const uint8_t g[7] = {14,16,16,30,17,17,14}; return g[row]; }
        case '7': { const uint8_t g[7] = {31,1,2,4,8,8,8}; return g[row]; }
        case '8': { const uint8_t g[7] = {14,17,17,14,17,17,14}; return g[row]; }
        case '9': { const uint8_t g[7] = {14,17,17,15,1,1,14}; return g[row]; }
        case '-': { const uint8_t g[7] = {0,0,0,31,0,0,0}; return g[row]; }
        case '_': { const uint8_t g[7] = {0,0,0,0,0,0,31}; return g[row]; }
        case '=': { const uint8_t g[7] = {0,31,0,31,0,0,0}; return g[row]; }
        case '+': { const uint8_t g[7] = {0,4,4,31,4,4,0}; return g[row]; }
        case '.': { const uint8_t g[7] = {0,0,0,0,0,12,12}; return g[row]; }
        case ',': { const uint8_t g[7] = {0,0,0,0,0,12,8}; return g[row]; }
        case ':': { const uint8_t g[7] = {0,12,12,0,12,12,0}; return g[row]; }
        case ';': { const uint8_t g[7] = {0,12,12,0,12,8,0}; return g[row]; }
        case '\'': { const uint8_t g[7] = {8,8,4,0,0,0,0}; return g[row]; }
        case '"': { const uint8_t g[7] = {10,10,0,0,0,0,0}; return g[row]; }
        case '/': { const uint8_t g[7] = {1,2,2,4,8,8,16}; return g[row]; }
        case '\\': { const uint8_t g[7] = {16,8,8,4,2,2,1}; return g[row]; }
        case '!': { const uint8_t g[7] = {4,4,4,4,4,0,4}; return g[row]; }
        case '?': { const uint8_t g[7] = {14,17,1,2,4,0,4}; return g[row]; }
        case '*': { const uint8_t g[7] = {0,4,21,14,21,4,0}; return g[row]; }
        case '[': { const uint8_t g[7] = {14,8,8,8,8,8,14}; return g[row]; }
        case ']': { const uint8_t g[7] = {14,2,2,2,2,2,14}; return g[row]; }
        case '{': { const uint8_t g[7] = {6,4,4,8,4,4,6}; return g[row]; }
        case '}': { const uint8_t g[7] = {12,4,4,2,4,4,12}; return g[row]; }
        case '<': { const uint8_t g[7] = {2,4,8,16,8,4,2}; return g[row]; }
        case '>': { const uint8_t g[7] = {8,4,2,1,2,4,8}; return g[row]; }
        case '&': { const uint8_t g[7] = {12,18,20,8,21,18,13}; return g[row]; }
        case '@': { const uint8_t g[7] = {14,17,23,21,23,16,14}; return g[row]; }
        case '#': { const uint8_t g[7] = {10,10,31,10,31,10,10}; return g[row]; }
        case '$': { const uint8_t g[7] = {4,15,20,14,5,30,4}; return g[row]; }
        case '%': { const uint8_t g[7] = {24,25,2,4,8,19,3}; return g[row]; }
        case '^': { const uint8_t g[7] = {4,10,17,0,0,0,0}; return g[row]; }
        case '|': { const uint8_t g[7] = {4,4,4,4,4,4,4}; return g[row]; }
        case '~': { const uint8_t g[7] = {0,0,0,13,18,0,0}; return g[row]; }
        case '`': { const uint8_t g[7] = {8,4,0,0,0,0,0}; return g[row]; }
        case '(': { const uint8_t g[7] = {2,4,8,8,8,4,2}; return g[row]; }
        case ')': { const uint8_t g[7] = {8,4,2,2,2,4,8}; return g[row]; }
        case ' ': { return 0; }
        default:  { const uint8_t g[7] = {31,1,2,4,8,0,8}; return g[row]; }
    }
}

static void draw_char_scaled(int x, int y, char c, int s, uint32_t fg) {
    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(c, row);
        for (int col = 0; col < 5; col++) {
            if (bits & (1u << (4 - col))) {
                fill_rect(x + col * s, y + row * s, s, s, fg);
            }
        }
    }
}

static void text_scaled(int x, int y, const char* t, int s, uint32_t fg) {
    if (!t) return;
    while (*t) {
        draw_char_scaled(x, y, *t++, s, fg);
        x += 6 * s;
    }
}

static inline uint8_t fb_bytes_per_pixel(void) {
    return (uint8_t)(g_fb_bpp / 8u);
}

static inline void pset_live(int x, int y, uint32_t c) {
    if ((unsigned)x >= (unsigned)g_draw_w || (unsigned)y >= (unsigned)g_draw_h) return;

    uint8_t r = (uint8_t)((c >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((c >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(c & 0xFFu);
    volatile uint8_t* p = g_fb + (uint32_t)y * g_fb_pitch + (uint32_t)x * ((uint32_t)g_fb_bpp / 8u);

    if (g_fb_bpp == 32) {
        p[0] = b; p[1] = g; p[2] = r; p[3] = 0;
    } else if (g_fb_bpp == 24) {
        p[0] = b; p[1] = g; p[2] = r;
    } else {
        uint16_t c565 =
            (uint16_t)(((r >> 3) & 0x1F) << 11) |
            (uint16_t)(((g >> 2) & 0x3F) << 5) |
            (uint16_t)((b >> 3) & 0x1F);
        p[0] = (uint8_t)(c565 & 0xFF);
        p[1] = (uint8_t)(c565 >> 8);
    }
}

static void fill_rect_live(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            pset_live(xx, yy, c);
        }
    }
}

static void hline_live(int x, int y, int w, uint32_t c) {
    if (w <= 0) return;
    for (int i = 0; i < w; i++) pset_live(x + i, y, c);
}

static void vline_live(int x, int y, int h, uint32_t c) {
    if (h <= 0) return;
    for (int i = 0; i < h; i++) pset_live(x, y + i, c);
}

static void draw_mouse_cursor(void) {
    fill_rect_live(g_mouse_x - 1, g_mouse_y - 1, 3, 3, C_WHITE);
    hline_live(g_mouse_x - 6, g_mouse_y, 13, 0x51b5ff);
    vline_live(g_mouse_x, g_mouse_y - 6, 13, 0x51b5ff);
}

static void save_cursor_under(int cx, int cy) {
    uint8_t bpp = fb_bytes_per_pixel();
    int idx = 0;

    for (int yy = 0; yy < CURSOR_SIZE; yy++) {
        int sy = cy - CURSOR_HALF + yy;
        for (int xx = 0; xx < CURSOR_SIZE; xx++) {
            int sx = cx - CURSOR_HALF + xx;

            for (uint8_t i = 0; i < MAX_BPP_BYTES; i++) g_cursor_under[idx + i] = 0;

            if ((unsigned)sx < (unsigned)g_draw_w && (unsigned)sy < (unsigned)g_draw_h) {
                volatile uint8_t* p = g_fb + (uint32_t)sy * g_fb_pitch + (uint32_t)sx * bpp;
                for (uint8_t i = 0; i < bpp; i++) g_cursor_under[idx + i] = p[i];
            }
            idx += MAX_BPP_BYTES;
        }
    }
}

static void restore_cursor_under(void) {
    uint8_t bpp = fb_bytes_per_pixel();
    int idx = 0;

    for (int yy = 0; yy < CURSOR_SIZE; yy++) {
        int sy = g_cursor_prev_y - CURSOR_HALF + yy;
        for (int xx = 0; xx < CURSOR_SIZE; xx++) {
            int sx = g_cursor_prev_x - CURSOR_HALF + xx;
            if ((unsigned)sx < (unsigned)g_draw_w && (unsigned)sy < (unsigned)g_draw_h) {
                volatile uint8_t* p = g_fb + (uint32_t)sy * g_fb_pitch + (uint32_t)sx * bpp;
                for (uint8_t i = 0; i < bpp; i++) p[i] = g_cursor_under[idx + i];
            }
            idx += MAX_BPP_BYTES;
        }
    }
}

static void cursor_present(void) {
    save_cursor_under(g_mouse_x, g_mouse_y);
    g_cursor_prev_x = g_mouse_x;
    g_cursor_prev_y = g_mouse_y;
    draw_mouse_cursor();
    g_cursor_visible = true;
}

static void cursor_refresh(bool force) {
    if (g_cursor_visible) restore_cursor_under();
    if (force || !g_cursor_visible || g_mouse_x != g_cursor_prev_x || g_mouse_y != g_cursor_prev_y) {
        cursor_present();
    }
}

static void ps2_wait_input_clear(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(KBD_STATUS_PORT) & 0x02) == 0) return;
    }
}

static void ps2_wait_output_full(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(KBD_STATUS_PORT) & 0x01) return;
    }
}

static void mouse_write(uint8_t val) {
    ps2_wait_input_clear();
    outb(KBD_STATUS_PORT, 0xD4);
    ps2_wait_input_clear();
    outb(KBD_DATA_PORT, val);
}

static uint8_t mouse_read(void) {
    ps2_wait_output_full();
    return inb(KBD_DATA_PORT);
}

static void mouse_init(void) {
    uint8_t status;

    ps2_wait_input_clear();
    outb(KBD_STATUS_PORT, 0xA8);

    ps2_wait_input_clear();
    outb(KBD_STATUS_PORT, 0x20);
    status = mouse_read();
    status |= 0x02;
    status |= 0x01;

    ps2_wait_input_clear();
    outb(KBD_STATUS_PORT, 0x60);
    ps2_wait_input_clear();
    outb(KBD_DATA_PORT, status);

    mouse_write(0xF6);
    (void)mouse_read();
    mouse_write(0xF4);
    (void)mouse_read();
}

static void mouse_poll(bool* moved, bool* left_pressed, bool* left_released) {
    static uint8_t pkt[3];
    static int idx = 0;

    *moved = false;
    *left_pressed = false;
    *left_released = false;

    while (inb(KBD_STATUS_PORT) & 0x01) {
        uint8_t status = inb(KBD_STATUS_PORT);
        uint8_t data;

        if ((status & 0x20) == 0) break;

        data = inb(KBD_DATA_PORT);

        pkt[idx++] = data;
        if (idx < 3) continue;
        idx = 0;

        if ((pkt[0] & 0x08) == 0) continue;
        if (pkt[0] & 0xC0) continue;

        int dx = (int)((int8_t)pkt[1]);
        int dy = (int)((int8_t)pkt[2]);

        if (dx != 0 || dy != 0) {
            if (MOUSE_INVERT_X) dx = -dx;
            if (MOUSE_INVERT_Y) dy = -dy;

            g_mouse_x += dx;
            g_mouse_y -= dy;
            g_mouse_x = clamp_int(g_mouse_x, 0, g_draw_w - 1);
            g_mouse_y = clamp_int(g_mouse_y, 0, g_draw_h - 1);
            *moved = true;
        }

        bool left_now = (pkt[0] & 0x01) != 0;
        if (left_now && !g_mouse_left_down) *left_pressed = true;
        if (!left_now && g_mouse_left_down) *left_released = true;
        g_mouse_left_down = left_now;
    }
}

static int window_index(const desk_window_t* win) {
    if (!win) return -1;
    if (win < &g_windows[0] || win >= &g_windows[MAX_WINDOWS]) return -1;
    int idx = (int)(win - &g_windows[0]);
    if (!g_windows[idx].used) return -1;
    return idx;
}

static void zorder_remove(int idx) {
    for (int i = 0; i < g_zcount; i++) {
        if (g_zorder[i] == idx) {
            for (int j = i; j < g_zcount - 1; j++) g_zorder[j] = g_zorder[j + 1];
            g_zcount--;
            return;
        }
    }
}

static void zorder_push_top(int idx) {
    zorder_remove(idx);
    if (g_zcount < MAX_WINDOWS) {
        g_zorder[g_zcount++] = idx;
    }
}

static void set_active_window(int idx) {
    if (idx < 0 || idx >= MAX_WINDOWS || !g_windows[idx].used) {
        g_active_window = -1;
        return;
    }
    zorder_push_top(idx);
    g_active_window = idx;
}

static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static int top_window_at(int x, int y) {
    for (int zi = g_zcount - 1; zi >= 0; zi--) {
        int idx = g_zorder[zi];
        desk_window_t* win = &g_windows[idx];
        if (!win->used || !win->visible) continue;
        if (point_in_rect(x, y, win->x, win->y, win->w, win->h)) return idx;
    }
    return -1;
}

static void window_close_button_rect(const desk_window_t* win, desk_rect_t* r) {
    r->w = WIN_CLOSE_W;
    r->h = WIN_CLOSE_H;
    r->x = win->x + win->w - WIN_BORDER - WIN_CLOSE_W - 6;
    r->y = win->y + WIN_BORDER + ((WIN_TITLE_H - WIN_BORDER - WIN_CLOSE_H) / 2);
}

static enum hit_part_t hit_test_window(int idx, int x, int y) {
    desk_window_t* win = &g_windows[idx];
    desk_rect_t close_r;

    window_close_button_rect(win, &close_r);

    if (!point_in_rect(x, y, win->x, win->y, win->w, win->h)) return HIT_NONE;
    if ((win->flags & DESK_WINDOW_CLOSABLE) && point_in_rect(x, y, close_r.x, close_r.y, close_r.w, close_r.h)) {
        return HIT_CLOSE;
    }
    if (point_in_rect(x, y, win->x + WIN_BORDER, win->y + WIN_BORDER, win->w - WIN_BORDER * 2, WIN_TITLE_H - WIN_BORDER)) {
        return HIT_TITLE;
    }
    return HIT_CLIENT;
}

static void window_client_rect(const desk_window_t* win, desk_rect_t* r) {
    r->x = win->x + WIN_BORDER;
    r->y = win->y + WIN_TITLE_H;
    r->w = win->w - WIN_BORDER * 2;
    r->h = win->h - WIN_TITLE_H - WIN_BORDER;
}

bool desk_register_app(const char* name, desk_app_entry_fn entry) {
    if (!entry || !name) return false;
    if (g_app_count >= MAX_APPS) return false;
    g_apps[g_app_count].used = true;
    g_apps[g_app_count].entry = entry;
    str_copy(g_apps[g_app_count].name, (int)sizeof(g_apps[g_app_count].name), name);
    g_app_count++;
    g_desktop_needs_redraw = true;
    return true;
}

desk_window_t* desk_window_create(const char* title, int x, int y, int w, int h,
                                  uint32_t flags, desk_paint_fn paint, void* user_data) {
    int idx = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_windows[i].used) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return NULL;

    if (w < WIN_MIN_W) w = WIN_MIN_W;
    if (h < WIN_MIN_H) h = WIN_MIN_H;

    int max_x = (int)g_fb_w - w;
    int max_y = (int)g_fb_h - TASKBAR_H - h;
    x = clamp_int(x, 0, max_x < 0 ? 0 : max_x);
    y = clamp_int(y, 0, max_y < 0 ? 0 : max_y);

    g_windows[idx].used = true;
    g_windows[idx].visible = true;
    g_windows[idx].dirty = true;
    g_windows[idx].x = x;
    g_windows[idx].y = y;
    g_windows[idx].w = w;
    g_windows[idx].h = h;
    g_windows[idx].flags = flags;
    g_windows[idx].paint = paint;
    g_windows[idx].user_data = user_data;
    str_copy(g_windows[idx].title, (int)sizeof(g_windows[idx].title), title ? title : "WINDOW");

    set_active_window(idx);
    g_desktop_needs_redraw = true;
    return &g_windows[idx];
}

void desk_window_close(desk_window_t* win) {
    int idx = window_index(win);
    if (idx < 0) return;

    if (&g_windows[idx] == g_terminal.win) {
        g_terminal.win = NULL;
    }
    if (&g_windows[idx] == g_explorer.win) {
        g_explorer.win = NULL;
    }

    g_windows[idx].used = false;
    g_windows[idx].visible = false;
    zorder_remove(idx);

    if (g_active_window == idx) {
        if (g_zcount > 0) g_active_window = g_zorder[g_zcount - 1];
        else g_active_window = -1;
    }

    if (g_drag_window == idx) g_drag_window = -1;
    g_desktop_needs_redraw = true;
}

void desk_window_set_title(desk_window_t* win, const char* title) {
    int idx = window_index(win);
    if (idx < 0) return;
    str_copy(g_windows[idx].title, (int)sizeof(g_windows[idx].title), title ? title : "WINDOW");
    g_windows[idx].dirty = true;
    g_desktop_needs_redraw = true;
}

void desk_window_set_position(desk_window_t* win, int x, int y) {
    int idx = window_index(win);
    if (idx < 0) return;

    int max_x = (int)g_fb_w - g_windows[idx].w;
    int max_y = (int)g_fb_h - TASKBAR_H - g_windows[idx].h;

    g_windows[idx].x = clamp_int(x, 0, max_x < 0 ? 0 : max_x);
    g_windows[idx].y = clamp_int(y, 0, max_y < 0 ? 0 : max_y);
    g_windows[idx].dirty = true;
    g_desktop_needs_redraw = true;
}

void desk_window_request_repaint(desk_window_t* win) {
    int idx = window_index(win);
    if (idx < 0) return;
    g_windows[idx].dirty = true;
    g_desktop_needs_redraw = true;
}

int desk_window_client_width(const desk_window_t* win) {
    int idx = window_index(win);
    if (idx < 0) return 0;
    return g_windows[idx].w - WIN_BORDER * 2;
}

int desk_window_client_height(const desk_window_t* win) {
    int idx = window_index(win);
    if (idx < 0) return 0;
    return g_windows[idx].h - WIN_TITLE_H - WIN_BORDER;
}

static bool clip_client_rect(desk_window_t* win, int* x, int* y, int* w, int* h) {
    int cw = desk_window_client_width(win);
    int ch = desk_window_client_height(win);

    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    if (*x + *w > cw) *w = cw - *x;
    if (*y + *h > ch) *h = ch - *y;

    return *w > 0 && *h > 0;
}

void desk_gfx_fill_rect(desk_window_t* win, int x, int y, int w, int h, uint32_t color) {
    int idx = window_index(win);
    if (idx < 0 || w <= 0 || h <= 0) return;
    if (!clip_client_rect(win, &x, &y, &w, &h)) return;

    desk_rect_t r;
    window_client_rect(win, &r);
    fill_rect(r.x + x, r.y + y, w, h, color);
}

void desk_gfx_frame_rect(desk_window_t* win, int x, int y, int w, int h, uint32_t color) {
    int idx = window_index(win);
    if (idx < 0 || w <= 1 || h <= 1) return;

    desk_rect_t r;
    window_client_rect(win, &r);
    frame(r.x + x, r.y + y, w, h, color);
}

void desk_gfx_text(desk_window_t* win, int x, int y, const char* text, int scale, uint32_t color) {
    int idx = window_index(win);
    if (idx < 0 || !text || scale <= 0) return;

    desk_rect_t r;
    window_client_rect(win, &r);
    text_scaled(r.x + x, r.y + y, text, scale, color);
}

static void draw_window_frame(const desk_window_t* win, bool active) {
    uint32_t title_color = active ? C_TITLE_ACTIVE : C_TITLE_INACT;
    desk_rect_t close_r;

    fill_rect(win->x, win->y, win->w, win->h, C_WINDOW);
    frame(win->x, win->y, win->w, win->h, C_WINDOW_BORDER);

    fill_rect(win->x + WIN_BORDER, win->y + WIN_BORDER, win->w - WIN_BORDER * 2, WIN_TITLE_H - WIN_BORDER, title_color);
    text_scaled(win->x + 10, win->y + 7, win->title, 2, C_WHITE);

    if (win->flags & DESK_WINDOW_CLOSABLE) {
        int glyph_w = 6;
        int glyph_h = 7;
        window_close_button_rect(win, &close_r);
        fill_rect(close_r.x, close_r.y, close_r.w, close_r.h, C_CLOSE_BG);
        frame(close_r.x, close_r.y, close_r.w, close_r.h, 0x812828);
        text_scaled(close_r.x + (close_r.w - glyph_w) / 2,
                    close_r.y + (close_r.h - glyph_h) / 2,
                    "X", 1, C_WHITE);
    }

    fill_rect(win->x + WIN_BORDER, win->y + WIN_TITLE_H, win->w - WIN_BORDER * 2, win->h - WIN_TITLE_H - WIN_BORDER, C_CLIENT_BG);
}

static void draw_desktop_wallpaper(void) {
    int sw = (int)g_fb_w;
    int sh = (int)g_fb_h;
    int work_h = sh - TASKBAR_H;

    fill_rect(0, 0, sw, work_h / 2, C_DESK_TOP);
    fill_rect(0, work_h / 2, sw, work_h - work_h / 2, C_DESK_BOTTOM);

    fill_rect(sw - 330, 90, 320, 260, C_DESK_SHAPE);
    dot(sw - 90, 170, 48, 0x5aa9ff);
    dot(sw - 130, 245, 62, 0x4a95ed);

    text_scaled(32, 26, "RINGOS", 3, C_WHITE);
    text_scaled(32, 52, "DESKTOP", 3, C_WHITE);
}

static int task_button_rect_for_order(int order, desk_rect_t* r) {
    int x = START_W + 10 + order * 126;
    int y = (int)g_fb_h - TASKBAR_H + 6;
    int w = 120;
    int h = TASKBAR_H - 12;

    if (x + w > (int)g_fb_w - 8) return 0;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    return 1;
}

static int task_window_by_point(int x, int y) {
    int shown = 0;
    for (int i = 0; i < g_zcount && shown < TASK_BUTTON_MAX; i++) {
        int idx = g_zorder[i];
        desk_rect_t r;
        if (!task_button_rect_for_order(shown, &r)) break;
        if (point_in_rect(x, y, r.x, r.y, r.w, r.h)) return idx;
        shown++;
    }
    return -1;
}

static void draw_taskbar(void) {
    int sh = (int)g_fb_h;
    int y = sh - TASKBAR_H;
    int start_x = 8;
    int start_y = y + 6;
    int start_w = START_W - 12;
    int start_h = TASKBAR_H - 12;
    int start_text_w = 5 * 6 * 2;
    int start_text_h = 7 * 2;

    fill_rect(0, y, (int)g_fb_w, TASKBAR_H, C_TASKBAR);
    hline(0, y, (int)g_fb_w, C_TASKBAR_TOP);

    fill_rect(start_x, start_y, start_w, start_h, g_start_menu_open ? C_START_BTN_OPEN : C_START_BTN);
    frame(start_x, start_y, start_w, start_h, 0x2a3d56);
    text_scaled(start_x + (start_w - start_text_w) / 2,
                start_y + (start_h - start_text_h) / 2,
                "START", 2, C_WHITE);

    int shown = 0;
    for (int i = 0; i < g_zcount && shown < TASK_BUTTON_MAX; i++) {
        int idx = g_zorder[i];
        desk_rect_t r;
        if (!task_button_rect_for_order(shown, &r)) break;

        uint32_t bc = (idx == g_active_window) ? C_TASK_BTN_ACT : C_TASK_BTN;
        fill_rect(r.x, r.y, r.w, r.h, bc);
        frame(r.x, r.y, r.w, r.h, 0x314156);
        text_scaled(r.x + 8, r.y + 8, g_windows[idx].title, 1, C_TEXT);
        shown++;
    }

    text_scaled((int)g_fb_w - 155, y + 14, "ESC SHUTDOWN", 2, C_TEXT);
}

static void start_menu_geometry(desk_rect_t* menu_rect, int* item_h) {
    int items = g_app_count + 1;
    int ih = 28;
    int header_h = 22;
    int menu_h = header_h + items * ih + 10;

    menu_rect->x = 8;
    menu_rect->w = 230;
    menu_rect->h = menu_h;
    menu_rect->y = (int)g_fb_h - TASKBAR_H - menu_h - 6;
    *item_h = ih;
}

static int start_menu_item_at(int x, int y) {
    desk_rect_t mr;
    int ih;
    start_menu_geometry(&mr, &ih);

    if (!point_in_rect(x, y, mr.x, mr.y, mr.w, mr.h)) return -3;

    int top = mr.y + 24;
    for (int i = 0; i < g_app_count; i++) {
        if (point_in_rect(x, y, mr.x + 6, top + i * ih, mr.w - 12, ih - 2)) return i;
    }

    if (point_in_rect(x, y, mr.x + 6, top + g_app_count * ih, mr.w - 12, ih - 2)) return -2;
    return -1;
}

static void draw_start_menu(void) {
    if (!g_start_menu_open) return;

    desk_rect_t mr;
    int ih;
    start_menu_geometry(&mr, &ih);

    fill_rect(mr.x, mr.y, mr.w, mr.h, C_MENU_BG);
    frame(mr.x, mr.y, mr.w, mr.h, 0x344a63);

    text_scaled(mr.x + 10, mr.y + 6, "APPS", 1, C_TEXT_MUTED);

    int top = mr.y + 24;
    for (int i = 0; i < g_app_count; i++) {
        fill_rect(mr.x + 6, top + i * ih, mr.w - 12, ih - 2, C_MENU_HOVER);
        text_scaled(mr.x + 16, top + i * ih + 7, g_apps[i].name, 2, C_TEXT);
    }

    fill_rect(mr.x + 6, top + g_app_count * ih, mr.w - 12, ih - 2, 0x6d2731);
    text_scaled(mr.x + 16, top + g_app_count * ih + 7, "EXIT", 2, C_TEXT);
}

static void draw_windows(void) {
    for (int zi = 0; zi < g_zcount; zi++) {
        int idx = g_zorder[zi];
        desk_window_t* win = &g_windows[idx];
        if (!win->used || !win->visible) continue;

        draw_window_frame(win, idx == g_active_window);

        if (win->paint) {
            win->paint(win, win->user_data);
        }

        win->dirty = false;
    }
}

static void draw_desktop(void) {
    draw_desktop_wallpaper();
    draw_windows();
    draw_start_menu();
    draw_taskbar();
}

static bool any_window_dirty(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].dirty) return true;
    }
    return false;
}

static void shutdown_system(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    /* QEMU debug-exit (enabled by Makefile run target). */
    outb(0x501, 0x00);

    for (;;) {
        asm volatile ("hlt");
    }
}

/* Optional user hook: define this in your own source file to add apps. */
void desk_register_user_apps(void) __attribute__((weak));
void desk_register_user_apps(void) {}

static void terminal_mark_dirty(void) {
    if (g_terminal.win) {
        desk_window_request_repaint(g_terminal.win);
    }
}

static void terminal_vga_putchar(char c, void* user_data) {
    terminal_state_t* t = (terminal_state_t*)user_data;
    term_putc(t, c);
    terminal_mark_dirty();
}

static void terminal_vga_clear(void* user_data) {
    terminal_state_t* t = (terminal_state_t*)user_data;
    term_reset(t);
    terminal_mark_dirty();
}

static void terminal_vga_color(uint8_t fg, uint8_t bg, void* user_data) {
    terminal_state_t* t = (terminal_state_t*)user_data;
    t->current_fg = vga_color_to_rgb(fg);
    t->current_bg = vga_color_to_rgb(bg);
}

static void terminal_begin_capture(void) {
    vga_set_redirect(terminal_vga_putchar, terminal_vga_clear, terminal_vga_color, &g_terminal);
}

static void terminal_end_capture(void) {
    vga_clear_redirect();
}

static void terminal_bootstrap(void) {
    if (g_terminal.initialized) return;

    term_reset(&g_terminal);
    terminal_begin_capture();
    shell_init();
    terminal_end_capture();
    g_terminal.initialized = true;
}

static void terminal_submit_input(void) {
    int i;
    bool is_editor_cmd;

    if (!g_terminal.win) return;

    for (int i = 0; i < g_terminal.input_len; i++) {
        term_putc(&g_terminal, g_terminal.input[i]);
    }
    term_putc(&g_terminal, '\n');

    g_terminal.input[g_terminal.input_len] = '\0';

    i = 0;
    while (g_terminal.input[i] == ' ') i++;
    is_editor_cmd = false;
    if ((g_terminal.input[i] == 'e' && g_terminal.input[i + 1] == 'd' && (g_terminal.input[i + 2] == '\0' || g_terminal.input[i + 2] == ' ')) ||
        (g_terminal.input[i] == 'i' && g_terminal.input[i + 1] == 'd' && g_terminal.input[i + 2] == 'e' &&
         (g_terminal.input[i + 3] == '\0' || g_terminal.input[i + 3] == ' ')) ||
        (g_terminal.input[i] == 'n' && g_terminal.input[i + 1] == 'e' && g_terminal.input[i + 2] == 'w' &&
         g_terminal.input[i + 3] == 'a' && g_terminal.input[i + 4] == 'p' && g_terminal.input[i + 5] == 'p' &&
         (g_terminal.input[i + 6] == '\0' || g_terminal.input[i + 6] == ' '))) {
        is_editor_cmd = true;
    }

    if (str_equal(g_terminal.input, "gui")) {
        term_puts(&g_terminal, "Already in GUI desktop.\n");
        term_write_prompt(&g_terminal);
    } else if (is_editor_cmd) {
        term_puts(&g_terminal, "Editor commands are text-mode only for now.\n");
        term_puts(&g_terminal, "Exit GUI and run: newapp APP.C or ed APP.C\n");
        term_write_prompt(&g_terminal);
    } else {
        terminal_begin_capture();
        shell_execute_line(g_terminal.input);
        terminal_end_capture();
    }

    g_terminal.input_len = 0;
    g_terminal.input[0] = '\0';
    terminal_mark_dirty();
}

static void terminal_draw_char(desk_window_t* win, int x, int y, char ch, uint32_t fg, uint32_t bg) {
    uint8_t bits;

    desk_gfx_fill_rect(win, x, y, 6, 7, bg);
    for (int row = 0; row < 7; row++) {
        bits = glyph_row(ch, row);
        for (int col = 0; col < 5; col++) {
            if (bits & (1u << (4 - col))) {
                desk_gfx_fill_rect(win, x + col, y + row, 1, 1, fg);
            }
        }
    }
}

static void app_shell_paint(desk_window_t* win, void* user_data) {
    (void)user_data;
    int cw = desk_window_client_width(win);
    int ch = desk_window_client_height(win);
    int char_w = 6;
    int char_h = 7;
    int line_step = 9;
    int max_rows;
    int start_line;

    g_terminal.win = win;

    fill_rect(win->x + WIN_BORDER, win->y + WIN_TITLE_H, cw, ch, 0x0c1118);
    desk_gfx_frame_rect(win, 0, 0, cw, ch, 0x1f2a39);
    desk_gfx_text(win, 8, 8, "RING SHELL", 1, 0x7bc6ff);

    max_rows = (ch - 24) / line_step;
    if (max_rows < 1) max_rows = 1;

    start_line = g_terminal.line_count - max_rows;
    if (start_line < 0) start_line = 0;

    for (int row = 0; row < max_rows; row++) {
        int li = start_line + row;
        int y = 22 + row * line_step;
        if (li >= g_terminal.line_count) break;
        for (int col = 0; col < TERM_MAX_COLS; col++) {
            char ch = g_terminal.lines[li][col];
            if (ch == '\0') break;
            terminal_draw_char(win, 8 + col * char_w, y, ch, g_terminal.fg[li][col], g_terminal.bg[li][col]);
        }
    }

    {
        int input_row = g_terminal.line_count - start_line - 1;
        if (input_row >= 0 && input_row < max_rows) {
            int base_x = 8 + g_terminal.cursor_col * char_w;
            int base_y = 22 + input_row * line_step;
            if (g_terminal.input_len > 0) {
                desk_gfx_text(win, base_x, base_y, g_terminal.input, 1, 0xe7eef9);
            }
            if (win == &g_windows[g_active_window]) {
                int cx = base_x + g_terminal.input_len * char_w;
                int cy = base_y + char_h + 1;
                desk_gfx_fill_rect(win, cx, cy, char_w - 1, 1, 0x7bc6ff);
            }
        }
    }
}

static void app_open_shell(void) {
    if (!g_terminal.initialized) {
        terminal_bootstrap();
    }

    if (g_terminal.win && window_index(g_terminal.win) >= 0) {
        set_active_window(window_index(g_terminal.win));
        return;
    }

    g_terminal.win = desk_window_create("RING SHELL", 170, 130, 520, 320,
                                        DESK_WINDOW_CLOSABLE, app_shell_paint, NULL);
}

static void app_explorer_paint(desk_window_t* win, void* user_data) {
    (void)user_data;
    int cw = desk_window_client_width(win);
    int ch = desk_window_client_height(win);
    const char* path = fat32_get_current_path();

    g_explorer.win = win;

    desk_gfx_fill_rect(win, 0, 0, cw, ch, 0xf7f9fc);
    desk_gfx_fill_rect(win, 0, 0, cw, 24, 0xe9eef5);
    desk_gfx_frame_rect(win, 0, 0, cw, ch, 0xc8d3e0);
    desk_gfx_text(win, 8, 7, "PATH:", 1, 0x4a5a70);
    desk_gfx_text(win, 42, 7, path, 1, 0x1f2a38);

    desk_gfx_fill_rect(win, EXPL_BTN_UP_X, EXPL_BTN_Y, EXPL_BTN_W, EXPL_BTN_H, 0xd9e8ff);
    desk_gfx_fill_rect(win, EXPL_BTN_REF_X, EXPL_BTN_Y, EXPL_BTN_W, EXPL_BTN_H, 0xd9e8ff);
    desk_gfx_fill_rect(win, EXPL_BTN_OPEN_X, EXPL_BTN_Y, EXPL_BTN_W + 12, EXPL_BTN_H, 0xd9e8ff);
    desk_gfx_fill_rect(win, EXPL_BTN_REN_X, EXPL_BTN_Y, EXPL_BTN_W + 12, EXPL_BTN_H, 0xffefbf);
    desk_gfx_fill_rect(win, EXPL_BTN_NEWF_X, EXPL_BTN_Y, EXPL_BTN_W + 10, EXPL_BTN_H, 0xe6f7db);
    desk_gfx_fill_rect(win, EXPL_BTN_NEWD_X, EXPL_BTN_Y, EXPL_BTN_W + 10, EXPL_BTN_H, 0xe6f7db);
    desk_gfx_fill_rect(win, EXPL_BTN_DEL_X, EXPL_BTN_Y, EXPL_BTN_W + 8, EXPL_BTN_H, 0xffe0e0);

    desk_gfx_frame_rect(win, EXPL_BTN_UP_X, EXPL_BTN_Y, EXPL_BTN_W, EXPL_BTN_H, 0x9ab3d6);
    desk_gfx_frame_rect(win, EXPL_BTN_REF_X, EXPL_BTN_Y, EXPL_BTN_W, EXPL_BTN_H, 0x9ab3d6);
    desk_gfx_frame_rect(win, EXPL_BTN_OPEN_X, EXPL_BTN_Y, EXPL_BTN_W + 12, EXPL_BTN_H, 0x9ab3d6);
    desk_gfx_frame_rect(win, EXPL_BTN_REN_X, EXPL_BTN_Y, EXPL_BTN_W + 12, EXPL_BTN_H, 0xd1b678);
    desk_gfx_frame_rect(win, EXPL_BTN_NEWF_X, EXPL_BTN_Y, EXPL_BTN_W + 10, EXPL_BTN_H, 0x97bf8b);
    desk_gfx_frame_rect(win, EXPL_BTN_NEWD_X, EXPL_BTN_Y, EXPL_BTN_W + 10, EXPL_BTN_H, 0x97bf8b);
    desk_gfx_frame_rect(win, EXPL_BTN_DEL_X, EXPL_BTN_Y, EXPL_BTN_W + 8, EXPL_BTN_H, 0xc88f8f);

    desk_gfx_text(win, EXPL_BTN_UP_X + 7, EXPL_BTN_Y + 5, "UP", 1, 0x23354d);
    desk_gfx_text(win, EXPL_BTN_REF_X + 4, EXPL_BTN_Y + 5, "REF", 1, 0x23354d);
    desk_gfx_text(win, EXPL_BTN_OPEN_X + 4, EXPL_BTN_Y + 5, "OPEN", 1, 0x23354d);
    desk_gfx_text(win, EXPL_BTN_REN_X + 4, EXPL_BTN_Y + 5, "RENAME", 1, 0x5d4b1f);
    desk_gfx_text(win, EXPL_BTN_NEWF_X + 4, EXPL_BTN_Y + 5, "NEWF", 1, 0x25431c);
    desk_gfx_text(win, EXPL_BTN_NEWD_X + 4, EXPL_BTN_Y + 5, "NEWD", 1, 0x25431c);
    desk_gfx_text(win, EXPL_BTN_DEL_X + 7, EXPL_BTN_Y + 5, "DEL", 1, 0x5a1f1f);

    desk_gfx_fill_rect(win, EXPL_LIST_X, EXPL_LIST_Y, EXPL_LIST_W, ch - EXPL_LIST_Y - 20, 0xffffff);
    desk_gfx_frame_rect(win, EXPL_LIST_X, EXPL_LIST_Y, EXPL_LIST_W, ch - EXPL_LIST_Y - 20, 0xc8d3e0);

    for (int r = 0; r < EXPL_MAX_ROWS; r++) {
        int idx = g_explorer.scroll + r;
        int y = EXPL_LIST_Y + 2 + r * EXPL_ROW_H;
        if (idx >= g_explorer.item_count) break;

        if (idx == g_explorer.selected) {
            desk_gfx_fill_rect(win, EXPL_LIST_X + 2, y - 1, EXPL_LIST_W - 4, EXPL_ROW_H, 0xd8e9ff);
        }

        if (g_explorer.items[idx].attr & ATTR_DIRECTORY) {
            desk_gfx_text(win, EXPL_LIST_X + 6, y + 2, "[DIR]", 1, 0x2b5ea9);
            if (g_explorer.rename_mode && idx == g_explorer.rename_idx) {
                desk_gfx_fill_rect(win, EXPL_LIST_X + 44, y, 150, EXPL_ROW_H - 1, 0xfff4c7);
                desk_gfx_text(win, EXPL_LIST_X + 46, y + 2, g_explorer.rename_buf, 1, 0x213349);
            } else {
                desk_gfx_text(win, EXPL_LIST_X + 46, y + 2, g_explorer.items[idx].display, 1, 0x213349);
            }
        } else {
            char sz[16];
            u32_to_dec(g_explorer.items[idx].size, sz, (int)sizeof(sz));
            if (g_explorer.rename_mode && idx == g_explorer.rename_idx) {
                desk_gfx_fill_rect(win, EXPL_LIST_X + 4, y, 190, EXPL_ROW_H - 1, 0xfff4c7);
                desk_gfx_text(win, EXPL_LIST_X + 6, y + 2, g_explorer.rename_buf, 1, 0x213349);
            } else {
                desk_gfx_text(win, EXPL_LIST_X + 6, y + 2, g_explorer.items[idx].display, 1, 0x213349);
            }
            desk_gfx_text(win, EXPL_LIST_X + 190, y + 2, sz, 1, 0x5d6f83);
        }
    }

    desk_gfx_fill_rect(win, EXPL_PREVIEW_X, EXPL_PREVIEW_Y, cw - EXPL_PREVIEW_X - 8, ch - EXPL_PREVIEW_Y - 20, 0x0f1720);
    desk_gfx_frame_rect(win, EXPL_PREVIEW_X, EXPL_PREVIEW_Y, cw - EXPL_PREVIEW_X - 8, ch - EXPL_PREVIEW_Y - 20, 0x32465d);
    desk_gfx_text(win, EXPL_PREVIEW_X + 8, EXPL_PREVIEW_Y + 8, "PREVIEW", 1, 0x7cc3ff);

    {
        int x = EXPL_PREVIEW_X + 8;
        int y = EXPL_PREVIEW_Y + 22;
        int col = 0;
        int row = 0;
        for (int i = 0; g_explorer.preview[i]; i++) {
            char c = g_explorer.preview[i];
            if (c == '\n') {
                row++;
                col = 0;
                if (row > 18) break;
                continue;
            }
            if (col < 40) {
                char s[2] = {c, '\0'};
                desk_gfx_text(win, x + col * 6, y + row * 9, s, 1, 0xdbe8f7);
            }
            col++;
            if (col >= 42) {
                row++;
                col = 0;
                if (row > 18) break;
            }
        }
    }

    desk_gfx_text(win, 8, ch - 14, g_explorer.status, 1, 0x607286);
}

static void app_open_explorer(void) {
    if (g_explorer.win && window_index(g_explorer.win) >= 0) {
        set_active_window(window_index(g_explorer.win));
        return;
    }

    g_explorer.win = desk_window_create("FILE EXPLORER", 90, 80, 620, 360,
                                        DESK_WINDOW_CLOSABLE, app_explorer_paint, NULL);
    g_explorer.selected = 0;
    g_explorer.scroll = 0;
    g_explorer.rename_mode = false;
    g_explorer.rename_idx = -1;
    g_explorer.rename_len = 0;
    g_explorer.rename_buf[0] = '\0';
    g_explorer.last_click_idx = -1;
    g_explorer.last_click_tick = 0;
    explorer_refresh();
}

static void app_notepad_paint(desk_window_t* win, void* user_data) {
    (void)user_data;
    int cw = desk_window_client_width(win);

    desk_gfx_fill_rect(win, 0, 0, cw, 26, 0xf5f7fb);
    desk_gfx_frame_rect(win, 0, 0, cw, 26, 0xd1d9e6);
    desk_gfx_text(win, 10, 8, "FILE   EDIT   VIEW", 1, C_TEXT_MUTED);

    desk_gfx_text(win, 12, 42, "TODO.TXT", 2, C_TEXT_DARK);
    desk_gfx_text(win, 12, 72, "- BUILD FILE BROWSER", 1, C_TEXT_DARK);
    desk_gfx_text(win, 12, 88, "- IMPROVE APP SDK", 1, C_TEXT_DARK);
    desk_gfx_text(win, 12, 104, "- SHIP RINGOS 0.2", 1, C_TEXT_DARK);
}

static void app_system_paint(desk_window_t* win, void* user_data) {
    (void)user_data;
    char mode_line[40];

    mode_line[0] = 'M'; mode_line[1] = 'O'; mode_line[2] = 'D'; mode_line[3] = 'E';
    mode_line[4] = ':'; mode_line[5] = ' '; mode_line[6] = '\0';

    desk_gfx_text(win, 12, 16, "SYSTEM INFORMATION", 2, C_TEXT_DARK);
    desk_gfx_text(win, 12, 50, mode_line, 2, C_TEXT_MUTED);
    desk_gfx_text(win, 90, 50, "VBE", 2, C_TEXT_DARK);
    desk_gfx_text(win, 12, 76, "RESOLUTION:", 2, C_TEXT_MUTED);
    desk_gfx_text(win, 140, 76, "800X600", 2, C_TEXT_DARK);
    desk_gfx_text(win, 12, 102, "BPP:", 2, C_TEXT_MUTED);
    desk_gfx_text(win, 78, 102, g_fb_bpp == 32 ? "32" : (g_fb_bpp == 24 ? "24" : "16"), 2, C_TEXT_DARK);
    desk_gfx_text(win, 12, 136, "MOUSE + WINDOWS API READY", 1, C_TEXT_MUTED);
}

static void app_about_paint(desk_window_t* win, void* user_data) {
    (void)user_data;

    desk_gfx_fill_rect(win, 0, 0, desk_window_client_width(win), 60, 0xe8f2ff);
    desk_gfx_text(win, 16, 18, "RINGOS DESKTOP", 2, 0x1b4f95);
    desk_gfx_text(win, 16, 74, "WINDOWS-STYLE UI WITH MULTI WINDOW SUPPORT", 1, C_TEXT_DARK);
    desk_gfx_text(win, 16, 96, "USE START MENU TO LAUNCH APPS", 1, C_TEXT_DARK);
    desk_gfx_text(win, 16, 118, "DRAG TITLE BARS TO MOVE WINDOWS", 1, C_TEXT_DARK);
    desk_gfx_text(win, 16, 140, "CLICK X TO CLOSE", 1, C_TEXT_DARK);
}

static const char* g_ide_keywords[] = {
    "int", "char", "void", "return", "if", "else", "for", "while",
    "break", "continue", "static", "const", "struct", "typedef", "enum",
    "unsigned", "signed", "sizeof", "switch", "case", "default", "include",
    "clib_puts", "clib_putc", "clib_open", "clib_read", "clib_write", "clib_close",
    "clib_exit", "main", "true", "false", "NULL"
};

static void ide_mark_dirty(void) {
    if (g_ide.win) desk_window_request_repaint(g_ide.win);
}

static void ide_set_status(const char* s) {
    str_copy(g_ide.status, (int)sizeof(g_ide.status), s ? s : "");
    ide_mark_dirty();
}

static void ide_load_file(const char* filename);

static void ide_clear(void) {
    for (int i = 0; i < IDE_MAX_LINES; i++) {
        g_ide.lines[i][0] = '\0';
    }
    g_ide.line_count = 1;
    g_ide.cursor_x = 0;
    g_ide.cursor_y = 0;
    g_ide.scroll_y = 0;
    g_ide.insert_mode = false;
    g_ide.cmd_mode = false;
    g_ide.cmd_len = 0;
    g_ide.cmd[0] = '\0';
    g_ide.open_dialog = false;
    g_ide.open_len = 0;
    g_ide.open_name[0] = '\0';
}

static void ide_new_file(void) {
    ide_clear();
    str_copy(g_ide.filename, (int)sizeof(g_ide.filename), "APP.C");
    ide_set_status("new file");
}

static void ide_begin_open_dialog(void) {
    g_ide.open_dialog = true;
    str_copy(g_ide.open_name, (int)sizeof(g_ide.open_name), g_ide.filename[0] ? g_ide.filename : "APP.C");
    g_ide.open_len = (int)strlen(g_ide.open_name);
    ide_set_status("open: type filename + ENTER");
}

static void ide_confirm_open_dialog(void) {
    if (g_ide.open_len <= 0) {
        g_ide.open_dialog = false;
        ide_set_status("open canceled");
        return;
    }
    g_ide.open_name[g_ide.open_len] = '\0';
    g_ide.open_dialog = false;
    ide_load_file(g_ide.open_name);
}

static bool ide_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

static bool ide_word_equals(const char* s, int len, const char* w) {
    int i = 0;
    while (i < len && w[i]) {
        if (s[i] != w[i]) return false;
        i++;
    }
    return i == len && w[i] == '\0';
}

static bool ide_is_keyword(const char* s, int len) {
    for (size_t i = 0; i < sizeof(g_ide_keywords) / sizeof(g_ide_keywords[0]); i++) {
        if (ide_word_equals(s, len, g_ide_keywords[i])) return true;
    }
    return false;
}

static void ide_cursor_clamp(void) {
    int len;
    if (g_ide.cursor_y < 0) g_ide.cursor_y = 0;
    if (g_ide.cursor_y >= g_ide.line_count) g_ide.cursor_y = g_ide.line_count - 1;
    if (g_ide.cursor_y < 0) g_ide.cursor_y = 0;
    len = (int)strlen(g_ide.lines[g_ide.cursor_y]);
    if (g_ide.cursor_x < 0) g_ide.cursor_x = 0;
    if (g_ide.cursor_x > len) g_ide.cursor_x = len;
}

static void ide_ensure_visible(int rows) {
    if (g_ide.cursor_y < g_ide.scroll_y) g_ide.scroll_y = g_ide.cursor_y;
    if (g_ide.cursor_y >= g_ide.scroll_y + rows) g_ide.scroll_y = g_ide.cursor_y - rows + 1;
    if (g_ide.scroll_y < 0) g_ide.scroll_y = 0;
}

static void ide_load_file(const char* filename) {
    static char raw[IDE_MAX_LINES * IDE_MAX_COLS];
    uint32_t sz = (uint32_t)sizeof(raw);
    int row = 0;
    int col = 0;

    ide_clear();
    str_copy(g_ide.filename, (int)sizeof(g_ide.filename), filename ? filename : "APP.C");

    if (!fat32_read_file(g_ide.filename, raw, &sz)) {
        const char* tpl =
            "#include \"clib.h\"\n"
            "\n"
            "int main(void) {\n"
            "    clib_puts(\"Hello from GUI IDE!\\n\");\n"
            "    return 0;\n"
            "}\n";
        (void)fat32_write_file(g_ide.filename, tpl, (uint32_t)strlen(tpl));
        if (!fat32_read_file(g_ide.filename, raw, &sz)) {
            ide_set_status("new file");
            return;
        }
    }

    for (uint32_t i = 0; i < sz && row < IDE_MAX_LINES; i++) {
        char c = raw[i];
        if (c == '\r') continue;
        if (c == '\n') {
            row++;
            col = 0;
            continue;
        }
        if (c >= 32 && c <= 126) {
            if (col < IDE_MAX_COLS) {
                g_ide.lines[row][col++] = c;
                g_ide.lines[row][col] = '\0';
            }
        }
    }

    g_ide.line_count = row + 1;
    if (g_ide.line_count < 1) g_ide.line_count = 1;
    ide_set_status("loaded");
}

static void ide_save_file(void) {
    static char out[IDE_MAX_LINES * (IDE_MAX_COLS + 1)];
    size_t out_len = 0;
    int last = -1;

    for (int r = 0; r < g_ide.line_count; r++) {
        if (g_ide.lines[r][0] != '\0') last = r;
    }
    if (last < 0) last = 0;

    for (int r = 0; r <= last; r++) {
        int len = (int)strlen(g_ide.lines[r]);
        for (int c = 0; c < len; c++) {
            if (out_len + 1 >= sizeof(out)) break;
            out[out_len++] = g_ide.lines[r][c];
        }
        if (r < last && out_len + 1 < sizeof(out)) out[out_len++] = '\n';
    }

    if (fat32_write_file(g_ide.filename, out, (uint32_t)out_len)) ide_set_status("saved");
    else ide_set_status("save failed");
}

static void ide_insert_char(char ch) {
    char* line = g_ide.lines[g_ide.cursor_y];
    int len = (int)strlen(line);
    if (len >= IDE_MAX_COLS) return;
    if (g_ide.cursor_x > len) g_ide.cursor_x = len;

    for (int i = len; i >= g_ide.cursor_x; i--) {
        line[i + 1] = line[i];
    }
    line[g_ide.cursor_x] = ch;
    g_ide.cursor_x++;
}

static void ide_backspace(void) {
    char* line = g_ide.lines[g_ide.cursor_y];
    int len = (int)strlen(line);

    if (g_ide.cursor_x > 0) {
        for (int i = g_ide.cursor_x - 1; i < len; i++) {
            line[i] = line[i + 1];
        }
        g_ide.cursor_x--;
        return;
    }

    if (g_ide.cursor_y > 0) {
        int prev = g_ide.cursor_y - 1;
        int prev_len = (int)strlen(g_ide.lines[prev]);
        if (prev_len + len <= IDE_MAX_COLS) {
            strcat(g_ide.lines[prev], line);
            for (int r = g_ide.cursor_y; r < g_ide.line_count - 1; r++) {
                str_copy(g_ide.lines[r], IDE_MAX_COLS + 1, g_ide.lines[r + 1]);
            }
            g_ide.lines[g_ide.line_count - 1][0] = '\0';
            g_ide.line_count--;
            g_ide.cursor_y = prev;
            g_ide.cursor_x = prev_len;
        }
    }
}

static void ide_newline(void) {
    char* line = g_ide.lines[g_ide.cursor_y];
    int len = (int)strlen(line);

    if (g_ide.line_count >= IDE_MAX_LINES) return;
    if (g_ide.cursor_x > len) g_ide.cursor_x = len;

    for (int r = g_ide.line_count; r > g_ide.cursor_y + 1; r--) {
        str_copy(g_ide.lines[r], IDE_MAX_COLS + 1, g_ide.lines[r - 1]);
    }

    str_copy(g_ide.lines[g_ide.cursor_y + 1], IDE_MAX_COLS + 1, &line[g_ide.cursor_x]);
    line[g_ide.cursor_x] = '\0';
    g_ide.line_count++;
    g_ide.cursor_y++;
    g_ide.cursor_x = 0;
}

static void ide_try_completion(void) {
    char* line = g_ide.lines[g_ide.cursor_y];
    int start = g_ide.cursor_x;
    int prefix_len;
    const char* best = NULL;

    while (start > 0 && ide_is_ident_char(line[start - 1])) start--;
    prefix_len = g_ide.cursor_x - start;
    if (prefix_len <= 0) return;

    for (size_t i = 0; i < sizeof(g_ide_keywords) / sizeof(g_ide_keywords[0]); i++) {
        const char* kw = g_ide_keywords[i];
        bool ok = true;
        for (int j = 0; j < prefix_len; j++) {
            if (!kw[j] || kw[j] != line[start + j]) {
                ok = false;
                break;
            }
        }
        if (ok && (int)strlen(kw) > prefix_len) {
            best = kw;
            break;
        }
    }

    if (!best) return;
    for (int i = prefix_len; best[i]; i++) {
        ide_insert_char(best[i]);
    }
}

static void ide_exec_command(void) {
    if (str_equal(g_ide.cmd, "w")) {
        ide_save_file();
    } else if (str_equal(g_ide.cmd, "q")) {
        if (g_ide.win) {
            desk_window_close(g_ide.win);
            g_ide.win = NULL;
        }
    } else if (str_equal(g_ide.cmd, "wq")) {
        ide_save_file();
        if (g_ide.win) {
            desk_window_close(g_ide.win);
            g_ide.win = NULL;
        }
    } else {
        ide_set_status("unknown cmd");
    }

    g_ide.cmd_mode = false;
    g_ide.cmd_len = 0;
    g_ide.cmd[0] = '\0';
}

static bool ide_handle_click(int cx, int cy, int view_rows) {
    if (cy >= IDE_BTN_Y && cy < IDE_BTN_Y + IDE_BTN_H) {
        if (cx >= IDE_BTN_OPEN_X && cx < IDE_BTN_OPEN_X + IDE_BTN_W) {
            ide_begin_open_dialog();
            return true;
        }
        if (cx >= IDE_BTN_SAVE_X && cx < IDE_BTN_SAVE_X + IDE_BTN_W) {
            ide_save_file();
            return true;
        }
        if (cx >= IDE_BTN_NEW_X && cx < IDE_BTN_NEW_X + IDE_BTN_W) {
            ide_new_file();
            return true;
        }
    }

    if (cy >= IDE_TEXT_Y && cy < IDE_TEXT_Y + view_rows * IDE_LINE_H && cx >= IDE_TEXT_X) {
        int row = (cy - IDE_TEXT_Y) / IDE_LINE_H;
        int col = (cx - IDE_TEXT_X) / 6;
        g_ide.cursor_y = g_ide.scroll_y + row;
        g_ide.cursor_x = col;
        ide_cursor_clamp();
        ide_mark_dirty();
        return true;
    }

    return false;
}

static void ide_handle_key(char key, int view_rows) {
    if (g_ide.open_dialog) {
        if (key == '\n') {
            ide_confirm_open_dialog();
        } else if (key == 27) {
            g_ide.open_dialog = false;
            ide_set_status("open canceled");
        } else if ((key == '\b' || key == 127) && g_ide.open_len > 0) {
            g_ide.open_len--;
            g_ide.open_name[g_ide.open_len] = '\0';
            ide_mark_dirty();
        } else if ((uint8_t)key >= 32 && g_ide.open_len < (int)sizeof(g_ide.open_name) - 1) {
            g_ide.open_name[g_ide.open_len++] = key;
            g_ide.open_name[g_ide.open_len] = '\0';
            ide_mark_dirty();
        }
        return;
    }

    if (g_ide.cmd_mode) {
        if (key == '\n') ide_exec_command();
        else if (key == 27) {
            g_ide.cmd_mode = false;
            g_ide.cmd_len = 0;
            g_ide.cmd[0] = '\0';
        } else if ((key == '\b' || key == 127) && g_ide.cmd_len > 0) {
            g_ide.cmd_len--;
            g_ide.cmd[g_ide.cmd_len] = '\0';
        } else if ((uint8_t)key >= 32 && g_ide.cmd_len < IDE_CMD_MAX) {
            g_ide.cmd[g_ide.cmd_len++] = key;
            g_ide.cmd[g_ide.cmd_len] = '\0';
        }
        ide_mark_dirty();
        return;
    }

    if (g_ide.insert_mode) {
        if (key == 27) {
            g_ide.insert_mode = false;
        } else if (key == '\n') {
            ide_newline();
        } else if (key == '\t') {
            ide_try_completion();
        } else if (key == '\b' || key == 127) {
            ide_backspace();
        } else if ((uint8_t)key >= 32 && (uint8_t)key <= 126) {
            ide_insert_char(key);
        }
    } else {
        if (key == '^') g_ide.cursor_y--;
        else if (key == 'v') g_ide.cursor_y++;
        else if (key == '<') g_ide.cursor_x--;
        else if (key == '>') g_ide.cursor_x++;
        else if (key == 'i' || key == 'I') g_ide.insert_mode = true;
        else if (key == ':') {
            g_ide.cmd_mode = true;
            g_ide.cmd_len = 0;
            g_ide.cmd[0] = '\0';
        } else if (key == 's' || key == 'S') {
            ide_save_file();
        }
    }

    ide_cursor_clamp();
    ide_ensure_visible(view_rows);
    ide_mark_dirty();
}

static void app_ide_paint(desk_window_t* win, void* user_data) {
    (void)user_data;
    int cw = desk_window_client_width(win);
    int ch = desk_window_client_height(win);
    int text_x = IDE_TEXT_X;
    int text_y = IDE_TEXT_Y;
    int line_h = IDE_LINE_H;
    int row_count = (ch - 54) / line_h;

    if (row_count < 1) row_count = 1;

    g_ide.win = win;

    desk_gfx_fill_rect(win, 0, 0, cw, ch, 0x0f1720);
    desk_gfx_fill_rect(win, 0, 0, cw, 24, 0x1b2a3d);
    desk_gfx_text(win, 8, 7, "IDE", 1, 0x8fd1ff);
    desk_gfx_text(win, 40, 7, g_ide.filename, 1, 0xe8f2ff);
    desk_gfx_fill_rect(win, IDE_BTN_OPEN_X, IDE_BTN_Y, IDE_BTN_W, IDE_BTN_H, 0x3c5a78);
    desk_gfx_fill_rect(win, IDE_BTN_SAVE_X, IDE_BTN_Y, IDE_BTN_W, IDE_BTN_H, 0x3c7850);
    desk_gfx_fill_rect(win, IDE_BTN_NEW_X, IDE_BTN_Y, IDE_BTN_W, IDE_BTN_H, 0x785a3c);
    desk_gfx_text(win, IDE_BTN_OPEN_X + 18, IDE_BTN_Y + 2, "OPEN", 1, 0xf5f5f5);
    desk_gfx_text(win, IDE_BTN_SAVE_X + 18, IDE_BTN_Y + 2, "SAVE", 1, 0xf5f5f5);
    desk_gfx_text(win, IDE_BTN_NEW_X + 22, IDE_BTN_Y + 2, "NEW", 1, 0xf5f5f5);

    for (int r = 0; r < row_count; r++) {
        int li = g_ide.scroll_y + r;
        int y = text_y + r * line_h;
        char lno[8];
        if (li >= g_ide.line_count) break;

        u32_to_dec((uint32_t)(li + 1), lno, (int)sizeof(lno));
        desk_gfx_text(win, 6, y, lno, 1, 0x4db16c);

        {
            char* line = g_ide.lines[li];
            int len = (int)strlen(line);
            int comment_at = -1;
            for (int i = 0; i + 1 < len; i++) {
                if (line[i] == '/' && line[i + 1] == '/') { comment_at = i; break; }
            }

            for (int c = 0; c < len && c < IDE_MAX_COLS; c++) {
                char s[2] = {line[c], '\0'};
                uint32_t col = 0xdbe8f7;

                if (comment_at >= 0 && c >= comment_at) {
                    col = 0x76c893;
                } else if (line[c] >= '0' && line[c] <= '9') {
                    col = 0x9ddfff;
                } else if (line[c] == '"' || line[c] == '\'') {
                    col = 0xe9d08f;
                } else if (ide_is_ident_char(line[c]) && !(line[c] >= '0' && line[c] <= '9')) {
                    int st = c;
                    int en = c;
                    while (st > 0 && ide_is_ident_char(line[st - 1])) st--;
                    while (en + 1 < len && ide_is_ident_char(line[en + 1])) en++;
                    if (ide_is_keyword(&line[st], en - st + 1)) col = 0x74a8ff;
                }

                desk_gfx_text(win, text_x + c * 6, y, s, 1, col);
            }
        }
    }

    desk_gfx_fill_rect(win, 0, ch - 20, cw, 20, 0x111d2a);
    if (g_ide.cmd_mode) {
        char cmdline[IDE_CMD_MAX + 4];
        cmdline[0] = ':';
        cmdline[1] = '\0';
        str_append(cmdline, (int)sizeof(cmdline), g_ide.cmd);
        desk_gfx_text(win, 8, ch - 14, cmdline, 1, 0xffd58a);
    } else {
        desk_gfx_text(win, 8, ch - 14, g_ide.insert_mode ? "-- INSERT -- TAB complete" : "-- NORMAL -- i insert  :w :q", 1, 0x8ea4bc);
    }
    desk_gfx_text(win, cw - 210, ch - 14, g_ide.status, 1, 0x7ca2c5);

    if (window_index(win) == g_active_window && !g_ide.cmd_mode) {
        int cy = g_ide.cursor_y - g_ide.scroll_y;
        if (cy >= 0 && cy < row_count) {
            int cx = g_ide.cursor_x;
            desk_gfx_fill_rect(win, text_x + cx * 6, text_y + cy * line_h + 7, 5, 1, 0x6ec5ff);
        }
    }

    if (g_ide.open_dialog) {
        int dw = cw - 120;
        int dh = 52;
        int dx;
        int dy;

        if (dw < 180) dw = cw - 20;
        if (dw < 120) dw = 120;
        dx = (cw - dw) / 2;
        dy = (ch - dh) / 2;

        desk_gfx_fill_rect(win, dx, dy, dw, dh, 0x1e232d);
        desk_gfx_fill_rect(win, dx + 1, dy + 1, dw - 2, 12, 0x465578);
        desk_gfx_text(win, dx + 5, dy + 2, "Open file", 1, 0xf5f5f5);
        desk_gfx_fill_rect(win, dx + 6, dy + 18, dw - 12, 14, 0x12141a);
        desk_gfx_text(win, dx + 8, dy + 20, g_ide.open_name, 1, 0xe6e6e6);
        if ((g_ui_tick / 20) % 2 == 0) {
            int ocx = dx + 8 + g_ide.open_len * 6;
            desk_gfx_fill_rect(win, ocx, dy + 19, 1, 10, 0xffffff);
        }
        desk_gfx_text(win, dx + 6, dy + 36, "ENTER open  ESC cancel", 1, 0xaab9d2);
    }
}

static void app_open_notepad(void) {
    int x = 86 + g_spawn_offset;
    int y = 76 + g_spawn_offset / 2;
    (void)desk_window_create("NOTEPAD", x, y, 360, 280,
                             DESK_WINDOW_CLOSABLE, app_notepad_paint, NULL);
    g_spawn_offset = (g_spawn_offset + 26) % 180;
}

static void app_open_system(void) {
    int x = 120 + g_spawn_offset;
    int y = 90 + g_spawn_offset / 2;
    (void)desk_window_create("SYSTEM", x, y, 340, 250,
                             DESK_WINDOW_CLOSABLE, app_system_paint, NULL);
    g_spawn_offset = (g_spawn_offset + 20) % 180;
}

static void app_open_about(void) {
    int x = 160 + g_spawn_offset;
    int y = 110 + g_spawn_offset / 2;
    (void)desk_window_create("ABOUT RINGOS", x, y, 400, 250,
                             DESK_WINDOW_CLOSABLE, app_about_paint, NULL);
    g_spawn_offset = (g_spawn_offset + 18) % 180;
}

static void app_open_ide(void) {
    if (g_ide.win && window_index(g_ide.win) >= 0) {
        set_active_window(window_index(g_ide.win));
        return;
    }

    int x = 140 + g_spawn_offset;
    int y = 96 + g_spawn_offset / 2;
    g_ide.win = desk_window_create("IDE", x, y, 560, 340,
                                   DESK_WINDOW_CLOSABLE, app_ide_paint, NULL);
    if (!g_ide.initialized) {
        ide_clear();
        g_ide.initialized = true;
    }
    ide_load_file("APP.C");
    g_spawn_offset = (g_spawn_offset + 18) % 180;
}

static void desk_bootstrap(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        g_windows[i].used = false;
        g_windows[i].visible = false;
        g_windows[i].dirty = false;
    }
    for (int i = 0; i < MAX_APPS; i++) g_apps[i].used = false;

    g_zcount = 0;
    g_active_window = -1;
    g_app_count = 0;
    g_start_menu_open = false;
    g_spawn_offset = 0;
    g_terminal.win = NULL;
    g_ide.win = NULL;

    desk_register_app("NOTEPAD", app_open_notepad);
    desk_register_app("EXPLORER", app_open_explorer);
    desk_register_app("SHELL", app_open_shell);
    desk_register_app("IDE", app_open_ide);
    desk_register_app("SYSTEM", app_open_system);
    desk_register_app("ABOUT", app_open_about);
    desk_register_user_apps();

    app_open_notepad();
    app_open_shell();
    app_open_system();
}

void gui_run(void) {
    if (!vbe_is_available()) {
        vga_writestr("\nVBE not available, GUI disabled.\n");
        return;
    }

    g_fb_phys = detect_lfb_phys_addr();
    g_fb = (volatile uint8_t*)(uint32_t)g_fb_phys;

    if (!vbe_try_mode(FB_W, FB_H, 32) &&
        !vbe_try_mode(FB_W, FB_H, 24) &&
        !vbe_try_mode(FB_W, FB_H, 16)) {
        vbe_disable();
        vga_set_color(VGA_WHITE, VGA_BLACK);
        vga_clear();
        vga_writestr("\nFailed to set VBE graphics mode.\n");
        return;
    }

    g_draw_w = (int)g_fb_w;
    g_draw_h = (int)g_fb_h;
    if (g_draw_w > BACKBUF_MAX_W) g_draw_w = BACKBUF_MAX_W;
    if (g_draw_h > BACKBUF_MAX_H) g_draw_h = BACKBUF_MAX_H;

    g_mouse_x = (int)g_fb_w / 2;
    g_mouse_y = (int)g_fb_h / 2;
    if (g_mouse_x >= g_draw_w) g_mouse_x = g_draw_w / 2;
    if (g_mouse_y >= g_draw_h) g_mouse_y = g_draw_h / 2;
    g_mouse_left_down = false;
    g_cursor_visible = false;
    g_request_shutdown = false;
    g_drag_window = -1;
    g_desktop_needs_redraw = true;

    mouse_init();
    desk_bootstrap();

    draw_desktop();
    blit_backbuffer();
    cursor_refresh(true);

    while (!g_request_shutdown) {
        bool mouse_moved = false;
        bool left_pressed = false;
        bool left_released = false;
        bool redraw = false;
        char key;

        g_ui_tick++;

        mouse_poll(&mouse_moved, &left_pressed, &left_released);

        for (int key_reads = 0; key_reads < 32 && (key = keyboard_poll_char()) != 0; key_reads++) {
            bool handled = false;

            if (g_terminal.win && window_index(g_terminal.win) == g_active_window) {
                if (key == '\n') {
                    terminal_submit_input();
                    redraw = true;
                    handled = true;
                } else if (key == '\b') {
                    if (g_terminal.input_len > 0) {
                        g_terminal.input_len--;
                        g_terminal.input[g_terminal.input_len] = '\0';
                        terminal_mark_dirty();
                        redraw = true;
                    }
                    handled = true;
                } else if ((uint8_t)key >= 32 && g_terminal.input_len < TERM_INPUT_MAX) {
                    g_terminal.input[g_terminal.input_len++] = key;
                    g_terminal.input[g_terminal.input_len] = '\0';
                    terminal_mark_dirty();
                    redraw = true;
                    handled = true;
                }
            } else if (g_explorer.win && window_index(g_explorer.win) == g_active_window) {
                if (g_explorer.rename_mode) {
                    if (key == '\n') {
                        explorer_confirm_rename();
                        redraw = true;
                        handled = true;
                    } else if (key == 27) {
                        explorer_cancel_rename();
                        redraw = true;
                        handled = true;
                    } else if (key == '\b' || key == 127) {
                        if (g_explorer.rename_len > 0) {
                            g_explorer.rename_len--;
                            g_explorer.rename_buf[g_explorer.rename_len] = '\0';
                            explorer_mark_dirty();
                            redraw = true;
                        }
                        handled = true;
                    } else if ((uint8_t)key >= 32 && g_explorer.rename_len < EXPL_NAME_MAX) {
                        g_explorer.rename_buf[g_explorer.rename_len++] = key;
                        g_explorer.rename_buf[g_explorer.rename_len] = '\0';
                        explorer_mark_dirty();
                        redraw = true;
                        handled = true;
                    }
                } else {
                    if (key == '^' || key == 'w' || key == 'W') {
                        if (g_explorer.selected > 0) {
                            g_explorer.selected--;
                            explorer_select_clamp();
                            explorer_load_preview();
                            explorer_mark_dirty();
                            redraw = true;
                        }
                        handled = true;
                    } else if (key == 'v' || key == 's' || key == 'S') {
                        if (g_explorer.selected < g_explorer.item_count - 1) {
                            g_explorer.selected++;
                            explorer_select_clamp();
                            explorer_load_preview();
                            explorer_mark_dirty();
                            redraw = true;
                        }
                        handled = true;
                    } else if (key == '\n') {
                        explorer_open_selected();
                        redraw = true;
                        handled = true;
                    } else if (key == 127 || key == 'd' || key == 'D') {
                        explorer_delete_selected();
                        redraw = true;
                        handled = true;
                    } else if (key == 'r' || key == 'R') {
                        explorer_start_rename();
                        redraw = true;
                        handled = true;
                    }
                }
            } else if (g_ide.win && window_index(g_ide.win) == g_active_window) {
                int ch = desk_window_client_height(g_ide.win);
                int rows = (ch - 54) / IDE_LINE_H;
                if (rows < 1) rows = 1;
                ide_handle_key(key, rows);
                redraw = true;
                handled = true;
            }

            if (!handled && key == 27) {
                g_request_shutdown = true;
                break;
            }
        }

        if (g_request_shutdown) break;

        if (left_pressed) {
            int task_y = (int)g_fb_h - TASKBAR_H;

            if (point_in_rect(g_mouse_x, g_mouse_y, 8, task_y + 6, START_W - 12, TASKBAR_H - 12)) {
                g_start_menu_open = !g_start_menu_open;
                redraw = true;
            } else if (g_start_menu_open) {
                int item = start_menu_item_at(g_mouse_x, g_mouse_y);
                if (item >= 0 && item < g_app_count) {
                    g_apps[item].entry();
                    g_start_menu_open = false;
                    redraw = true;
                } else if (item == -2) {
                    g_request_shutdown = true;
                    break;
                } else if (item == -3) {
                    g_start_menu_open = false;
                    redraw = true;
                }
            } else if (point_in_rect(g_mouse_x, g_mouse_y, START_W + 8, task_y, (int)g_fb_w - START_W - 8, TASKBAR_H)) {
                int tw = task_window_by_point(g_mouse_x, g_mouse_y);
                if (tw >= 0) {
                    set_active_window(tw);
                    redraw = true;
                }
            } else {
                int idx = top_window_at(g_mouse_x, g_mouse_y);
                if (idx >= 0) {
                    enum hit_part_t part;
                    set_active_window(idx);
                    part = hit_test_window(idx, g_mouse_x, g_mouse_y);

                    if (part == HIT_CLOSE) {
                        desk_window_close(&g_windows[idx]);
                    } else if (part == HIT_TITLE) {
                        g_drag_window = idx;
                        g_drag_off_x = g_mouse_x - g_windows[idx].x;
                        g_drag_off_y = g_mouse_y - g_windows[idx].y;
                    } else if (part == HIT_CLIENT) {
                        if (&g_windows[idx] == g_explorer.win) {
                            desk_rect_t cr;
                            window_client_rect(&g_windows[idx], &cr);
                            explorer_handle_click(g_mouse_x - cr.x, g_mouse_y - cr.y);
                        } else if (&g_windows[idx] == g_ide.win) {
                            desk_rect_t cr;
                            int rows;
                            window_client_rect(&g_windows[idx], &cr);
                            rows = (desk_window_client_height(&g_windows[idx]) - 54) / IDE_LINE_H;
                            if (rows < 1) rows = 1;
                            if (ide_handle_click(g_mouse_x - cr.x, g_mouse_y - cr.y, rows)) {
                                ide_mark_dirty();
                            }
                        }
                    }
                    redraw = true;
                } else if (g_start_menu_open) {
                    g_start_menu_open = false;
                    redraw = true;
                }
            }
        }

        if (mouse_moved && g_drag_window >= 0 && g_mouse_left_down && g_windows[g_drag_window].used) {
            int nx = g_mouse_x - g_drag_off_x;
            int ny = g_mouse_y - g_drag_off_y;
            int max_x = (int)g_fb_w - g_windows[g_drag_window].w;
            int max_y = (int)g_fb_h - TASKBAR_H - g_windows[g_drag_window].h;

            nx = clamp_int(nx, 0, max_x < 0 ? 0 : max_x);
            ny = clamp_int(ny, 0, max_y < 0 ? 0 : max_y);

            if (nx != g_windows[g_drag_window].x || ny != g_windows[g_drag_window].y) {
                g_windows[g_drag_window].x = nx;
                g_windows[g_drag_window].y = ny;
                g_windows[g_drag_window].dirty = true;
                redraw = true;
            }
        }

        if (left_released) {
            g_drag_window = -1;
        }

        if (g_desktop_needs_redraw || any_window_dirty()) redraw = true;

        if (redraw) {
            g_desktop_needs_redraw = false;
            draw_desktop();
            blit_backbuffer();
            g_cursor_visible = false;
            cursor_refresh(true);
        } else if (mouse_moved) {
            cursor_refresh(false);
        }
    }

    shutdown_system();
}
