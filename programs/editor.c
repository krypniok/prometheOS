#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../drivers/hdd.h"
#include "../stdlibs/string.h"
#include "../kernel/util.h"
#include "../kernel/ui.h"

#include "../stdlibs/stdio_fs.h"

#include "editor.h"

// Optional callbacks used to integrate with DB/runtime
static editor_save_cb_t g_editor_save_cb = 0;
static editor_run_cb_t  g_editor_run_cb  = 0;
void editor_set_save_callback(editor_save_cb_t cb) { g_editor_save_cb = cb; }
void editor_set_run_callback(editor_run_cb_t cb)  { g_editor_run_cb  = cb; }

#define MAX_BUFFER_SIZE (1024 * 1024)
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define VIEW_ROWS (SCREEN_HEIGHT - 1)
#define HEX_ROWS 23
#define HEX_COLS 16
#define HEX_PAGE_BYTES (HEX_ROWS * HEX_COLS)
#define MAX_EDIT_INDEX (MAX_BUFFER_SIZE - 2)

static unsigned char g_editor_buffer[MAX_BUFFER_SIZE];
static char g_last_filename[256] = {0};
static char g_last_search[64] = {0};
static char g_last_replace[64] = {0};

unsigned char* editor_get_buffer(void) { return g_editor_buffer; }
int editor_buffer_capacity(void) { return MAX_BUFFER_SIZE; }
void editor_reset_buffer(void) { memset(g_editor_buffer, 0, MAX_BUFFER_SIZE); }

void editor_set_filename_hint(const char* name) {
    if (!name || !name[0]) {
        g_last_filename[0] = '\0';
        return;
    }
    size_t len = strlen(name);
    if (len >= sizeof(g_last_filename)) len = sizeof(g_last_filename) - 1;
    memcpy(g_last_filename, name, len);
    g_last_filename[len] = '\0';
}

static const char* editor_filename_label(void) {
    return g_last_filename[0] ? g_last_filename : "(untitled)";
}

typedef enum { MODE_TEXT = 0, MODE_HEX = 1 } editor_mode_t;

static void editor_draw_status_bar(editor_mode_t mode, int modified, int line, int column,
                                   int total_lines, int length_bytes, int hex_offset) {
    set_color(FG_BLACK | BG_CYAN);
    set_cursor_xy(0, 0);
    const char* mode_str = (mode == MODE_TEXT) ? "TEXT" : "HEX";
    char mod_char = modified ? '*' : ' ';
    printf(" %s%c %s  ", mode_str, mod_char, editor_filename_label());
    if (mode == MODE_TEXT) {
        if (line < 1) line = 1;
        if (column < 1) column = 1;
        if (total_lines < 1) total_lines = 1;
        printf("Ln %d/%d Col %d  ", line, total_lines, column);
    } else {
        if (hex_offset < 0) hex_offset = 0;
        printf("Off 0x%06X  ", (unsigned int)hex_offset);
    }
    printf("Len %d bytes", length_bytes);
    int cur = get_cursor();
    int col = (cur / 2) % SCREEN_WIDTH;
    for (int i = col; i < SCREEN_WIDTH; i++) printf(" ");
    set_color(FG_WHITE | BG_LIGHT_BLUE);
}

static void display_text_buffer(const unsigned char *text_buffer, int to_print) {
    set_color(FG_WHITE | BG_LIGHT_BLUE);
    int current_line = 0;
    int col = 0;
    for (int i = 0; i < to_print; i++) {
        unsigned char ch = text_buffer[i];
        if (ch == '\0') break;
        if (ch == '\n') {
            printf("\n");
            current_line++;
            col = 0;
            if (current_line >= VIEW_ROWS) break;
            continue;
        }
        if (ch == '\t') {
            int spaces = 4 - (col % 4);
            for (int s = 0; s < spaces; s++) printf(" ");
            col += spaces;
            continue;
        }
        if (ch != 0x0D) {
            printf("%c", ch);
            if (col < SCREEN_WIDTH - 1) col++;
        }
    }
}

