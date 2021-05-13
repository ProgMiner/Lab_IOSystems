#pragma once

#include <linux/types.h>
#include <linux/icmp.h>


struct lab3_history {
    char * iface;
    char * content;
    size_t content_length;
    struct lab3_history * next;
};

struct lab3_history * lab3_history_new(
        const char * iface,
        const char * content,
        size_t content_length,
        struct lab3_history * next
);

void lab3_history_delete(struct lab3_history * history);

size_t lab3_history_length(struct lab3_history * history);
size_t lab3_history_to_array(struct lab3_history * history, struct lab3_history ** dest);

size_t lab3_history_print(struct lab3_history * history, char ** dest);
