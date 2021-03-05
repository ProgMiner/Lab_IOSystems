#include "lab1.h"

#include <linux/slab.h>

#define SNPRINTF_BUFFER_LENGTH (256)


struct lab1_history * lab1_history_new(
        size_t length,
        struct lab1_history * next
) {
    struct lab1_history * new = kmalloc(sizeof(struct lab1_history), GFP_KERNEL);

    new->length = length;
    new->next = next;

    return new;
}

void lab1_history_delete(struct lab1_history * history) {
    struct lab1_history * cur, * next = history;

    while (next) {
        cur = next;
        next = cur->next;
        kfree(cur);
    }
}

size_t lab1_history_length(struct lab1_history * history) {
    size_t length = 0;

    for (; history; history = history->next) {
        ++length;
    }

    return length;
}

size_t lab1_history_to_array(struct lab1_history * history, size_t ** dest) {
    size_t length = lab1_history_length(history);

    size_t * array = kmalloc(sizeof(size_t) * length, GFP_KERNEL);
    size_t i;

    for (i = 1; i <= length && history; ++i, history = history->next) {
        array[length - i] = history->length;
    }

    *dest = array;
    return length;
}

size_t lab1_history_print(struct lab1_history * history, char ** dest) {
    size_t len = 0, sum = 0, i;
    size_t * history_array;

    size_t history_length = lab1_history_to_array(history, &history_array);
    char * buf = kmalloc_array(
            history_length + 1,
            sizeof(char) * SNPRINTF_BUFFER_LENGTH,
            GFP_KERNEL
    );

    for (i = 0; i < history_length; ++i) {
        sum += history_array[i];

        len += snprintf(
                buf + len,
                sizeof(char) * SNPRINTF_BUFFER_LENGTH,
                "%lu. %lu bytes\n",
                i + 1,
                history_array[i]
        );
    }

    kfree(history_array);
    len += snprintf(buf + len, sizeof(char) * SNPRINTF_BUFFER_LENGTH, "Summary: %lu bytes\n", sum);

    *dest = buf;
    return len;
}
