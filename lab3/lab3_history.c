#include "lab3.h"

#include <linux/slab.h>

#define SNPRINTF_BUFFER_LENGTH (1024)


struct lab3_history * lab3_history_new(
        const char * iface,
        const char * content,
        size_t content_length,
        struct lab3_history * next
) {
    struct lab3_history * new = kmalloc(sizeof(struct lab3_history), GFP_KERNEL);

    size_t iface_length = strlen(iface);
    char * iface_copy = kmalloc(sizeof(char) * (iface_length + 1), GFP_KERNEL);
    char * content_copy = kmalloc(sizeof(char) * content_length, GFP_KERNEL);

    strncpy(iface_copy, iface, iface_length + 1);
    new->iface = iface_copy;

    memcpy(content_copy, content, content_length);
    new->content = content_copy;

    new->content_length = content_length;
    new->next = next;
    return new;
}

void lab3_history_delete(struct lab3_history * history) {
    struct lab3_history * cur, * next = history;

    while (next) {
        cur = next;
        next = cur->next;
        kfree(cur->iface);
        kfree(cur->content);
        kfree(cur);
    }
}

size_t lab3_history_length(struct lab3_history * history) {
    size_t length = 0;

    for (; history; history = history->next) {
        ++length;
    }

    return length;
}

size_t lab3_history_to_array(struct lab3_history * history, struct lab3_history ** dest) {
    size_t length = lab3_history_length(history);

    struct lab3_history * array = kmalloc(sizeof(struct lab3_history) * length, GFP_KERNEL);
    size_t i;

    for (i = 1; i <= length && history; ++i, history = history->next) {
        array[length - i] = *history;
    }

    *dest = array;
    return length;
}

static char * lab3_history_print_content(struct lab3_history item) {
    char * buf = kmalloc(sizeof(char) * (item.content_length + 1), GFP_KERNEL);
    size_t i;
    char c;

    for (i = 0; i < item.content_length; ++i) {
        c = item.content[i];

        if (c < 0x20 || c > 0x7E) {
            buf[i] = '.';
        } else {
            buf[i] = c;
        }
    }

    buf[i] = '\0';
    return buf;
}

static char * lab3_history_print_content_hex(struct lab3_history item) {
    char * buf = kmalloc(sizeof(char) * (item.content_length * 3), GFP_KERNEL);
    size_t i;
    char c;

    for (i = 0; i < item.content_length; ++i) {
        c = item.content[i];

        snprintf(buf + i * 3, 3, "%02hhX", (unsigned char) c);
        buf[i * 3 + 2] = ' ';
    }

    buf[i * 3 - 1] = '\0';
    return buf;
}

size_t lab3_history_print(struct lab3_history * history, char ** dest) {
    size_t len = 0, sum = 0, i;
    char * content_buf;
    char * content_hex;

    struct lab3_history * history_array;
    size_t history_length = lab3_history_to_array(history, &history_array);
    char * buf = kmalloc_array(
            history_length + 1,
            sizeof(char) * SNPRINTF_BUFFER_LENGTH * (history_length + 1),
            GFP_KERNEL
    );

    for (i = 0; i < history_length; ++i) {
        content_hex = lab3_history_print_content_hex(history_array[i]);
        content_buf = lab3_history_print_content(history_array[i]);
        sum += history_array[i].content_length;

        len += snprintf(
                buf + len,
                sizeof(char) * SNPRINTF_BUFFER_LENGTH,
                "%lu. %s: %lu bytes:\nBytes: \"%s\"\nHex: %s\n",
                i + 1,
                history_array[i].iface,
                history_array[i].content_length,
                content_buf,
                content_hex
        );

        kfree(content_buf);
        kfree(content_hex);
    }

    kfree(history_array);
    len += snprintf(buf + len, sizeof(char) * SNPRINTF_BUFFER_LENGTH, "Summary: %lu bytes.\n", sum);

    *dest = buf;
    return len;
}
