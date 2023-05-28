

#ifndef _POPTRIE_H
#define _POPTRIE_H
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpointer-arith"
#endif
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define POPTRIE_S 18

#define POPTRIE_INIT_FIB_SIZE 4096

#define popcnt(v) __builtin_popcountll(v)

typedef struct poptrie_node {
    u64 leafvec;

    u64 vector;

    u32 base0;

    u32 base1;
} poptrie_node_t;

typedef u16 poptrie_leaf_t;

typedef u16 poptrie_fib_index_t;

struct radix_node {
    int valid;
    struct radix_node *left;
    struct radix_node *right;

    int len;
    poptrie_leaf_t nexthop;

    struct radix_node *ext;

    int mark;
};

struct poptrie_fib_entry {
    void *entry;
    int refs;
};
struct poptrie_fib {
    struct poptrie_fib_entry *entries;
    int sz;
};

struct poptrie {
    u32 root;

    struct poptrie_fib fib;

    poptrie_node_t *nodes;
    poptrie_leaf_t *leaves;
    void *cnodes;
    void *cleaves;

    int nodesz;
    int leafsz;

    u32 *dir;
    u32 *altdir;

    struct radix_node *radix;

    int _allocated;
};

#ifdef __cplusplus
extern "C" {
#endif

struct poptrie *poptrie_init(struct poptrie *, int, int);
void poptrie_release(struct poptrie *);
int poptrie_route_add(struct poptrie *, u32, int, void *);
int poptrie_route_change(struct poptrie *, u32, int, void *);
int poptrie_route_update(struct poptrie *, u32, int, void *);
int poptrie_route_del(struct poptrie *, u32, int);
void *poptrie_lookup(struct poptrie *, u32);
void *poptrie_rib_lookup(struct poptrie *, u32);

int poptrie6_route_add(struct poptrie *, __uint128_t, int, void *);
int poptrie6_route_change(struct poptrie *, __uint128_t, int, void *);
int poptrie6_route_update(struct poptrie *, __uint128_t, int, void *);
int poptrie6_route_del(struct poptrie *, __uint128_t, int);
void *poptrie6_lookup(struct poptrie *, __uint128_t);
void *poptrie6_rib_lookup(struct poptrie *, __uint128_t);

#ifdef __cplusplus
}
#endif

#endif
