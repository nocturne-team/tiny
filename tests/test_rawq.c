#include "test.h"
#include "input/rawq.h"

int main(void) {
    static struct rawq q;
    struct raw_key e = { 0 }, out;
    CHECK(!rawq_pop(&q, &out));
    for (int i = 0; i < RAWQ_LEN + 3; i++) {
        e.usage = (uint8_t)i;
        rawq_push(&q, &e);
    }
    CHECK_EQ(q.dropped, 3);  // the newest events are dropped when full
    for (int i = 0; i < RAWQ_LEN; i++) {
        CHECK(rawq_pop(&q, &out));
        CHECK_EQ(out.usage, i);
    }
    CHECK(!rawq_pop(&q, &out));
    e.usage = 0x55;  // wraps around
    rawq_push(&q, &e);
    CHECK(rawq_pop(&q, &out));
    CHECK_EQ(out.usage, 0x55);
    TEST_MAIN_END();
}
