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
