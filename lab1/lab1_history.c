#include "lab1.h"

#include <linux/slab.h>


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
