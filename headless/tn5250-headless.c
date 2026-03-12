/* TN5250 - An implementation of the 5250 telnet protocol.
 * Copyright (C) 1997-2008 Michael Madore
 *
 * This file is part of TN5250.
 *
 * TN5250 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1, or (at your option)
 * any later version.
 *
 * TN5250 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 *
 */

#include "tn5250-private.h"
#include "headlessterm.h"
#include <pthread.h>

static Tn5250Session *sess = NULL;
static Tn5250Stream *stream = NULL;
static Tn5250Terminal *term = NULL;
static Tn5250Display *display = NULL;
static Tn5250Config *config = NULL;

static volatile int session_running = 0;

/* Key name to key code mapping */
struct key_map_entry {
    const char *name;
    int code;
};

static const struct key_map_entry key_map[] = {
    // clang-format off
    { "enter",     K_ENTER      },
    { "tab",       K_TAB        },
    { "backtab",   K_BACKTAB    },
    { "f1",        K_F1         },
    { "f2",        K_F2         },
    { "f3",        K_F3         },
    { "f4",        K_F4         },
    { "f5",        K_F5         },
    { "f6",        K_F6         },
    { "f7",        K_F7         },
    { "f8",        K_F8         },
    { "f9",        K_F9         },
    { "f10",       K_F10        },
    { "f11",       K_F11        },
    { "f12",       K_F12        },
    { "f13",       K_F13        },
    { "f14",       K_F14        },
    { "f15",       K_F15        },
    { "f16",       K_F16        },
    { "f17",       K_F17        },
    { "f18",       K_F18        },
    { "f19",       K_F19        },
    { "f20",       K_F20        },
    { "f21",       K_F21        },
    { "f22",       K_F22        },
    { "f23",       K_F23        },
    { "f24",       K_F24        },
    { "left",      K_LEFT       },
    { "right",     K_RIGHT      },
    { "up",        K_UP         },
    { "down",      K_DOWN       },
    { "pgup",      K_ROLLDN     },
    { "pgdn",      K_ROLLUP     },
    { "pageup",    K_ROLLDN     },
    { "pagedown",  K_ROLLUP     },
    { "backspace", K_BACKSPACE  },
    { "home",      K_HOME       },
    { "end",       K_END        },
    { "insert",    K_INSERT     },
    { "delete",    K_DELETE     },
    { "reset",     K_RESET      },
    { "print",     K_PRINT      },
    { "help",      K_HELP       },
    { "sysreq",    K_SYSREQ     },
    { "clear",     K_CLEAR      },
    { "fieldexit", K_FIELDEXIT  },
    { "attention",  K_ATTENTION  },
    { "duplicate",  K_DUPLICATE  },
    { "fieldminus", K_FIELDMINUS },
    { "fieldplus",  K_FIELDPLUS  },
    { "newline",    K_NEWLINE    },
    { NULL,         0            }
    // clang-format on
};

static int lookup_key(const char *name) {
    const struct key_map_entry *p;
    for (p = key_map; p->name != NULL; p++) {
        if (strcasecmp(p->name, name) == 0) {
            return p->code;
        }
    }
    return -1;
}

static void send_ok(void) {
    printf("{\"status\":\"ok\"}\n");
    fflush(stdout);
}

static void send_error(const char *msg) {
    printf("{\"status\":\"error\",\"message\":\"");
    /* Escape JSON string */
    while (*msg) {
        switch (*msg) {
        case '"':  printf("\\\""); break;
        case '\\': printf("\\\\"); break;
        case '\n': printf("\\n"); break;
        case '\r': printf("\\r"); break;
        case '\t': printf("\\t"); break;
        default:   putchar(*msg); break;
        }
        msg++;
    }
    printf("\"}\n");
    fflush(stdout);
}

static void *session_thread(void *arg) {
    (void)arg;
    session_running = 1;
    tn5250_session_main_loop(sess);
    session_running = 0;
    return NULL;
}

