#pragma once

#include <linux/types.h>


struct lab1_history {
    size_t length;
    struct lab1_history * next;
};

struct lab1_history * lab1_history_new(size_t length, struct lab1_history * next);
void lab1_history_delete(struct lab1_history * history);
