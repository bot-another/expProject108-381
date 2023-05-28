

#include "poptrie.hh"

#include "buddy.hh"

#include <stdlib.h>
#include <string.h>

#define INDEX(a, s, n) (((u64)(a) << 32 >> (64 - ((s) + (n)))) & ((1 << (n)) - 1))

#define KEYLENGTH 32

static void _release_radix(struct radix_node *);

struct poptrie *poptrie_init(struct poptrie *poptrie, int sz1, int sz0) {
    int ret;
    int i;

    if (NULL == poptrie) {
        poptrie = (struct poptrie *)malloc(sizeof(struct poptrie));
        if (NULL == poptrie) {
            return NULL;
        }
        (void)memset(poptrie, 0, sizeof(struct poptrie));

        poptrie->_allocated = 1;
    } else {
        (void)memset(poptrie, 0, sizeof(struct poptrie));
    }

    poptrie->nodes = (poptrie_node_t *)malloc(sizeof(poptrie_node_t) * (1 << sz1));
    if (NULL == poptrie->nodes) {
        poptrie_release(poptrie);
        return NULL;
    }
    poptrie->leaves = (poptrie_leaf_t *)malloc(sizeof(poptrie_leaf_t) * (1 << sz0));
    if (NULL == poptrie->leaves) {
        poptrie_release(poptrie);
        return NULL;
    }

    poptrie->cnodes = malloc(sizeof(struct buddy));
    if (NULL == poptrie->cnodes) {
        poptrie_release(poptrie);
        return NULL;
    }
    ret = buddy_init((buddy *)poptrie->cnodes, sz1, sz1, sizeof(u32));
    if (ret < 0) {
        free(poptrie->cnodes);
        poptrie->cnodes = NULL;
        poptrie_release(poptrie);
        return NULL;
    }

    poptrie->cleaves = malloc(sizeof(struct buddy));
    if (NULL == poptrie->cleaves) {
        poptrie_release(poptrie);
        return NULL;
    }
    ret = buddy_init((buddy *)poptrie->cleaves, sz0, sz0, sizeof(u32));
    if (ret < 0) {
        free(poptrie->cnodes);
        poptrie->cnodes = NULL;
        poptrie_release(poptrie);
        return NULL;
    }

    poptrie->dir = (u32 *)malloc(sizeof(u32) << POPTRIE_S);
    if (NULL == poptrie->dir) {
        poptrie_release(poptrie);
        return NULL;
    }
    for (i = 0; i < (1 << POPTRIE_S); i++) {
        poptrie->dir[i] = (u32)1 << 31;
    }

    poptrie->altdir = (u32 *)malloc(sizeof(u32) << POPTRIE_S);
    if (NULL == poptrie->altdir) {
        poptrie_release(poptrie);
        return NULL;
    }

    poptrie->fib.entries = (poptrie_fib_entry *)malloc(sizeof(struct poptrie_fib_entry) * POPTRIE_INIT_FIB_SIZE);
    if (NULL == poptrie->fib.entries) {
        poptrie_release(poptrie);
        return NULL;
    }
    memset(poptrie->fib.entries, 0, sizeof(struct poptrie_fib_entry) * POPTRIE_INIT_FIB_SIZE);
    poptrie->fib.sz = POPTRIE_INIT_FIB_SIZE;

    poptrie->fib.entries[0].entry = NULL;
    poptrie->fib.entries[0].refs = 1;

    return poptrie;
}

void poptrie_release(struct poptrie *poptrie) {
    _release_radix(poptrie->radix);

    if (poptrie->nodes) {
        free(poptrie->nodes);
    }
    if (poptrie->leaves) {
        free(poptrie->leaves);
    }
    if (poptrie->cnodes) {
        buddy_release((buddy *)poptrie->cnodes);
        free(poptrie->cnodes);
    }
    if (poptrie->cleaves) {
        buddy_release((buddy *)poptrie->cleaves);
        free(poptrie->cleaves);
    }
    if (poptrie->dir) {
        free(poptrie->dir);
    }
    if (poptrie->altdir) {
        free(poptrie->altdir);
    }
    if (poptrie->fib.entries) {
        free(poptrie->fib.entries);
    }
    if (poptrie->_allocated) {
        free(poptrie);
    }
}

static void _release_radix(struct radix_node *node) {
    if (NULL != node) {
        _release_radix(node->left);
        _release_radix(node->right);
        free(node);
    }
}