static int count_lines_n(const unsigned char *buf, int n) {
    int lines = 1;
    for (int i = 0; i < n; i++) if (buf[i] == '\n') lines++;
    return lines;
}

static int my_strnlen(const char* s, int max) {
    int i = 0;
    while (i < max && s[i] != '\0') i++;
    return i;
}

static void editor_draw_hex_view(const unsigned char* buf, int total_len, int page) {
    int start = page * HEX_PAGE_BYTES;
    int eof_drawn = 0;
    for (int row = 0; row < HEX_ROWS; row++) {
        int offset = start + row * HEX_COLS;
        set_cursor_xy(0, 1 + row);
        if (offset >= total_len) {
            if (!eof_drawn) {
                printf("  << EOF >>");
                for (int i = 11; i < SCREEN_WIDTH; i++) printf(" ");
                eof_drawn = 1;
            } else {
                for (int i = 0; i < SCREEN_WIDTH; i++) printf(" ");
            }
            continue;
        }
        printf("%06X: ", offset);
        int count = total_len - offset;
        if (count > HEX_COLS) count = HEX_COLS;
        for (int i = 0; i < HEX_COLS; i++) {
            if (i < count) printf("%02X ", buf[offset + i]);
            else printf("   ");
        }
        printf(" ");
        for (int i = 0; i < HEX_COLS; i++) {
            if (i < count) {
                unsigned char v = buf[offset + i];
                char c = (v >= 32 && v <= 126) ? (char)v : '.';
                printf("%c", c);
            } else {
                printf(" ");
            }
        }
        for (int i = 0; i < 80 - (8 + 1 + HEX_COLS * 3 + 1 + HEX_COLS); i++) printf(" ");
    }
}

void editor_exit() {
    set_color(FG_WHITE | BG_BLACK);
    clear_screen();
}

static void *memmove_safe(void *dst, const void *src, size_t n){
    unsigned char *d = (unsigned char*)dst; const unsigned char *s = (const unsigned char*)src;
    if (!n || d==s) return dst;
    if (d < s) for (size_t i=0;i<n;i++) d[i]=s[i];
    else       for (size_t i=n;i>0;i--) d[i-1]=s[i-1];
    return dst;
}

static void editor_frame_begin(void){
    hidecursor();
    console_begin_batch();
}

static void editor_frame_end(int col, int row_plus1){
    set_cursor_xy(col, row_plus1);
    console_end_batch();
    showcursor();
}

static void editor_clear_viewport(void){
    char spaces[SCREEN_WIDTH+1];
    for (int i=0;i<SCREEN_WIDTH;i++) spaces[i] = ' ';
    spaces[SCREEN_WIDTH] = '\0';
    set_color(FG_WHITE | BG_LIGHT_BLUE);
    for (int r = 1; r < SCREEN_HEIGHT; r++){
        set_cursor_xy(0, r);
        printf("%s", spaces);
    }
}

static int file_load_into(const char *name, unsigned char *buf, int max_bytes){
    FILE *f = fopen(name, "rb");
    if (!f) return -1;
    int rd = (int)fread(buf, 1, max_bytes-1, f);
    fclose(f);
    if (rd < 0) rd = 0;
    int w = 0;
    for (int r = 0; r < rd; r++) {
        unsigned char c = buf[r];
        if (c == '\r') {
            if (r+1 < rd && buf[r+1] == '\n') { continue; }
            c = '\n';
        }
        buf[w++] = c;
    }
    buf[w] = '\0';
    rd = w;
    return rd;
}

static int file_save_from(const char *name, const unsigned char *buf, int len){
    FILE *f = fopen(name, "wb");
    if (!f) return -1;
    int wr = (int)fwrite(buf, 1, len, f);
    fclose(f);
    return (wr == len) ? 0 : -2;
}

static int editor_find_forward(const unsigned char* buffer, int len, int start, const char* needle){
    if (!needle || !needle[0]) return -1;
    int nlen = (int)strlen(needle);
    if (nlen <= 0 || nlen > len) return -1;
    if (start < 0) start = 0;
    for (int i = start; i <= len - nlen; i++) {
        if (memcmp(buffer + i, needle, (size_t)nlen) == 0) return i;
    }
    return -1;
}

