

#include "buddy.hh"

#include "poptrie.hh"

#include <stdlib.h>
#include <string.h>

#define BUDDY_EOL 0xffffffffUL

int buddy_init(struct buddy *bs, int sz, int level, int bsz) {
    int i;
    u8 *b;
    u32 *buddy;
    void *blocks;
    u64 off;

    if (bsz < 4) {
        return -1;
    }

    buddy = (u32 *)malloc(sizeof(u32) * level);
    if (NULL == buddy) {
        return -1;
    }

    blocks = malloc(bsz * (1 << sz));
    if (NULL == blocks) {
        free(buddy);
        return -1;
    }

    b = (u8 *)malloc(((1 << (sz)) + 7) / 8);
    if (NULL == b) {
        free(blocks);
        free(buddy);
        return -1;
    }
    (void)memset(b, 0, ((1 << (sz)) + 7) / 8);

    for (i = 0; i < level; i++) {
        buddy[i] = BUDDY_EOL;
    }
    if (sz < level) {
        buddy[sz] = 0;
        *(u32 *)blocks = 0;
    } else {
        buddy[level - 1] = 0;
        for (i = 0; i < (1 << (sz - level + 1)); i++) {
            off = bsz * (i * (1 << (level - 1)));
            if (i == (1 << (sz - level + 1)) - 1) {
                *(u32 *)(blocks + off) = BUDDY_EOL;
            } else {
                *(u32 *)(blocks + off) = (u32)((i + 1) * (1 << (level - 1)));
            }
        }
    }

    bs->sz = sz;
    bs->bsz = bsz;
    bs->level = level;
    bs->buddy = buddy;
    bs->blocks = blocks;
    bs->b = b;

    return 0;
}

void buddy_release(struct buddy *bs) {
    free(bs->buddy);
    free(bs->blocks);
    free(bs->b);
}

static int _split_buddy(struct buddy *bs, int lv) {
    int ret;
    u32 next;

    if (BUDDY_EOL != bs->buddy[lv]) {
        return 0;
    }

    if (lv + 1 >= bs->level) {
        return -1;
    }

    if (BUDDY_EOL == bs->buddy[lv + 1]) {
        ret = _split_buddy(bs, lv + 1);
        if (ret < 0) {
            return ret;
        }
    }

    bs->buddy[lv] = bs->buddy[lv + 1];
    bs->buddy[lv + 1] = *(u32 *)((u64)bs->blocks + bs->bsz * bs->buddy[lv]);
    next = bs->buddy[lv] + (1 << lv);
    *(u32 *)((u64)bs->blocks + bs->bsz * next) = BUDDY_EOL;
    *(u32 *)((u64)bs->blocks + bs->bsz * bs->buddy[lv]) = next;

    return 0;
}

void *buddy_alloc(struct buddy *bs, int n) {
    int ret;

    ret = buddy_alloc2(bs, n);
    if (ret < 0) {
        return NULL;
        ;
    }

    return (void *)((u64)bs->blocks + bs->bsz * ret);
}
int buddy_alloc2(struct buddy *bs, int sz) {
    int ret;
    u32 a;
    u32 b;

    if (sz < 0) {
        return -1;
    }

    if (sz >= bs->level) {
        return -1;
    }

    ret = _split_buddy(bs, sz);
    if (ret < 0) {
        return -1;
    }

    a = bs->buddy[sz];
    b = *(u32 *)(bs->blocks + bs->bsz * a);
#if 0
    printf("ALLOC %p %x, %x [%x/%d]\n", bs, a, b, sz, bs->bsz);
#endif
    bs->buddy[sz] = b;

    bs->b[(a + (1 << sz) - 1) >> 3] |= 1 << ((a + (1 << sz) - 1) & 0x7);

    return a;
}

static void _merge(struct buddy *bs, int off, int lv) {
    int i;
    u32 s;
    u32 *n;

    if (lv + 1 >= bs->level) {
        return;
    }

    s = off / (1 << (lv + 1)) * (1 << (lv + 1));
    for (i = 0; i < (1 << (lv + 1)); i++) {
        if (bs->b[(s + i) >> 3] & (1 << ((s + i) & 7))) {
            return;
        }
    }

    n = &bs->buddy[lv];
    while (BUDDY_EOL != *n) {
        if (s == *n || (s + (1 << lv)) == *n) {
            *n = *(u32 *)(bs->blocks + bs->bsz * (*n));
        } else {
            n = (u32 *)(bs->blocks + bs->bsz * (*n));
        }
    }

    *(u32 *)(bs->blocks + bs->bsz * s) = bs->buddy[lv + 1];
    bs->buddy[lv + 1] = s;

    _merge(bs, s, lv + 1);
}

void buddy_free(struct buddy *bs, void *a) {
    int off;

    off = ((u64)a - (u64)bs->blocks) / bs->bsz;

    buddy_free2(bs, off);
}
void buddy_free2(struct buddy *bs, int a) {
    int sz;
    u32 next;
    u32 *n;

    sz = 0;
    for (;;) {
        if (bs->b[(a + (1 << sz) - 1) >> 3] & (1 << ((a + (1 << sz) - 1) & 0x7))) {
            break;
        }
        sz++;
    }

    if (sz >= bs->level) {
        return;
    }

    bs->b[(a + (1 << sz) - 1) >> 3] &= ~(1 << ((a + (1 << sz) - 1) & 0x7));

    n = &bs->buddy[sz];
    while (BUDDY_EOL != *n) {
        if ((u32)a < *n) {
            next = *n;
            *n = a;
            *(u32 *)(bs->blocks + bs->bsz * a) = next;
            n = NULL;
            break;
        } else {
            n = (u32 *)(bs->blocks + bs->bsz * (*n));
        }
    }
    if (n != NULL) {
        *n = a;
        *(u32 *)(bs->blocks + bs->bsz * a) = BUDDY_EOL;
    }

    _merge(bs, a, sz);
}
