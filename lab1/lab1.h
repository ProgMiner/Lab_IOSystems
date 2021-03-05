#pragma once

#include <linux/types.h>


struct lab1_history {
    size_t length;
    struct lab1_history * next;
};

struct lab1_history * lab1_history_new(size_t length, struct lab1_history * next);
void lab1_history_delete(struct lab1_history * history);

size_t lab1_history_length(struct lab1_history * history);
size_t lab1_history_to_array(struct lab1_history * history, size_t ** dest);

size_t lab1_history_print(struct lab1_history * history, char ** dest);