static void cmd_connect(const char *host) {
    pthread_t tid;

    if (stream != NULL) {
        send_error("already connected");
        return;
    }

    config = tn5250_config_new();
    if (tn5250_config_load_default(config) == -1) {
        send_error("failed to load default config");
        tn5250_config_unref(config);
        config = NULL;
        return;
    }
    tn5250_config_set(config, "host", host);

    stream = tn5250_stream_open(host, config);
    if (stream == NULL) {
        const char *err = tn5250_strerror();
        send_error(err ? err : "connection failed");
        tn5250_config_unref(config);
        config = NULL;
        return;
    }

    display = tn5250_display_new();
    if (tn5250_display_config(display, config) == -1) {
        send_error("display config failed");
        tn5250_stream_destroy(stream);
        stream = NULL;
        tn5250_config_unref(config);
        config = NULL;
        return;
    }

    term = tn5250_headless_terminal_new();
    if (term == NULL) {
        send_error("failed to create terminal");
        tn5250_stream_destroy(stream);
        stream = NULL;
        tn5250_config_unref(config);
        config = NULL;
        return;
    }

    if (tn5250_terminal_config(term, config) == -1) {
        send_error("terminal config failed");
        tn5250_terminal_destroy(term);
        term = NULL;
        tn5250_stream_destroy(stream);
        stream = NULL;
        tn5250_config_unref(config);
        config = NULL;
        return;
    }

    tn5250_terminal_init(term);
    tn5250_display_set_terminal(display, term);

    sess = tn5250_session_new();
    tn5250_display_set_session(display, sess);

    term->conn_fd = tn5250_stream_socket_handle(stream);
    tn5250_session_set_stream(sess, stream);
    if (tn5250_session_config(sess, config) == -1) {
        send_error("session config failed");
        tn5250_terminal_term(term);
        tn5250_session_destroy(sess);
        sess = NULL;
        stream = NULL; /* destroyed by session */
        term = NULL;
        tn5250_config_unref(config);
        config = NULL;
        return;
    }

    if (pthread_create(&tid, NULL, session_thread, NULL) != 0) {
        send_error("failed to start session thread");
        tn5250_terminal_term(term);
        tn5250_session_destroy(sess);
        sess = NULL;
        stream = NULL;
        term = NULL;
        tn5250_config_unref(config);
        config = NULL;
        return;
    }
    pthread_detach(tid);

    /* Give the session thread time to receive initial screen */
    usleep(500000);

    send_ok();
}

