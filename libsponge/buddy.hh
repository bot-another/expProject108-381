

#include "poptrie.hh"

#include <stdlib.h>

#ifndef _POPTRIE_BUDDY_H
#define _POPTRIE_BUDDY_H
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpointer-arith"
#endif

struct buddy {
    int sz;

    int bsz;

    u8 *b;

    void *blocks;

    int level;

    u32 *buddy;
};

#ifdef __cplusplus
extern "C" {
#endif

int buddy_init(struct buddy *, int, int, int);
void buddy_release(struct buddy *);
void *buddy_alloc(struct buddy *, int);
int buddy_alloc2(struct buddy *, int);
void buddy_free(struct buddy *, void *);
void buddy_free2(struct buddy *, int);

#ifdef __cplusplus
}
#endif

#endif
