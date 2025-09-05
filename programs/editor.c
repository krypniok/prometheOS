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

static void editor_draw_status_bar() {
    set_color(FG_BLACK | BG_CYAN);
    set_cursor_xy(0, 0);
    printf("  PrometheOS Editor  |  F1:Clear  F2:Save  F5:Run  F6:Hex  ESC:Exit");
    // pad to end of line
    int cur = get_cursor();
    int col = (cur / 2) % SCREEN_WIDTH;
    for (int i = col; i < SCREEN_WIDTH; i++) printf(" ");
}

void editor_exit() {
    set_color(FG_WHITE | BG_BLACK);
    clear_screen();
}

static void insert_character(unsigned char *text_buffer, int *len, unsigned char c, int *num_lines) {
    if (*len < (MAX_BUFFER_SIZE - 1)) {
        text_buffer[*len] = c;
        (*len)++;
        text_buffer[*len] = '\0';
        if (c == '\n' && num_lines) (*num_lines)++;
    }
}

static void display_text_buffer(unsigned char *text_buffer, int to_print) {
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
            if (current_line >= SCREEN_HEIGHT - 1) break; // leave space after status bar
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

typedef enum { MODE_TEXT = 0, MODE_HEX = 1 } editor_mode_t;

int editor_main2() {
    unsigned char* text_buffer = (unsigned char*)0x300000;
    // Determine initial length (image is zero-filled after file contents)
    int text_len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
    // Ensure NUL-termination even if region is fully nonzero
    if (text_len >= MAX_BUFFER_SIZE) {
        text_len = MAX_BUFFER_SIZE - 1;
        text_buffer[text_len] = '\0';
    }
    int num_lines = count_lines_n(text_buffer, text_len);
    int scroll_offset = 0;
    int cursor_pos = text_len; // caret at end by default
    int desired_col = -1;      // preserve column across up/down
    editor_mode_t mode = MODE_TEXT;

    // HEX mode state
    int hex_page = 0;          // 368 bytes per page (23 rows * 16)
    unsigned char hex_cx = 0;  // [0..15]
    unsigned char hex_cy = 0;  // [0..22]
    int hex_hi = 1;            // edit high nibble first

    auto int line_start_before(int pos) {
        if (pos <= 0) return 0;
        int i = pos - 1;
        while (i > 0 && text_buffer[i-1] != '\n') i--;
        return i;
    }

    auto int line_start_next(int pos) {
        // start of next line after current line
        int i = pos;
        while (i < text_len && text_buffer[i] != '\n') i++;
        if (i < text_len && text_buffer[i] == '\n') i++;
        if (i > text_len) i = text_len;
        return i;
    }

    auto int line_length_at(int start) {
        int i = start;
        while (i < text_len && text_buffer[i] != '\n') i++;
        return i - start;
    }

    auto void ensure_visible(void) {
        // adjust scroll_offset to keep cursor within viewport rows
        if (cursor_pos < scroll_offset) {
            scroll_offset = line_start_before(cursor_pos);
            return;
        }
        // compute row relative to scroll
        int row = 0;
        for (int i = scroll_offset; i < cursor_pos && row < 10000; i++) {
            if (text_buffer[i] == '\n') row++;
        }
        while (row >= (SCREEN_HEIGHT - 1)) {
            scroll_offset = line_start_next(scroll_offset);
            row--;
        }
    }

    while (1) {
        console_begin_batch();
        clear_screen();
        editor_draw_status_bar();

        if (mode == MODE_TEXT) {
            ensure_visible();
            set_cursor_xy(0, 1);
            int visible = text_len - scroll_offset;
            if (visible < 0) visible = 0;
            display_text_buffer(text_buffer + scroll_offset, visible);
            // compute caret screen position (expand tabs to 4 spaces)
            int row = 0, col = 0;
            for (int i = scroll_offset; i < cursor_pos; i++) {
                unsigned char ch = text_buffer[i];
                if (ch == '\n') { row++; col = 0; }
                else if (ch == '\t') { int add = 4 - (col % 4); col += add; if (col > SCREEN_WIDTH-1) col = SCREEN_WIDTH-1; }
                else { if (col < SCREEN_WIDTH-1) col++; }
            }
            set_cursor_xy(col, row + 1);
        } else {
            // HEX mode view
            set_cursor_xy(0, 1);
            hexdump((void*)(text_buffer + (368 * hex_page)), 368);
            // place cursor on hex byte column
            unsigned char px = 10 + (hex_cx * 3);
            unsigned char py = 1 + hex_cy;
            set_cursor_xy(px + (hex_hi ? 0 : 1), py);
        }
        console_end_batch();
        unsigned int scancode = getkey();
        // Ignore key release events (low 8 bits >= 0x80)
        if ((scancode & 0xFF) >= 0x80) {
            continue;
        }

        unsigned char letter = char_from_key(scancode);
        if (scancode == SC_ESC) {
            editor_exit();
            break;
        } else if (scancode == SC_F1) {
            text_buffer[0] = '\0';
            text_len = 0;
            num_lines = 1;
            scroll_offset = 0;
        } else if (scancode == SC_F5) {
            // Execute via run-callback if set
            editor_exit();
            if (g_editor_run_cb) g_editor_run_cb();
            // Pause before returning to editor
            printf("\n\nPress any key to continue...");
            // Consume one key press (ignore releases)
            unsigned int any;
            do { any = getkey(); } while ((any & 0xFF) >= 0x80);
            // After run, return to editor UI
            set_color(FG_WHITE | BG_BLACK);
            clear_screen();
        } else if (scancode == SC_F2) {
            // On save: prefer callback into DB; fallback to raw disk write
            if (g_editor_save_cb) {
                // compute logical content length (up to first 0 or full buffer)
                int len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
                g_editor_save_cb(text_buffer, len);
            } else {
                // Save fixed 1MiB region back to disk at 1MiB (LBA 2048)
                write_to_disk_fast(2048, text_buffer, MAX_BUFFER_SIZE);
            }
            beep(880, 60);
        } else if (scancode == SC_F6) {
            // Toggle mode Text <-> Hex
            mode = (mode == MODE_TEXT) ? MODE_HEX : MODE_TEXT;
        } else if (mode == MODE_TEXT && scancode == SC_ENTER) {
            // Insert newline at caret
            if (text_len < (MAX_BUFFER_SIZE-1)) {
                memcpy(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = '\n';
                text_len++;
                num_lines++;
                cursor_pos++;
                desired_col = -1;
            }
        } else if (mode == MODE_TEXT && scancode == SC_BACKSPACE) {
            if (cursor_pos > 0) {
                if (text_buffer[cursor_pos-1] == '\n' && num_lines > 1) num_lines--;
                memcpy(&text_buffer[cursor_pos-1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_len--;
                cursor_pos--;
                if (scroll_offset > cursor_pos) scroll_offset = line_start_before(cursor_pos);
                desired_col = -1;
            }
        } else if (mode == MODE_TEXT && scroll_offset > 0 && num_lines > SCREEN_HEIGHT) {
            if (scancode == SC_UP_ARROW) {
                beep(440, 500);
                scroll_offset = line_start_before(scroll_offset);
            }
        } else if (mode == MODE_TEXT && scancode == SC_DOWN_ARROW && num_lines > SCREEN_HEIGHT) {
            if (scroll_offset < text_len) {
                scroll_offset = line_start_next(scroll_offset);
                beep(440, 500);
            }
        } else if (mode == MODE_TEXT && scancode == SC_LEFT_ARROW) {
            if (cursor_pos > 0) { cursor_pos--; }
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_RIGHT_ARROW) {
            if (cursor_pos < text_len) { cursor_pos++; }
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_HOME) {
            cursor_pos = line_start_before(cursor_pos);
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_END) {
            int ls = line_start_before(cursor_pos);
            cursor_pos = ls + line_length_at(ls);
            desired_col = -1;
        } else if (mode == MODE_TEXT && scancode == SC_DELETE) {
            if (cursor_pos < text_len) {
                if (text_buffer[cursor_pos] == '\n' && num_lines > 1) num_lines--;
                memcpy(&text_buffer[cursor_pos], &text_buffer[cursor_pos+1], text_len - cursor_pos);
                text_len--;
            }
        } else if (mode == MODE_TEXT && scancode == SC_UP_ARROW) {
            int cur_line_start = line_start_before(cursor_pos);
            int prev_line_start = line_start_before(cur_line_start);
            int cur_col = cursor_pos - cur_line_start;
            if (desired_col < 0) desired_col = cur_col;
            int prev_len = line_length_at(prev_line_start);
            int target_col = desired_col < prev_len ? desired_col : prev_len;
            cursor_pos = prev_line_start + target_col;
        } else if (mode == MODE_TEXT && scancode == SC_DOWN_ARROW) {
            int next_start = line_start_next(cursor_pos);
            int cur_line_start = line_start_before(cursor_pos);
            int cur_col = cursor_pos - cur_line_start;
            if (desired_col < 0) desired_col = cur_col;
            int next_len = line_length_at(next_start);
            int target_col = desired_col < next_len ? desired_col : next_len;
            cursor_pos = next_start + target_col;
        } else if (mode == MODE_TEXT && scancode < 128) {
            if (letter && text_len < (MAX_BUFFER_SIZE-1)) {
                // insert at cursor_pos
                memcpy(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = letter;
                text_len++;
                if (letter == '\n') { num_lines++; desired_col = -1; }
                cursor_pos++;
            }
        } else if (mode == MODE_HEX) {
            // HEX mode input handling
            if (scancode == SC_PAGEUP) { if (hex_page > 0) hex_page--; }
            else if (scancode == SC_PAGEDOWN) { hex_page++; }
            else if (scancode == SC_UP_ARROW) { if (hex_cy > 0) hex_cy--; else if (hex_page > 0) { hex_page--; hex_cy = 22; } }
            else if (scancode == SC_DOWN_ARROW) { if (hex_cy < 22) hex_cy++; else { hex_page++; hex_cy = 0; } }
            else if (scancode == SC_LEFT_ARROW) {
                if (hex_hi == 0) { hex_hi = 1; }
                else if (hex_cx > 0) { hex_cx--; hex_hi = 0; }
                else if (hex_cy > 0) { hex_cy--; hex_cx = 15; hex_hi = 0; }
                else if (hex_page > 0) { hex_page--; hex_cy = 22; hex_cx = 15; hex_hi = 0; }
            }
            else if (scancode == SC_RIGHT_ARROW) {
                if (hex_hi == 1) { hex_hi = 0; }
                else if (hex_cx < 15) { hex_cx++; hex_hi = 1; }
                else if (hex_cy < 22) { hex_cy++; hex_cx = 0; hex_hi = 1; }
                else { hex_page++; hex_cy = 0; hex_cx = 0; hex_hi = 1; }
            }
            else if (scancode < 128) {
                // convert to hex digit
                unsigned char ch = letter;
                int is_hex = ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'));
                unsigned char val = 0;
                if (ch >= '0' && ch <= '9') val = ch - '0';
                else if (ch >= 'a' && ch <= 'f') val = 10 + (ch - 'a');
                else if (ch >= 'A' && ch <= 'F') val = 10 + (ch - 'A');
                if (is_hex) {
                    // compute byte pointer within full 1MiB region
                    unsigned int ofs_in_page = hex_cy * 16 + hex_cx;
                    unsigned int abs_ofs = hex_page * 368u + ofs_in_page;
                    if (abs_ofs < MAX_BUFFER_SIZE) {
                        unsigned char *bytep = text_buffer + abs_ofs;
                        unsigned char orig = *bytep;
                        if (hex_hi) { orig = (orig & 0x0F) | (val << 4); hex_hi = 0; }
                        else { orig = (orig & 0xF0) | val; // advance
                               if (hex_cx < 15) { hex_cx++; hex_hi = 1; }
                               else if (hex_cy < 22) { hex_cy++; hex_cx = 0; hex_hi = 1; }
                               else { hex_page++; hex_cy = 0; hex_cx = 0; hex_hi = 1; } }
                        *bytep = orig;
                    }
                }
            }
        }

        
    }
    return 0;
}

/* --- kleine helfer --- */

static void *memmove_safe(void *dst, const void *src, size_t n){
    unsigned char *d = (unsigned char*)dst; const unsigned char *s = (const unsigned char*)src;
    if (!n || d==s) return dst;
    if (d < s) for (size_t i=0;i<n;i++) d[i]=s[i];
    else       for (size_t i=n;i>0;i--) d[i-1]=s[i-1];
    return dst;
}

static void draw_box(int x,int y,int w,int h,unsigned char frame,unsigned char fill){
    unsigned int old=get_color();
    unsigned char TL=0xC9,TR=0xBB,BL=0xC8,BR=0xBC,HZ=0xCD,VT=0xBA;
    console_begin_batch();
    set_color(frame);
    set_cursor_xy(x,y); printf("%c",TL);
    for(int i=0;i<w-2;i++) printf("%c",HZ);
    printf("%c",TR);
    for(int r=1;r<h-1;r++){
        set_cursor_xy(x,y+r);   printf("%c",VT);
        set_color(fill);
        for(int i=0;i<w-2;i++) printf(" ");
        set_color(frame);       printf("%c",VT);
    }
    set_cursor_xy(x,y+h-1); printf("%c",BL);
    for(int i=0;i<w-2;i++) printf("%c",HZ);
    printf("%c",BR);
    console_end_batch();
    set_color(old);
}

// prompt_filename_box replaced by ui_prompt_box() in kernel/ui.c

static int file_load_into(const char *name, unsigned char *buf, int max_bytes){
    FILE *f = fopen(name, "rb");
    if (!f) return -1;
    int rd = (int)fread(buf, 1, max_bytes-1, f);
    fclose(f);
    if (rd < 0) rd = 0;
    // Normalize line endings to '\n': convert CRLF and CR to LF in-place
    int w = 0;
    for (int r = 0; r < rd; r++) {
        unsigned char c = buf[r];
        if (c == '\r') {
            // if next is '\n', skip the '\r' and let '\n' fall through
            if (r+1 < rd && buf[r+1] == '\n') { continue; }
            // standalone CR -> convert to LF
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

/* --- editor main mit stdio-io --- */
static void editor_frame_begin(void){
    hidecursor();          // hw-cursor aus
    console_begin_batch();      // alles sammeln, kein sichtbares "zwischenframe"
}

static void editor_frame_end(int col, int row_plus1){
    set_cursor_xy(col, row_plus1);
    console_end_batch();        // einmalig flush → kein flackern
    showcursor();          // cursor wieder an
}

static void editor_clear_viewport(void){
    // nur zeilen 1..SCREEN_HEIGHT-1 (statusbar bleibt)
    char spaces[SCREEN_WIDTH+1];
    for (int i=0;i<SCREEN_WIDTH;i++) spaces[i] = ' ';
    spaces[SCREEN_WIDTH] = '\0';

    set_color(FG_WHITE | BG_LIGHT_BLUE); // editorhintergrund
    for (int r = 1; r < SCREEN_HEIGHT; r++){
        set_cursor_xy(0, r);
        printf("%s", spaces);
    }
}

int editor_main() {
    static char g_filename[256] = {0};               // merkt den letzten dateinamen
    unsigned char* text_buffer = (unsigned char*)0x300000;

    // Detect if prefilled (db_edit) vs. standalone (em)
    int prefilled = 0;
    for (int i=0;i<64;i++){ if (text_buffer[i] != 0){ prefilled=1; break; } }
    int text_len = 0;
    if (!prefilled) {
        // standalone: start with empty buffer
        for (int i=0;i<MAX_BUFFER_SIZE;i++) text_buffer[i] = 0;
        text_len = 0;
    } else {
        text_len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
        if (text_len >= MAX_BUFFER_SIZE) { text_len = MAX_BUFFER_SIZE - 1; text_buffer[text_len] = '\0'; }
    }

    int num_lines = count_lines_n(text_buffer, text_len);
    int scroll_offset = 0;
    int cursor_pos = text_len;
    int desired_col = -1;
    editor_mode_t mode = MODE_TEXT;

    // HEX mode state
    int hex_page = 0;            // 368 bytes pro seite (23*16)
    unsigned char hex_cx = 0;    // [0..15]
    unsigned char hex_cy = 0;    // [0..22]
    int hex_hi = 1;              // high-nibble zuerst

    // clamp-konstanten für hex
    #define HEX_ROWS 23
    #define HEX_COLS 16
    #define HEX_PAGE_BYTES (HEX_ROWS*HEX_COLS)

    // nested helpers wie gehabt
    auto int line_start_before(int pos) {
        if (pos <= 0) return 0;
        int i = pos - 1;
        while (i > 0 && text_buffer[i-1] != '\n') i--;
        return i;
    }
    auto int line_start_next(int pos) {
        int i = pos;
        while (i < text_len && text_buffer[i] != '\n') i++;
        if (i < text_len && text_buffer[i] == '\n') i++;
        if (i > text_len) i = text_len;
        return i;
    }
    auto int line_length_at(int start) {
        int i = start;
        while (i < text_len && text_buffer[i] != '\n') i++;
        return i - start;
    }
    auto void ensure_visible(void) {
        if (cursor_pos < scroll_offset) { scroll_offset = line_start_before(cursor_pos); return; }
        int row = 0;
        for (int i = scroll_offset; i < cursor_pos && row < 10000; i++) if (text_buffer[i]=='\n') row++;
        while (row >= (SCREEN_HEIGHT - 1)) { scroll_offset = line_start_next(scroll_offset); row--; }
    }

    while (1) {
editor_frame_begin();
editor_draw_status_bar();   // muss komplette zeile auffüllen (falls noch nicht)
editor_clear_viewport();    // löscht viewport batched (unsichtbar)
if (mode == MODE_TEXT) {
    ensure_visible();
    set_cursor_xy(0, 1);
    int visible = text_len - scroll_offset; if (visible < 0) visible = 0;
    display_text_buffer(text_buffer + scroll_offset, visible);

    // caret berechnen wie gehabt → (row,col)
    int row = 0, col = 0;
    for (int i = scroll_offset; i < cursor_pos; i++) {
        unsigned char ch = text_buffer[i];
        if (ch == '\n') { row++; col = 0; }
        else if (ch == '\t') { int add = 4 - (col % 4); col += add; if (col > SCREEN_WIDTH-1) col = SCREEN_WIDTH-1; }
        else { if (col < SCREEN_WIDTH-1) col++; }
    }
    editor_frame_end(col, row + 1);
} else {
    set_cursor_xy(0, 1);
    int start = hex_page * HEX_PAGE_BYTES;
    int remain = MAX_BUFFER_SIZE - start;
    int dump_len = remain > HEX_PAGE_BYTES ? HEX_PAGE_BYTES : (remain > 0 ? remain : 0);
    hexdump((void*)(text_buffer + start), dump_len);

    unsigned char px = 10 + (hex_cx * 3);
    unsigned char py = 1 + hex_cy;
    editor_frame_end(px + (hex_hi ? 0 : 1), py);
}


        unsigned int scancode = getkey();
        if ((scancode & 0xFF) >= 0x80) continue;
        unsigned char letter = char_from_key(scancode);

        if (scancode == SC_ESC) {
            editor_exit();
            break;

        } else if (scancode == SC_F1) {                       // clear
            text_buffer[0] = '\0'; text_len = 0; num_lines = 1; scroll_offset = 0; cursor_pos = 0; desired_col = -1;
        } else if (scancode == SC_F3) {
		char name[256]={0};
		ui_file_prompt(name, sizeof(name), " open file ", "name: ", g_filename[0]?g_filename:NULL);

            if (name[0]) {
                int rd = file_load_into(name, text_buffer, MAX_BUFFER_SIZE);
                if (rd >= 0) {
                    strncpy(g_filename, name, sizeof(g_filename)-1);
                    g_filename[sizeof(g_filename)-1] = '\0';
                    text_len = rd;
                    num_lines = count_lines_n(text_buffer, text_len);
                    scroll_offset = 0; cursor_pos = 0; desired_col = -1;
                    mode = MODE_TEXT; hex_page = 0; hex_cx = hex_cy = 0; hex_hi = 1;
                    beep(1320, 40);
                } else {
                    beep(220, 120);
                }
            }

        } else if (scancode == SC_F2) {                       // SAVE (always ask, prefill current)
            {
                char tmp[256]; tmp[0]='\0';
                if (g_filename[0]) { strncpy(tmp, g_filename, sizeof(tmp)-1); tmp[sizeof(tmp)-1]='\0'; }
                if (!ui_file_prompt(tmp, sizeof(tmp), " save file ", "name: ", tmp)) { beep(220,120); goto after_save; }
                strncpy(g_filename, tmp, sizeof(g_filename)-1);
                g_filename[sizeof(g_filename)-1] = '\0';
            }
            int len = my_strnlen((char*)text_buffer, MAX_BUFFER_SIZE);
            int rc = file_save_from(g_filename, text_buffer, len);
            if (rc == 0) beep(880, 60); else beep(220, 120);
        after_save: ;

        } else if (scancode == SC_F5) {                       // RUN
            editor_exit();
            if (g_editor_run_cb) g_editor_run_cb();
            printf("\n\nPress any key to continue...");
            unsigned int any; do { any = getkey(); } while ((any & 0xFF) >= 0x80);
            set_color(FG_WHITE | BG_BLACK); clear_screen();

        } else if (scancode == SC_F6) {                       // HEX toggle
            mode = (mode == MODE_TEXT) ? MODE_HEX : MODE_TEXT;

        } else if (mode == MODE_TEXT && scancode == SC_ENTER) {
            if (text_len < (MAX_BUFFER_SIZE-1)) {
                memmove_safe(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = '\n';
                text_len++; num_lines++; cursor_pos++; desired_col = -1;
            }

        } else if (mode == MODE_TEXT && scancode == SC_BACKSPACE) {
            if (cursor_pos > 0) {
                if (text_buffer[cursor_pos-1] == '\n' && num_lines > 1) num_lines--;
                memmove_safe(&text_buffer[cursor_pos-1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_len--; cursor_pos--;
                if (scroll_offset > cursor_pos) scroll_offset = line_start_before(cursor_pos);
                desired_col = -1;
            }

        } else if (mode == MODE_TEXT && scroll_offset > 0 && num_lines > SCREEN_HEIGHT) {
            if (scancode == SC_UP_ARROW) { scroll_offset = line_start_before(scroll_offset); }

        } else if (mode == MODE_TEXT && scancode == SC_DOWN_ARROW && num_lines > SCREEN_HEIGHT) {
            if (scroll_offset < text_len) { scroll_offset = line_start_next(scroll_offset); }

        } else if (mode == MODE_TEXT && scancode == SC_LEFT_ARROW) {
            if (cursor_pos > 0) cursor_pos--; desired_col = -1;

        } else if (mode == MODE_TEXT && scancode == SC_RIGHT_ARROW) {
            if (cursor_pos < text_len) cursor_pos++; desired_col = -1;

        } else if (mode == MODE_TEXT && scancode == SC_UP_ARROW) {
            int cur_line_start = line_start_before(cursor_pos);
            int prev_line_start = line_start_before(cur_line_start);
            int cur_col = cursor_pos - cur_line_start;
            if (desired_col < 0) desired_col = cur_col;
            int prev_len = line_length_at(prev_line_start);
            int target_col = desired_col < prev_len ? desired_col : prev_len;
            cursor_pos = prev_line_start + target_col;

        } else if (mode == MODE_TEXT && scancode == SC_DOWN_ARROW) {
            int next_start = line_start_next(cursor_pos);
            int cur_line_start = line_start_before(cursor_pos);
            int cur_col = cursor_pos - cur_line_start;
            if (desired_col < 0) desired_col = cur_col;
            int next_len = line_length_at(next_start);
            int target_col = desired_col < next_len ? desired_col : next_len;
            cursor_pos = next_start + target_col;

        } else if (mode == MODE_TEXT && scancode < 128) {
            if (letter && text_len < (MAX_BUFFER_SIZE-1)) {
                memmove_safe(&text_buffer[cursor_pos+1], &text_buffer[cursor_pos], text_len - cursor_pos + 1);
                text_buffer[cursor_pos] = letter;
                text_len++; if (letter == '\n') { num_lines++; desired_col = -1; }
                cursor_pos++;
            }

        } else if (mode == MODE_HEX) {
            if (scancode == SC_PAGEUP) { if (hex_page > 0) hex_page--; }
            else if (scancode == SC_PAGEDOWN) {
                int max_page = (MAX_BUFFER_SIZE-1)/HEX_PAGE_BYTES;
                if (hex_page < max_page) hex_page++;
            }
            else if (scancode == SC_UP_ARROW) { if (hex_cy > 0) hex_cy--; else if (hex_page > 0) { hex_page--; hex_cy = 22; } }
            else if (scancode == SC_DOWN_ARROW) { if (hex_cy < 22) hex_cy++; else { int max_page=(MAX_BUFFER_SIZE-1)/HEX_PAGE_BYTES; if (hex_page < max_page){ hex_page++; hex_cy = 0; } } }
            else if (scancode == SC_LEFT_ARROW) {
                if (hex_hi == 0) hex_hi = 1;
                else if (hex_cx > 0) { hex_cx--; hex_hi = 0; }
                else if (hex_cy > 0) { hex_cy--; hex_cx = 15; hex_hi = 0; }
                else if (hex_page > 0) { hex_page--; hex_cy = 22; hex_cx = 15; hex_hi = 0; }
            }
            else if (scancode == SC_RIGHT_ARROW) {
                int max_page = (MAX_BUFFER_SIZE-1)/HEX_PAGE_BYTES;
                if (hex_hi == 1) hex_hi = 0;
                else if (hex_cx < 15) { hex_cx++; hex_hi = 1; }
                else if (hex_cy < 22) { hex_cy++; hex_cx = 0; hex_hi = 1; }
                else if (hex_page < max_page) { hex_page++; hex_cy = 0; hex_cx = 0; hex_hi = 1; }
            }
            else if (scancode < 128) {
                unsigned char ch = letter;
                int is_hex = ((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F'));
                unsigned char val = 0;
                if (ch>='0'&&ch<='9') val = ch-'0';
                else if (ch>='a'&&ch<='f') val = 10+(ch-'a');
                else if (ch>='A'&&ch<='F') val = 10+(ch-'A');
                if (is_hex){
                    unsigned int ofs_in_page = hex_cy*16 + hex_cx;
                    unsigned int abs_ofs = hex_page*HEX_PAGE_BYTES + ofs_in_page;
                    if (abs_ofs < (unsigned)MAX_BUFFER_SIZE){
                        unsigned char *bytep = text_buffer + abs_ofs;
                        unsigned char orig = *bytep;
                        if (hex_hi){ orig = (orig & 0x0F) | (val<<4); hex_hi = 0; }
                        else {
                            orig = (orig & 0xF0) | val;
                            if (hex_cx < 15){ hex_cx++; hex_hi = 1; }
                            else if (hex_cy < 22){ hex_cy++; hex_cx = 0; hex_hi = 1; }
                            else { int max_page=(MAX_BUFFER_SIZE-1)/HEX_PAGE_BYTES; if (hex_page < max_page){ hex_page++; hex_cy=0; hex_cx=0; hex_hi=1; } }
                        }
                        *bytep = orig;
                    }
                }
            }
        }
    }
    return 0;
}