static void get_screen_text(char *buf, int bufsize) {
    int w, h, y, x, pos;
    Tn5250CharMap *map;

    if (display == NULL || tn5250_display_dbuffer(display) == NULL) {
        buf[0] = '\0';
        return;
    }

    w = tn5250_display_width(display);
    h = tn5250_display_height(display);
    map = tn5250_display_char_map(display);

    pos = 0;
    for (y = 0; y < h && pos < bufsize - 2; y++) {
        for (x = 0; x < w && pos < bufsize - 2; x++) {
            unsigned char c = tn5250_display_char_at(display, y, x);
            unsigned char local;
            if (map != NULL) {
                local = tn5250_char_map_to_local(map, c);
            }
            else {
                local = c;
            }
            buf[pos++] = (local >= 0x20 && local < 0x7f) ? local : ' ';
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';
}

static void print_json_string(const char *s) {
    putchar('"');
    while (*s) {
        switch (*s) {
        case '"':  printf("\\\""); break;
        case '\\': printf("\\\\"); break;
        case '\n': printf("\\n"); break;
        case '\r': printf("\\r"); break;
        case '\t': printf("\\t"); break;
        default:
            if ((unsigned char)*s < 0x20) {
                printf("\\u%04x", (unsigned char)*s);
            }
            else {
                putchar(*s);
            }
            break;
        }
        s++;
    }
    putchar('"');
}

static void cmd_getscreen(const char *fmt) {
    char screen[132 * 28 + 28 + 1]; /* max 132x27 + newlines */
    int cursor_y, cursor_x, indicators;

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    get_screen_text(screen, sizeof(screen));

    if (fmt != NULL && strcasecmp(fmt, "json") == 0) {
        cursor_y = tn5250_display_cursor_y(display);
        cursor_x = tn5250_display_cursor_x(display);
        indicators = tn5250_display_indicators(display);

        printf("{\"status\":\"ok\",\"screen\":");
        print_json_string(screen);
        printf(",\"cursor\":[%d,%d]", cursor_y, cursor_x);
        printf(",\"rows\":%d,\"cols\":%d",
               tn5250_display_height(display),
               tn5250_display_width(display));
        printf(",\"indicators\":{");
        printf("\"inhibit\":%s",
               (indicators & TN5250_DISPLAY_IND_INHIBIT) ? "true" : "false");
        printf(",\"message_waiting\":%s",
               (indicators & TN5250_DISPLAY_IND_MESSAGE_WAITING) ? "true"
                                                                  : "false");
        printf(",\"insert\":%s",
               (indicators & TN5250_DISPLAY_IND_INSERT) ? "true" : "false");
        printf(",\"x_system\":%s",
               (indicators & TN5250_DISPLAY_IND_X_SYSTEM) ? "true" : "false");
        printf(",\"x_clock\":%s",
               (indicators & TN5250_DISPLAY_IND_X_CLOCK) ? "true" : "false");
        printf("}}\n");
    }
    else {
        printf("{\"status\":\"ok\",\"screen\":");
        print_json_string(screen);
        printf("}\n");
    }
    fflush(stdout);
}

static void get_field_data(Tn5250Field *field, char *buf, int bufsize) {
    Tn5250CharMap *map = tn5250_display_char_map(display);
    int w = tn5250_display_width(display);
    int i, pos = 0;

    for (i = 0; i < tn5250_field_length(field) && pos < bufsize - 1; i++) {
        int fr, fc;
        unsigned char c, local;
        fr = tn5250_field_start_row(field);
        fc = tn5250_field_start_col(field) + i;
        while (fc >= w) {
            fc -= w;
            fr++;
        }
        c = tn5250_display_char_at(display, fr, fc);
        if (map != NULL) {
            local = tn5250_char_map_to_local(map, c);
        }
        else {
            local = c;
        }
        buf[pos++] = (local >= 0x20 && local < 0x7f) ? local : ' ';
    }
    buf[pos] = '\0';
}

static void print_field_json(Tn5250Field *field) {
    char data[1024];

    get_field_data(field, data, sizeof(data));
    printf("{\"id\":%d,\"row\":%d,\"col\":%d,\"length\":%d,\"data\":",
           field->id, tn5250_field_start_row(field),
           tn5250_field_start_col(field), tn5250_field_length(field));
    print_json_string(data);
    printf(",\"type\":\"%s\"", tn5250_field_description(field));
    printf(",\"bypass\":%s",
           tn5250_field_is_bypass(field) ? "true" : "false");
    printf(",\"modified\":%s",
           tn5250_field_mdt(field) ? "true" : "false");
    printf("}");
}

static void cmd_getfield(int row, int col) {
    Tn5250Field *field;

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    field = tn5250_display_field_at(display, row, col);
    if (field == NULL) {
        send_error("no field at position");
        return;
    }

    printf("{\"status\":\"ok\",\"field\":");
    print_field_json(field);
    printf("}\n");
    fflush(stdout);
}

static void cmd_getfields(void) {
    Tn5250Field *field, *first;
    Tn5250DBuffer *dbuf;
    int count = 0;

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    dbuf = tn5250_display_dbuffer(display);
    if (dbuf == NULL || dbuf->field_list == NULL) {
        printf("{\"status\":\"ok\",\"fields\":[],\"count\":0}\n");
        fflush(stdout);
        return;
    }

    printf("{\"status\":\"ok\",\"fields\":[");

    first = dbuf->field_list;
    field = first;
    do {
        if (count > 0) printf(",");
        print_field_json(field);
        count++;
        field = field->next;
    } while (field != first);

    printf("],\"count\":%d}\n", count);
    fflush(stdout);
}

static void cmd_sendkey(const char *keyname) {
    int key;

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    key = lookup_key(keyname);
    if (key < 0) {
        send_error("unknown key name");
        return;
    }

    tn5250_headless_terminal_push_key(term, key);

    /* Give the session time to process the key and any server response */
    usleep(100000); /* 100ms */

    send_ok();
}

static void cmd_type(const char *text) {
    if (display == NULL) {
        send_error("not connected");
        return;
    }

    while (*text) {
        tn5250_headless_terminal_push_key(term, (unsigned char)*text);
        text++;
    }

    usleep(50000); /* 50ms */

    send_ok();
}

static void cmd_movecursor(int row, int col) {
    if (display == NULL) {
        send_error("not connected");
        return;
    }

    tn5250_display_set_cursor(display, row, col);
    send_ok();
}

static void cmd_waitfor(const char *text, int timeout_secs) {
    int elapsed_ms = 0;
    int timeout_ms = timeout_secs * 1000;
    char screen[132 * 28 + 28 + 1];

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    while (elapsed_ms < timeout_ms) {
        get_screen_text(screen, sizeof(screen));
        if (strstr(screen, text) != NULL) {
            send_ok();
            return;
        }
        usleep(100000); /* 100ms */
        elapsed_ms += 100;
    }

    send_error("timeout waiting for text");
}

static int display_is_ready(void) {
    int ind = tn5250_display_indicators(display);
    return (ind & (TN5250_DISPLAY_IND_INHIBIT | TN5250_DISPLAY_IND_X_SYSTEM |
                   TN5250_DISPLAY_IND_X_CLOCK)) == 0;
}

static void cmd_waitready(int timeout_secs) {
    int elapsed_ms = 0;
    int timeout_ms = timeout_secs * 1000;

    if (display == NULL) {
        send_error("not connected");
        return;
    }

    while (elapsed_ms < timeout_ms) {
        if (display_is_ready()) {
            send_ok();
            return;
        }
        usleep(100000); /* 100ms */
        elapsed_ms += 100;
    }

    send_error("timeout waiting for system ready");
}

static void process_command(char *line) {
    char *cmd;
    char *arg;

    /* Strip trailing newline/carriage return */
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
    }

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    if (*line == '\0') return;

    /* Split command and arguments */
    cmd = line;
    arg = line;
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    if (*arg) {
        *arg = '\0';
        arg++;
        while (*arg == ' ' || *arg == '\t') arg++;
    }

    if (strcasecmp(cmd, "connect") == 0) {
        if (*arg == '\0') {
            send_error("usage: connect <host>");
        }
        else {
            cmd_connect(arg);
        }
    }
    else if (strcasecmp(cmd, "getscreen") == 0) {
        cmd_getscreen(*arg ? arg : NULL);
    }
    else if (strcasecmp(cmd, "getfield") == 0) {
        int row, col;
        if (sscanf(arg, "%d %d", &row, &col) != 2) {
            send_error("usage: getfield <row> <col>");
        }
        else {
            cmd_getfield(row, col);
        }
    }
    else if (strcasecmp(cmd, "getfields") == 0) {
        cmd_getfields();
    }
    else if (strcasecmp(cmd, "sendkey") == 0) {
        if (*arg == '\0') {
            send_error("usage: sendkey <keyname>");
        }
        else {
            cmd_sendkey(arg);
        }
    }
    else if (strcasecmp(cmd, "type") == 0) {
        if (*arg == '\0') {
            send_error("usage: type <text>");
        }
        else {
            cmd_type(arg);
        }
    }
    else if (strcasecmp(cmd, "movecursor") == 0) {
        int row, col;
        if (sscanf(arg, "%d %d", &row, &col) != 2) {
            send_error("usage: movecursor <row> <col>");
        }
        else {
            cmd_movecursor(row, col);
        }
    }
    else if (strcasecmp(cmd, "waitfor") == 0) {
        /* Parse: waitfor <text> [timeout] */
        char text[256];
        int timeout = 30;
        char *timeout_str;

        if (*arg == '\0') {
            send_error("usage: waitfor <text> [timeout]");
            return;
        }

        /* Find the last space-separated token and check if it's a number */
        strncpy(text, arg, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';

        timeout_str = strrchr(text, ' ');
        if (timeout_str != NULL) {
            char *endptr;
            long val = strtol(timeout_str + 1, &endptr, 10);
            if (*endptr == '\0' && val > 0) {
                timeout = (int)val;
                *timeout_str = '\0'; /* Remove timeout from text */
            }
        }

        cmd_waitfor(text, timeout);
    }
    else if (strcasecmp(cmd, "waitready") == 0) {
        int timeout = 30;
        if (*arg != '\0') {
            timeout = atoi(arg);
            if (timeout <= 0) timeout = 30;
        }
        cmd_waitready(timeout);
    }
    else if (strcasecmp(cmd, "quit") == 0) {
        /* Will be handled by the main loop */
    }
    else {
        send_error("unknown command");
    }
}

static void syntax(void) {
    printf("tn5250-headless - Headless 5250 terminal for automation\n"
           "Syntax:\n"
           "  tn5250-headless\n\n"
           "Reads commands from stdin, writes JSON responses to stdout.\n\n"
           "Commands:\n"
           "  connect <host[:port]>     Connect to AS/400\n"
           "  getscreen [json]          Dump screen content\n"
           "  getfield <row> <col>      Get field at position\n"
           "  getfields                 Get all fields on screen\n"
           "  sendkey <keyname>         Send key (enter, f1-f24, etc.)\n"
           "  type <text>               Type text at cursor\n"
           "  movecursor <row> <col>    Move cursor position\n"
           "  waitfor <text> [timeout]  Wait for text on screen\n"
           "  waitready [timeout]       Wait for system ready (input unlocked)\n"
           "  quit                      Disconnect and exit\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    char line[4096];

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            syntax();
        }
        else if (strcmp(argv[1], "--version") == 0) {
            printf("tn5250-headless %s\n", VERSION);
            exit(0);
        }
    }

    /* Disable stdout buffering for reliable JSON-per-line protocol */
    setvbuf(stdout, NULL, _IOLBF, 0);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip and check for quit */
        {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncasecmp(p, "quit", 4) == 0) {
                send_ok();
                break;
            }
        }

        process_command(line);
    }

    /* Cleanup */
    if (term != NULL) {
        tn5250_terminal_term(term);
    }
    if (sess != NULL) {
        tn5250_session_destroy(sess);
    }
    else if (stream != NULL) {
        tn5250_stream_destroy(stream);
    }
    if (config != NULL) {
        tn5250_config_unref(config);
    }

    return 0;
}
