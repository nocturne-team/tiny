#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "input/input.h"

#define RAWQ_LEN 32

// Ring of raw key events. When full, new events are dropped and counted.
struct rawq {
    struct raw_key ev[RAWQ_LEN];
    size_t head, len;
    uint32_t dropped;
};

void rawq_push(struct rawq *q, const struct raw_key *e);
bool rawq_pop(struct rawq *q, struct raw_key *out);
