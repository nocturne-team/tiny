#include "input/rawq.h"

void rawq_push(struct rawq *q, const struct raw_key *e) {
    if (q->len == RAWQ_LEN) {
        q->dropped++;
        return;
    }
    q->ev[(q->head + q->len) % RAWQ_LEN] = *e;
    q->len++;
}

bool rawq_pop(struct rawq *q, struct raw_key *out) {
    if (!q->len)
        return false;
    *out = q->ev[q->head];
    q->head = (q->head + 1) % RAWQ_LEN;
    q->len--;
    return true;
}
