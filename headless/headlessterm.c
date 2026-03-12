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
#define _TN5250_TERMINAL_PRIVATE_DEFINED
#include "tn5250-private.h"
#include "headlessterm.h"

#define HEADLESS_KEYQ_SIZE 256

struct _Tn5250TerminalPrivate {
    int width;
    int height;
    int quit_flag;

    /* Ring buffer for injected keys */
    int key_queue[HEADLESS_KEYQ_SIZE];
    int key_queue_head;
    int key_queue_tail;
};

static void headless_terminal_init(Tn5250Terminal *This);
static void headless_terminal_term(Tn5250Terminal *This);
static void headless_terminal_destroy(Tn5250Terminal *This);
static int headless_terminal_width(Tn5250Terminal *This);
static int headless_terminal_height(Tn5250Terminal *This);
static int headless_terminal_flags(Tn5250Terminal *This);
static void headless_terminal_update(Tn5250Terminal *This,
                                     Tn5250Display *display);
static void headless_terminal_update_indicators(Tn5250Terminal *This,
                                                Tn5250Display *display);
static int headless_terminal_waitevent(Tn5250Terminal *This);
static int headless_terminal_getkey(Tn5250Terminal *This);
static void headless_terminal_beep(Tn5250Terminal *This);
static int headless_terminal_enhanced(Tn5250Terminal *This);
static int headless_terminal_config(Tn5250Terminal *This, Tn5250Config *config);

static int headless_key_queue_empty(Tn5250Terminal *This) {
    return This->data->key_queue_head == This->data->key_queue_tail;
}

static void headless_key_queue_push(Tn5250Terminal *This, int key) {
    int next = (This->data->key_queue_tail + 1) % HEADLESS_KEYQ_SIZE;
    if (next == This->data->key_queue_head) {
        return; /* Queue full, drop key */
    }
    This->data->key_queue[This->data->key_queue_tail] = key;
    This->data->key_queue_tail = next;
}

static int headless_key_queue_pop(Tn5250Terminal *This) {
    int key;
    if (headless_key_queue_empty(This)) {
        return -1;
    }
    key = This->data->key_queue[This->data->key_queue_head];
    This->data->key_queue_head =
        (This->data->key_queue_head + 1) % HEADLESS_KEYQ_SIZE;
    return key;
}

Tn5250Terminal *tn5250_headless_terminal_new(void) {
    Tn5250Terminal *r = tn5250_new(Tn5250Terminal, 1);
    if (r == NULL) return NULL;

    r->data = tn5250_new(struct _Tn5250TerminalPrivate, 1);
    if (r->data == NULL) {
        free(r);
        return NULL;
    }

    r->data->width = 80;
    r->data->height = 24;
    r->data->quit_flag = 0;
    r->data->key_queue_head = 0;
    r->data->key_queue_tail = 0;

    r->conn_fd = -1;
    r->init = headless_terminal_init;
    r->term = headless_terminal_term;
    r->destroy = headless_terminal_destroy;
    r->width = headless_terminal_width;
    r->height = headless_terminal_height;
    r->flags = headless_terminal_flags;
    r->update = headless_terminal_update;
    r->update_indicators = headless_terminal_update_indicators;
    r->waitevent = headless_terminal_waitevent;
    r->getkey = headless_terminal_getkey;
    r->putkey = NULL;
    r->beep = headless_terminal_beep;
    r->enhanced = headless_terminal_enhanced;
    r->config = headless_terminal_config;
    r->create_window = NULL;
    r->destroy_window = NULL;
    r->create_scrollbar = NULL;
    r->destroy_scrollbar = NULL;
    r->create_menubar = NULL;
    r->destroy_menubar = NULL;
    r->create_menuitem = NULL;
    r->destroy_menuitem = NULL;
    return r;
}

void tn5250_headless_terminal_push_key(Tn5250Terminal *term, int key) {
    headless_key_queue_push(term, key);
}

static void headless_terminal_init(Tn5250Terminal *This) {
    /* No-op: no screen to initialize */
}

static void headless_terminal_term(Tn5250Terminal *This) {
    /* No-op: no screen to tear down */
}

static void headless_terminal_destroy(Tn5250Terminal *This) {
    if (This->data != NULL) {
        free(This->data);
    }
    free(This);
}

static int headless_terminal_width(Tn5250Terminal *This) {
    return This->data->width;
}

static int headless_terminal_height(Tn5250Terminal *This) {
    return This->data->height;
}

static int headless_terminal_flags(Tn5250Terminal *This) {
    return 0;
}

static void headless_terminal_update(Tn5250Terminal *This,
                                     Tn5250Display *display) {
    /* No-op: no screen to draw */
}

static void headless_terminal_update_indicators(Tn5250Terminal *This,
                                                Tn5250Display *display) {
    /* No-op: no indicator line to draw */
}

static int headless_terminal_waitevent(Tn5250Terminal *This) {
    fd_set fdr;
    struct timeval tv;
    int result = 0;
    int sm;

    if (This->data->quit_flag) return TN5250_TERMINAL_EVENT_QUIT;

    /* If we have queued keys, return immediately */
    if (!headless_key_queue_empty(This)) {
        result |= TN5250_TERMINAL_EVENT_KEY;
    }

    FD_ZERO(&fdr);
    sm = 0;

    if (This->conn_fd >= 0) {
        FD_SET(This->conn_fd, &fdr);
        sm = This->conn_fd + 1;
    }

    /* If we already have keys, do a non-blocking check for data;
     * otherwise block with a short timeout to allow key injection. */
    if (result != 0) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    }
    else {
        tv.tv_sec = 0;
        tv.tv_usec = 50000; /* 50ms poll interval */
    }

    if (sm > 0) {
        select(sm, &fdr, NULL, NULL, &tv);
        if (This->conn_fd >= 0 && FD_ISSET(This->conn_fd, &fdr)) {
            result |= TN5250_TERMINAL_EVENT_DATA;
        }
    }
    else if (result == 0) {
        /* No connection and no keys: sleep briefly to avoid busy-wait */
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        select(0, NULL, NULL, NULL, &tv);
    }

    /* Re-check key queue after select (keys may have been pushed) */
    if (!headless_key_queue_empty(This)) {
        result |= TN5250_TERMINAL_EVENT_KEY;
    }

    return result;
}

static int headless_terminal_getkey(Tn5250Terminal *This) {
    return headless_key_queue_pop(This);
}

static void headless_terminal_beep(Tn5250Terminal *This) {
    /* No-op */
}

static int headless_terminal_enhanced(Tn5250Terminal *This) {
    return 0;
}

static int headless_terminal_config(Tn5250Terminal *This,
                                    Tn5250Config *config) {
    return 0;
}
