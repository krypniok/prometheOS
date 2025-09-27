#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../kernel/ui.h"
#include "../kernel/thread.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"
#include <stdint.h>

static void fill_line(int x, int y, int width) {
    set_cursor_xy(x, y);
    for (int i = 0; i < width; ++i) {
        printf(" ");
    }
}

static void line_clear(char *line, int width) {
    for (int i = 0; i < width; ++i) {
        line[i] = ' ';
    }
    line[width] = '\0';
}

static void line_write(char *line, int column, int width, const char *text) {
    if (!text || width <= 0) {
        return;
    }
    int len = (int)strlen(text);
    if (len > width) {
        len = width;
    }
    memcpy(line + column, text, len);
}

static void line_write_hex(char *line, int column, int width, uintptr_t value) {
    char buf[16];
    sprintf(buf, "%08X", (unsigned int)value);
    line_write(line, column, width, buf);
}

void taskmgr(void) {
    uint16_t snapshot[MAX_COLS * MAX_ROWS];
    uint16_t *shadow = console_get_shadow();
    if (shadow) {
        memcpy(snapshot, shadow, sizeof(snapshot));
    }

    unsigned char old_color = get_color();
    int old_cursor = get_cursor();

    hidecursor();

    // purge leftover keypresses so the window survives its own launch key
    while (getkey_async()) {
    }

    const int frame_x = 4;
    const int frame_y = 2;
    const int frame_w = 70;
    const int frame_h = 20;

    printframe_caption(frame_x, frame_y, frame_w, frame_h, FG_WHITE | BG_BLUE, " Task Manager ");

    const int inner_x = frame_x + 2;
    const int inner_y = frame_y + 2;
    const int list_start_y = inner_y + 1;
    const int status_y = frame_y + frame_h - 4;
    const int instr_y = status_y + 1;
    int rows_available = status_y - list_start_y;
    if (rows_available < 1) {
        rows_available = 1;
    }

    enum {
        COL_MARK = 0,
        COL_ID   = 2,
        COL_STATE= 6,
        COL_STOP = 18,
        COL_ENTRY= 24,
        COL_STACK= 36
    };

    char line[128];
    line_clear(line, frame_w);
    line_write(line, COL_ID,    4, "ID");
    line_write(line, COL_STATE, 10, "STATE");
    line_write(line, COL_STOP,  4, "STOP");
    line_write(line, COL_ENTRY, 10, "ENTRY");
    line_write(line, COL_STACK, 10, "STACK");
    set_color(FG_BRIGHT_WHITE | BG_BLUE);
    set_cursor_xy(inner_x, inner_y);
    printf("%s", line);

    const char *hint = "ESC=Close  Up/Down=Select  PgUp/PgDn  ENTER=Kill";
    line_clear(line, frame_w);
    line_write(line, 0, frame_w, hint);
    set_cursor_xy(inner_x, instr_y);
    printf("%s", line);
    set_color(old_color);

    char status[64];
    status[0] = '\0';

    int selected = 0;
    int scroll = 0;

    while (1) {
        thread_info_t infos[MAX_THREADS];
        int total = thread_enumerate(infos, MAX_THREADS);
        int current = thread_current_id();

        if (total <= 0) {
            selected = 0;
            scroll = 0;
        } else {
            if (selected >= total) {
                selected = total - 1;
            }
            if (selected < 0) {
                selected = 0;
            }

            int max_scroll = total - rows_available;
            if (max_scroll < 0) {
                max_scroll = 0;
            }
            if (selected < scroll) {
                scroll = selected;
            } else if (selected >= scroll + rows_available) {
                scroll = selected - rows_available + 1;
            }
            if (scroll > max_scroll) {
                scroll = max_scroll;
            }
        }

        const unsigned char row_color = FG_WHITE | BG_BLUE;
        const unsigned char selected_color = FG_BRIGHT_WHITE | BG_LIGHT_BLUE;

        console_begin_batch();
        for (int row = 0; row < rows_available; ++row) {
            int idx = scroll + row;
            set_cursor_xy(inner_x, list_start_y + row);
            const unsigned char color = (idx == selected && total > 0) ? selected_color : row_color;
            set_color(color);

            if (idx < total) {
                const thread_info_t *info = &infos[idx];
                const char *state;
                if (!info->active) {
                    state = "inactive";
                } else if (info->id == current) {
                    state = "current";
                } else {
                    state = "ready";
                }
                const char *stop = info->should_stop ? "yes" : "no";

                line_clear(line, frame_w);
                line_write(line, COL_MARK, 1, (idx == selected) ? ">" : " ");

                char idbuf[8];
                sprintf(idbuf, "%d", info->id);
                line_write(line, COL_ID, 4, idbuf);
                line_write(line, COL_STATE, 10, state);
                line_write(line, COL_STOP, 4, stop);
                line_write_hex(line, COL_ENTRY, 10, (uintptr_t)info->entry);
                line_write_hex(line, COL_STACK, 10, (uintptr_t)info->stack);

                printf("%s", line);
            } else {
                fill_line(inner_x, list_start_y + row, frame_w);
            }
        }

        set_cursor_xy(inner_x, status_y);
        set_color(FG_YELLOW | BG_BLUE);
        int status_len = (int)strlen(status);
        if (status_len > frame_w) {
            status_len = frame_w;
        }
        if (status_len >= (int)sizeof(status)) {
            status_len = (int)sizeof(status) - 1;
        }
        if (status_len > 0) {
            char status_line[80];
            if (status_len >= (int)sizeof(status_line)) {
                status_len = (int)sizeof(status_line) - 1;
            }
            memcpy(status_line, status, status_len);
            status_line[status_len] = '\0';
            printf("%s", status_line);
        }
        for (int i = status_len; i < frame_w; ++i) {
            printf(" ");
        }
        console_end_batch();

        set_color(old_color);

        uint32_t key = getkey();
        if (key == SC_ESC) {
            break;
        } else if (key == SC_UP_ARROW) {
            if (selected > 0) {
                selected--;
                status[0] = '\0';
            }
        } else if (key == SC_DOWN_ARROW) {
            if (total > 0 && selected + 1 < total) {
                selected++;
                status[0] = '\0';
            }
        } else if (key == SC_PAGEUP) {
            if (selected > 0) {
                selected -= rows_available;
                if (selected < 0) {
                    selected = 0;
                }
                status[0] = '\0';
            }
        } else if (key == SC_PAGEDOWN) {
            if (total > 0 && selected < total - 1) {
                selected += rows_available;
                if (selected >= total) {
                    selected = total - 1;
                }
                status[0] = '\0';
            }
        } else if (key == SC_HOME) {
            if (selected != 0) {
                selected = 0;
                status[0] = '\0';
            }
        } else if (key == SC_END) {
            if (total > 0 && selected != total - 1) {
                selected = total - 1;
                status[0] = '\0';
            }
        } else if (key == SC_ENTER || key == SC_DELETE) {
            if (total > 0) {
                int target = infos[selected].id;
                int rc = thread_kill(target);
                if (rc == 0) {
                    sprintf(status, "Signaled thread %d", target);
                } else {
                    sprintf(status, "Kill failed for %d", target);
                }
            }
        } else if (key == SC_KEY_Q && is_key_pressed(SC_LEFT_CTRL)) {
            // quick escape (Ctrl+Q)
            break;
        }
    }

    showcursor();
    set_cursor(old_cursor);
    set_color(old_color);

    if (shadow) {
        console_begin_batch();
        memcpy(shadow, snapshot, sizeof(snapshot));
        console_end_batch();
        console_flush_to_vga();
    }
}