int editor_main() {
    unsigned char* text_buffer = editor_get_buffer();
    int prefilled = 0;
    for (int i=0; i<64; i++){ if (text_buffer[i] != 0){ prefilled=1; break; } }
    int text_len = 0;
    if (prefilled) {
        text_len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
        if (text_len >= MAX_BUFFER_SIZE) { text_len = MAX_BUFFER_SIZE - 1; text_buffer[text_len] = '\0'; }
    } else {
        editor_reset_buffer();
        text_len = 0;
    }
    int num_lines = count_lines_n(text_buffer, text_len);
    int scroll_offset = 0;
    int cursor_pos = text_len;
    int desired_col = -1;
    editor_mode_t mode = MODE_TEXT;
    int modified = 0;

    int hex_page = 0;
    unsigned char hex_cx = 0;
    unsigned char hex_cy = 0;
    int hex_hi = 1;

    auto int ctrl_active(void){ return is_key_pressed(SC_LEFT_CTRL) || is_key_pressed(SC_RIGHT_CTRL); };
    auto int is_word_sep(unsigned char ch){ return (ch==' '||ch=='\n'||ch=='\t'||ch=='\r'); };
    auto int line_start_before(int pos) {
        if (pos <= 0) return 0;
        int i = pos - 1;
        while (i > 0 && text_buffer[i-1] != '\n') i--;
        return i;
    };
    auto int line_start_next(int pos) {
        int i = pos;
        while (i < text_len && text_buffer[i] != '\n') i++;
        if (i < text_len && text_buffer[i] == '\n') i++;
        if (i > text_len) i = text_len;
        return i;
    };
    auto int line_length_at(int start) {
        int i = start;
        while (i < text_len && text_buffer[i] != '\n') i++;
        return i - start;
    };
    auto void ensure_visible(void) {
        if (cursor_pos < scroll_offset) {
            scroll_offset = line_start_before(cursor_pos);
            return;
        }
        int row = 0;
        for (int i = scroll_offset; i < cursor_pos && row < 10000; i++) if (text_buffer[i] == '\n') row++;
        while (row >= VIEW_ROWS) {
            scroll_offset = line_start_next(scroll_offset);
            row--;
        }
    };
    auto void update_hex_cursor_from_pos(int pos){
        if (pos < 0) pos = 0;
        if (pos > text_len) pos = text_len;
        hex_page = pos / HEX_PAGE_BYTES;
        int within = pos % HEX_PAGE_BYTES;
        hex_cy = (unsigned char)(within / HEX_COLS);
        hex_cx = (unsigned char)(within % HEX_COLS);
        hex_hi = 1;
    };
    auto void focus_offset(int pos){
        if (pos < 0) pos = 0;
        if (pos > text_len) pos = text_len;
        cursor_pos = pos;
        desired_col = -1;
        ensure_visible();
        update_hex_cursor_from_pos(cursor_pos);
    };
    auto void scroll_view_lines(int delta){
        if (delta > 0) {
            for (int i=0;i<delta;i++) {
                int next = line_start_next(scroll_offset);
                if (next == scroll_offset) break;
                scroll_offset = next;
            }
        } else if (delta < 0) {
            for (int i=0;i<-delta;i++) {
                int prev = line_start_before(scroll_offset);
                if (prev == scroll_offset) break;
                scroll_offset = prev;
            }
        }
    };
    auto void cursor_up(void){
        int cur_line_start = line_start_before(cursor_pos);
        int prev_line_start = line_start_before(cur_line_start);
        int cur_col = cursor_pos - cur_line_start;
        if (desired_col < 0) desired_col = cur_col;
        int prev_len = line_length_at(prev_line_start);
        int target_col = desired_col < prev_len ? desired_col : prev_len;
        cursor_pos = prev_line_start + target_col;
    };
    auto void cursor_down(void){
        int next_start = line_start_next(cursor_pos);
        int cur_line_start = line_start_before(cursor_pos);
        int cur_col = cursor_pos - cur_line_start;
        if (desired_col < 0) desired_col = cur_col;
        int next_len = line_length_at(next_start);
        int target_col = desired_col < next_len ? desired_col : next_len;
        cursor_pos = next_start + target_col;
    };
    auto void move_cursor_lines(int delta){
        if (delta < 0) {
            for (int i=0;i<-delta;i++) {
                if (cursor_pos == 0) break;
                cursor_up();
            }
        } else if (delta > 0) {
            for (int i=0;i<delta;i++) {
                if (cursor_pos >= text_len) break;
                cursor_down();
            }
        }
    };
    auto int compute_line_number(int pos){
        int ln = 1;
        for (int i=0;i<pos;i++) if (text_buffer[i]=='\n') ln++;
        return ln;
    };
    auto int compute_column_number(int pos){
        int col = 0;
        for (int i=line_start_before(pos); i<pos && i<text_len; i++) {
            unsigned char ch = text_buffer[i];
            if (ch == '\t') { int add = 4 - (col % 4); col += add; }
            else col++;
        }
        return col + 1;
    };

    while (1) {
        int status_line = compute_line_number(cursor_pos);
        int status_col = compute_column_number(cursor_pos);
        int hex_offset = hex_page * HEX_PAGE_BYTES + hex_cy * HEX_COLS + hex_cx;

        editor_frame_begin();
        editor_draw_status_bar(mode, modified, status_line, status_col, num_lines, text_len, hex_offset);
        editor_clear_viewport();

        if (mode == MODE_TEXT) {
            ensure_visible();
            set_cursor_xy(0, 1);
            int visible = text_len - scroll_offset;
            if (visible < 0) visible = 0;
            display_text_buffer(text_buffer + scroll_offset, visible);
            int row = 0, col = 0;
            for (int i = scroll_offset; i < cursor_pos; i++) {
                unsigned char ch = text_buffer[i];
                if (ch == '\n') { row++; col = 0; }
                else if (ch == '\t') { int add = 4 - (col % 4); col += add; if (col > SCREEN_WIDTH-1) col = SCREEN_WIDTH-1; }
                else { if (col < SCREEN_WIDTH-1) col++; }
            }
            editor_frame_end(col, row + 1);
        } else {
            editor_draw_hex_view(text_buffer, text_len, hex_page);
            unsigned char px = 10 + (hex_cx * 3);
            unsigned char py = 1 + hex_cy;
            editor_frame_end(px + (hex_hi ? 0 : 1), py);
        }

        unsigned int scancode = getkey();
        if ((scancode & 0xFF) >= 0x80) continue;
        unsigned char letter = char_from_key(scancode);
        int ctrl = ctrl_active();

        if (scancode == SC_ESC) {
            editor_exit();
            break;
        }

        if (ctrl && (letter == 'f' || letter == 'F')) {
            char needle[sizeof(g_last_search)];
            needle[0] = '\0';
            if (!ui_prompt_box_ex(needle, sizeof(needle), " find ", "text: ", g_last_search[0]?g_last_search:NULL)) {
                beep(220, 120);
                continue;
            }
            strncpy(g_last_search, needle, sizeof(g_last_search)-1);
            g_last_search[sizeof(g_last_search)-1] = '\0';
            int start = cursor_pos;
            if (start < text_len) start++;
            int idx = editor_find_forward(text_buffer, text_len, start, g_last_search);
            if (idx < 0) idx = editor_find_forward(text_buffer, text_len, 0, g_last_search);
            if (idx >= 0) {
                focus_offset(idx);
            } else {
                beep(220, 120);
            }
            continue;
        }
        if (ctrl && (letter == 'n' || letter == 'N')) {
            if (!g_last_search[0]) { beep(220, 120); continue; }
            int start = cursor_pos;
            if (start < text_len) start++;
            int idx = editor_find_forward(text_buffer, text_len, start, g_last_search);
            if (idx < 0) idx = editor_find_forward(text_buffer, text_len, 0, g_last_search);
            if (idx >= 0) focus_offset(idx); else beep(220, 120);
            continue;
        }
        if (ctrl && (letter == 'r' || letter == 'R')) {
            if (!g_last_search[0]) {
                char initial[sizeof(g_last_search)]; initial[0]='\0';
                if (!ui_prompt_box_ex(initial, sizeof(initial), " replace ", "find: ", NULL)) { beep(220,120); continue; }
                strncpy(g_last_search, initial, sizeof(g_last_search)-1);
                g_last_search[sizeof(g_last_search)-1]='\0';
            }
            if (!g_last_search[0]) { beep(220,120); continue; }
            int idx = editor_find_forward(text_buffer, text_len, cursor_pos, g_last_search);
            if (idx < 0) idx = editor_find_forward(text_buffer, text_len, 0, g_last_search);
            if (idx < 0) { beep(220,120); continue; }
            char repl[sizeof(g_last_replace)];
            if (!ui_prompt_box_ex(repl, sizeof(repl), " replace ", "with: ", g_last_replace[0]?g_last_replace:NULL)) { beep(220,120); continue; }
            strncpy(g_last_replace, repl, sizeof(g_last_replace)-1);
            g_last_replace[sizeof(g_last_replace)-1] = '\0';
            int find_len = (int)strlen(g_last_search);
            int repl_len = (int)strlen(g_last_replace);
            if (idx + find_len > text_len) { beep(220,120); continue; }
            if (repl_len > find_len) {
                int delta = repl_len - find_len;
                if (text_len + delta >= MAX_BUFFER_SIZE) { beep(220,120); continue; }
                memmove_safe(&text_buffer[idx + repl_len], &text_buffer[idx + find_len], text_len - (idx + find_len) + 1);
                text_len += delta;
            } else if (repl_len < find_len) {
                int delta = find_len - repl_len;
                memmove_safe(&text_buffer[idx + repl_len], &text_buffer[idx + find_len], text_len - (idx + find_len) + 1);
                text_len -= delta;
            }
            memcpy(&text_buffer[idx], g_last_replace, repl_len);
            text_buffer[text_len] = '\0';
            num_lines = count_lines_n(text_buffer, text_len);
            modified = 1;
            cursor_pos = idx + repl_len;
            desired_col = -1;
            update_hex_cursor_from_pos(cursor_pos);
            continue;
        }

        if (ctrl && mode == MODE_TEXT && scancode == SC_LEFT_ARROW) {
            if (cursor_pos > 0) {
                int pos = cursor_pos;
                while (pos > 0 && is_word_sep(text_buffer[pos-1])) pos--;
                while (pos > 0 && !is_word_sep(text_buffer[pos-1])) pos--;
                cursor_pos = pos;
                desired_col = -1;
            }
            continue;
        }
        if (ctrl && mode == MODE_TEXT && scancode == SC_RIGHT_ARROW) {
            if (cursor_pos < text_len) {
                int pos = cursor_pos;
                while (pos < text_len && !is_word_sep(text_buffer[pos])) pos++;
                while (pos < text_len && is_word_sep(text_buffer[pos])) pos++;
                cursor_pos = pos;
                desired_col = -1;
            }
            continue;
        }
        if (mode == MODE_TEXT && scancode == SC_PAGEUP) {
            move_cursor_lines(-(VIEW_ROWS-1));
            scroll_view_lines(-(VIEW_ROWS-1));
            desired_col = -1;
            continue;
        }
        if (mode == MODE_TEXT && scancode == SC_PAGEDOWN) {
            move_cursor_lines(VIEW_ROWS-1);
            scroll_view_lines(VIEW_ROWS-1);
            desired_col = -1;
            continue;
        }

        if (scancode == SC_F1) {
            text_buffer[0] = '\0'; text_len = 0; num_lines = 1; scroll_offset = 0; cursor_pos = 0; desired_col = -1;
            modified = 1;
        } else if (scancode == SC_F3) {
            char name[256]={0};
            ui_file_prompt(name, sizeof(name), " open file ", "name: ", g_last_filename[0]?g_last_filename:NULL);
            if (name[0]) {
                int rd = file_load_into(name, text_buffer, MAX_BUFFER_SIZE);
                if (rd >= 0) {
                    editor_set_filename_hint(name);
                    text_len = rd;
                    num_lines = count_lines_n(text_buffer, text_len);
                    scroll_offset = 0; cursor_pos = 0; desired_col = -1;
                    mode = MODE_TEXT; hex_page = 0; hex_cx = hex_cy = 0; hex_hi = 1;
                    modified = 0;
                    beep(1320, 40);
                } else {
                    beep(220, 120);
                }
            }
        } else if (scancode == SC_F2) {
            if (g_editor_save_cb) {
                int len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
                g_editor_save_cb(text_buffer, len);
                modified = 0;
                beep(880, 60);
            } else {
                char tmp[256]; tmp[0]='\0';
                if (g_last_filename[0]) { strncpy(tmp, g_last_filename, sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0'; }
                if (!ui_file_prompt(tmp, sizeof(tmp), " save file ", "name: ", tmp)) { beep(220,120); continue; }
                editor_set_filename_hint(tmp);
                int len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
                int rc = file_save_from(g_last_filename, text_buffer, len);
                if (rc == 0) { beep(880, 60); modified = 0; }
                else beep(220, 120);
            }
        } else if (scancode == SC_F5) {
            editor_exit();
            if (g_editor_run_cb) g_editor_run_cb();
            printf("\n\nPress any key to continue...");
            unsigned int any; do { any = getkey(); } while ((any & 0xFF) >= 0x80);
            set_color(FG_WHITE | BG_BLACK); clear_screen();
        } else if (scancode == SC_F6) {
            mode = (mode == MODE_TEXT) ? MODE_HEX : MODE_TEXT;
        } else if (mode == MODE_TEXT && scancode == SC_ENTER) {
            if (text_len < (MAX_BUFFER_SIZE-1)) {
                memmove_safe(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = '\n';
                text_len++; num_lines++; cursor_pos++; desired_col = -1; modified = 1;
            }
        } else if (mode == MODE_TEXT && scancode == SC_BACKSPACE) {
            if (cursor_pos > 0) {
                if (text_buffer[cursor_pos-1] == '\n' && num_lines > 1) num_lines--;
                memmove_safe(&text_buffer[cursor_pos-1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_len--; cursor_pos--;
                if (scroll_offset > cursor_pos) scroll_offset = line_start_before(cursor_pos);
                desired_col = -1; modified = 1;
            }
        } else if (mode == MODE_TEXT && scancode == SC_DELETE) {
            if (cursor_pos < text_len) {
                if (text_buffer[cursor_pos] == '\n' && num_lines > 1) num_lines--;
                memmove_safe(&text_buffer[cursor_pos], &text_buffer[cursor_pos+1], text_len - cursor_pos);
                text_len--;
                modified = 1;
            }
        } else if (mode == MODE_TEXT && scancode == SC_HOME) {
            cursor_pos = line_start_before(cursor_pos);
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_END) {
            int ls = line_start_before(cursor_pos);
            cursor_pos = ls + line_length_at(ls);
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_UP_ARROW) {
            if (cursor_pos > 0) cursor_up();
        } else if (mode == MODE_TEXT && scancode == SC_DOWN_ARROW) {
            if (cursor_pos < text_len) cursor_down();
        } else if (mode == MODE_TEXT && scancode == SC_LEFT_ARROW) {
            if (cursor_pos > 0) { cursor_pos--; desired_col = -1; }
        } else if (mode == MODE_TEXT && scancode == SC_RIGHT_ARROW) {
            if (cursor_pos < text_len) { cursor_pos++; desired_col = -1; }
        } else if (mode == MODE_TEXT && scancode < 128) {
            if (letter && text_len < (MAX_BUFFER_SIZE-1)) {
                memmove_safe(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = letter;
                text_len++;
                if (letter == '\n') { num_lines++; desired_col = -1; }
                cursor_pos++;
                modified = 1;
            }
        } else if (mode == MODE_HEX) {
            int max_page = (text_len > 0) ? ((text_len - 1) / HEX_PAGE_BYTES) : 0;
            if (max_page < 0) max_page = 0;
            if (scancode == SC_PAGEUP) {
                if (hex_page > 0) hex_page--;
            } else if (scancode == SC_PAGEDOWN) {
                if (hex_page < (MAX_BUFFER_SIZE/HEX_PAGE_BYTES) - 1) hex_page++;
                if (hex_page > max_page && text_len < MAX_BUFFER_SIZE) hex_page = max_page;
            } else if (scancode == SC_UP_ARROW) {
                if (hex_cy > 0) hex_cy--;
                else if (hex_page > 0) { hex_page--; hex_cy = HEX_ROWS-1; }
            } else if (scancode == SC_DOWN_ARROW) {
                if (hex_cy < HEX_ROWS-1) hex_cy++;
                else if (hex_page < (MAX_BUFFER_SIZE/HEX_PAGE_BYTES) -1) { hex_page++; hex_cy = 0; }
            } else if (scancode == SC_LEFT_ARROW) {
                if (hex_hi == 0) hex_hi = 1;
                else if (hex_cx > 0) { hex_cx--; hex_hi = 0; }
                else if (hex_cy > 0) { hex_cy--; hex_cx = HEX_COLS-1; hex_hi = 0; }
                else if (hex_page > 0) { hex_page--; hex_cy = HEX_ROWS-1; hex_cx = HEX_COLS-1; hex_hi = 0; }
            } else if (scancode == SC_RIGHT_ARROW) {
                if (hex_hi == 1) hex_hi = 0;
                else if (hex_cx < HEX_COLS-1) { hex_cx++; hex_hi = 1; }
                else if (hex_cy < HEX_ROWS-1) { hex_cy++; hex_cx = 0; hex_hi = 1; }
                else if (hex_page < (MAX_BUFFER_SIZE/HEX_PAGE_BYTES) -1) { hex_page++; hex_cy = 0; hex_cx = 0; hex_hi = 1; }
            } else if (scancode < 128) {
                unsigned char ch = letter;
                int is_hex = ((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F'));
                unsigned char val = 0;
                if (ch>='0'&&ch<='9') val = ch-'0';
                else if (ch>='a'&&ch<='f') val = 10+(ch-'a');
                else if (ch>='A'&&ch<='F') val = 10+(ch-'A');
                if (is_hex){
                    unsigned int ofs_in_page = hex_cy*HEX_COLS + hex_cx;
                    unsigned int abs_ofs = hex_page*HEX_PAGE_BYTES + ofs_in_page;
                    if (abs_ofs <= (unsigned)MAX_EDIT_INDEX) {
                        unsigned char *bytep = text_buffer + abs_ofs;
                        unsigned char orig = *bytep;
                        if (hex_hi){ orig = (orig & 0x0F) | (val<<4); hex_hi = 0; }
                        else {
                            orig = (orig & 0xF0) | val;
                            if (hex_cx < HEX_COLS-1){ hex_cx++; hex_hi = 1; }
                            else if (hex_cy < HEX_ROWS-1){ hex_cy++; hex_cx = 0; hex_hi = 1; }
                            else if (hex_page < (MAX_BUFFER_SIZE/HEX_PAGE_BYTES)-1){ hex_page++; hex_cy=0; hex_cx=0; hex_hi=1; }
                        }
                        *bytep = orig;
                        if ((int)abs_ofs >= text_len) {
                            text_len = (int)abs_ofs + 1;
                            text_buffer[text_len] = '\0';
                        }
                        num_lines = count_lines_n(text_buffer, text_len);
                        modified = 1;
                        cursor_pos = abs_ofs;
                    }
                }
            }
        }

        if (cursor_pos > text_len) cursor_pos = text_len;
        if (cursor_pos < 0) cursor_pos = 0;
    }
    return 0;
}
