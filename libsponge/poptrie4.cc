

#include "buddy.hh"
#include "poptrie.hh"
#include "poptrie_private.hh"

#include <stdlib.h>
#include <string.h>

#define INDEX(a, s, n) (((u64)(a) << 32 >> (64 - ((s) + (n)))) & ((1 << (n)) - 1))

#define KEYLENGTH 32

static int _route_add(struct poptrie *, struct radix_node **, u32, int, poptrie_leaf_t, int, struct radix_node *);
static int _update_subtree(struct poptrie *, struct radix_node *, u32, int);
static int
_descend_and_update(struct poptrie *, struct radix_node *, int, struct poptrie_stack *, u32, int, int, u32 *);
static int _update_inode_chunk(struct poptrie *, struct radix_node *, int, poptrie_node_t *, poptrie_leaf_t *);
static int _update_inode(struct poptrie *, struct radix_node *, int, poptrie_node_t *, poptrie_leaf_t *);
static int _update_dp1(struct poptrie *, struct radix_node *, int, u32, int, int);
static int _update_dp2(struct poptrie *, struct radix_node *, int, u32, int, int);
static void _parse_triangle(struct radix_node *, u64 *, struct radix_node *, int, int);
static void _clear_mark(struct radix_node *);
static int _route_change(struct poptrie *, struct radix_node **, u32, int, poptrie_leaf_t, int);
static int _route_update(struct poptrie *, struct radix_node **, u32, int, poptrie_leaf_t, int, struct radix_node *);
static int _route_del(struct poptrie *, struct radix_node **, u32, int, int, struct radix_node *);
static poptrie_fib_index_t _rib_lookup(struct radix_node *, u32, int, struct radix_node *);

int poptrie_route_add(struct poptrie *poptrie, u32 prefix, int len, void *nexthop) {
    int ret;
    int n;

    n = poptrie_fib_ref(poptrie, nexthop);

    ret = _route_add(poptrie, &poptrie->radix, prefix, len, n, 0, NULL);
    if (ret < 0) {
        poptrie_fib_deref(poptrie, nexthop);
        return ret;
    }

    return 0;
}

int poptrie_route_change(struct poptrie *poptrie, u32 prefix, int len, void *nexthop) {
    int n;

    n = poptrie_fib_ref(poptrie, nexthop);

    return _route_change(poptrie, &poptrie->radix, prefix, len, n, 0);
}

int poptrie_route_update(struct poptrie *poptrie, u32 prefix, int len, void *nexthop) {
    int ret;
    int n;

    n = poptrie_fib_ref(poptrie, nexthop);

    ret = _route_update(poptrie, &poptrie->radix, prefix, len, n, 0, NULL);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int poptrie_route_del(struct poptrie *poptrie, u32 prefix, int len) {
    return _route_del(poptrie, &poptrie->radix, prefix, len, 0, NULL);
}

void *poptrie_lookup(struct poptrie *poptrie, u32 addr) {
    int inode;
    int base;
    int idx;
    int pos;

    idx = INDEX(addr, 0, POPTRIE_S);
    pos = POPTRIE_S;
    base = poptrie->root;

    if (poptrie->dir[idx] & ((u32)1 << 31)) {
        return poptrie->fib.entries[poptrie->dir[idx] & (((u32)1 << 31) - 1)].entry;
    } else {
        base = poptrie->dir[idx];
        idx = INDEX(addr, pos, 6);
        pos += 6;
    }

    for (;;) {
        inode = base;
        if (VEC_BT(poptrie->nodes[inode].vector, idx)) {
            base = poptrie->nodes[inode].base1;
            idx = POPCNT_LS(poptrie->nodes[inode].vector, idx);

            base = base + (idx - 1);

            idx = INDEX(addr, pos, 6);
            pos += 6;
        } else {
            base = poptrie->nodes[inode].base0;
            idx = POPCNT_LS(poptrie->nodes[inode].leafvec, idx);
            return poptrie->fib.entries[poptrie->leaves[base + idx - 1]].entry;
        }
    }

    return 0;
}

void *poptrie_rib_lookup(struct poptrie *poptrie, u32 addr) {
    poptrie_fib_index_t idx;

    idx = _rib_lookup(poptrie->radix, addr, 0, NULL);
    return poptrie->fib.entries[idx].entry;
}

static int _update_subtree(struct poptrie *poptrie, struct radix_node *node, u32 prefix, int depth) {
    int ret;
    struct poptrie_stack stack[KEYLENGTH / 6 + 1];
    struct radix_node *ntnode;
    int idx;
    int i;
    u32 *tmpdir;
    int inode;

    stack[0].inode = -1;
    stack[0].idx = -1;
    stack[0].width = -1;

    if (depth < POPTRIE_S) {
        memcpy(poptrie->altdir, poptrie->dir, sizeof(u32) << POPTRIE_S);

        ret = _update_dp1(poptrie, poptrie->radix, 1, prefix, depth, 0);

        tmpdir = poptrie->dir;
        poptrie->dir = poptrie->altdir;
        poptrie->altdir = tmpdir;

        idx = INDEX(prefix, 0, POPTRIE_S) >> (POPTRIE_S - depth) << (POPTRIE_S - depth);

        for (i = 0; i < (1 << (POPTRIE_S - depth)); i++) {
            if (poptrie->dir[idx + i] != poptrie->altdir[idx + i]) {
                if ((poptrie->dir[idx + i] & ((u32)1 << 31)) && !(poptrie->altdir[idx + i] & ((u32)1 << 31))) {
                    _update_clean_subtree(poptrie, poptrie->altdir[idx + i]);
                    buddy_free2((buddy *)poptrie->cnodes, poptrie->altdir[idx + i]);
                } else if (!(poptrie->altdir[idx + i] & ((u32)1 << 31))) {
                    _update_clean_root(poptrie, poptrie->dir[idx + i], poptrie->altdir[idx + i]);
                }
            }
        }
    } else if (depth == POPTRIE_S) {
        ret = _update_dp1(poptrie, poptrie->radix, 0, prefix, depth, 0);
    } else {
        idx = INDEX(prefix, 0, POPTRIE_S);

        ntnode = _next_block(poptrie->radix, idx, 0, POPTRIE_S);

        if (poptrie->dir[idx] & ((u32)1 << 31)) {
            inode = -1;
        } else {
            inode = poptrie->dir[idx];
        }
        ret = _descend_and_update(poptrie, ntnode, inode, &stack[1], prefix, depth, POPTRIE_S, &poptrie->dir[idx]);
    }
    if (ret < 0) {
        return -1;
    }

    _clear_mark(node);

    return 0;
}

static int _descend_and_update(struct poptrie *poptrie,
                               struct radix_node *tnode,
                               int inode,
                               struct poptrie_stack *stack,
                               u32 prefix,
                               int len,
                               int depth,
                               u32 *root) {
    int idx;
    int p;
    int n;
    struct poptrie_node *node;
    struct radix_node *ntnode;
    int width;
    int ninode;

    if (0 == depth) {
        width = POPTRIE_S;
    } else {
        width = 6;
    }

    if (len <= depth + width) {
        return _update_part(poptrie, tnode, inode, stack, root, 0);
    } else {
        idx = INDEX(prefix, depth, width);

        if (inode < 0) {
            return _update_part(poptrie, tnode, inode, stack, root, 0);
        }

        node = poptrie->nodes + inode + NODEINDEX(idx);

        ntnode = _next_block(tnode, idx, 0, width);
        if (NULL == ntnode) {
            return _update_part(poptrie, tnode, inode, stack, root, 0);
        }

        if (VEC_BT(node->vector, BITINDEX(idx))) {
            p = POPCNT_LS(node->vector, BITINDEX(idx));
            n = (p - 1);
            ninode = node->base1 + n;
        } else {
            ninode = -1;
        }
        stack->inode = inode;
        stack->idx = idx;
        stack->width = width;
        stack++;
        return _descend_and_update(poptrie, ntnode, ninode, stack, prefix, len, depth + width, root);
    }
}

static int _update_dp1(struct poptrie *poptrie, struct radix_node *tnode, int alt, u32 prefix, int len, int depth) {
    int i;
    int idx;

    if (depth == len) {
        return _update_dp2(poptrie, tnode, alt, prefix, len, depth);
    }

    if (BT(prefix, KEYLENGTH - depth - 1)) {
        if (tnode->right) {
            return _update_dp1(poptrie, tnode->right, alt, prefix, len, depth + 1);
        } else {
            idx = INDEX(prefix, 0, POPTRIE_S) >> (POPTRIE_S - len) << (POPTRIE_S - len);
            for (i = 0; i < (1 << (POPTRIE_S - len)); i++) {
                if (alt) {
                    poptrie->altdir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                } else {
                    poptrie->dir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                    _update_clean_subtree(poptrie, poptrie->dir[idx + i]);
                    if ((int)poptrie->dir[idx + i] >= 0) {
                        buddy_free2((buddy *)poptrie->cnodes, poptrie->dir[idx + i]);
                    }
                }
            }
            return 0;
        }
    } else {
        if (tnode->left) {
            return _update_dp1(poptrie, tnode->left, alt, prefix, len, depth + 1);
        } else {
            idx = INDEX(prefix, 0, POPTRIE_S) >> (POPTRIE_S - len) << (POPTRIE_S - len);
            for (i = 0; i < (1 << (POPTRIE_S - len)); i++) {
                if (alt) {
                    poptrie->altdir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                } else {
                    poptrie->dir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                    _update_clean_subtree(poptrie, poptrie->dir[idx + i]);
                    if ((int)poptrie->dir[idx + i] >= 0) {
                        buddy_free2((buddy *)poptrie->cnodes, poptrie->dir[idx + i]);
                    }
                }
            }
            return 0;
        }
    }
}
static int _update_dp2(struct poptrie *poptrie, struct radix_node *tnode, int alt, u32 prefix, int len, int depth) {
    int i;
    int idx;
    int ret;
    struct poptrie_stack stack[KEYLENGTH / 6 + 1];

    if (depth == POPTRIE_S) {
        idx = INDEX(prefix, 0, POPTRIE_S);
        stack[0].inode = -1;
        stack[0].idx = -1;
        stack[0].width = -1;

        if (poptrie->dir[idx] & ((u32)1 << 31)) {
            if (alt) {
                ret = _update_part(poptrie, tnode, -1, &stack[1], &poptrie->altdir[idx], alt);
            } else {
                ret = _update_part(poptrie, tnode, -1, &stack[1], &poptrie->dir[idx], alt);
            }
        } else {
            if (alt) {
                ret = _update_part(poptrie, tnode, poptrie->dir[idx], &stack[1], &poptrie->altdir[idx], alt);
            } else {
                ret = _update_part(poptrie, tnode, poptrie->dir[idx], &stack[1], &poptrie->dir[idx], alt);
            }
        }
        return ret;
    }

    if (tnode->left) {
        _update_dp2(poptrie, tnode->left, alt, prefix, len, depth + 1);
    } else {
        idx = INDEX(prefix, 0, POPTRIE_S) >> (POPTRIE_S - depth) << (POPTRIE_S - depth);
        for (i = 0; i < (1 << (POPTRIE_S - depth - 1)); i++) {
            if (alt) {
                poptrie->altdir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
            } else {
                poptrie->dir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                _update_clean_subtree(poptrie, poptrie->dir[idx + i]);
                if ((int)poptrie->dir[idx + i] >= 0) {
                    buddy_free2((buddy *)poptrie->cnodes, poptrie->dir[idx + i]);
                }
            }
        }
    }
    if (tnode->right) {
        prefix |= 1 << (KEYLENGTH - depth - 1);
        return _update_dp2(poptrie, tnode->right, alt, prefix, len, depth + 1);
    } else {
        idx = INDEX(prefix, 0, POPTRIE_S) >> (POPTRIE_S - depth) << (POPTRIE_S - depth);
        idx += 1 << (POPTRIE_S - depth - 1);
        for (i = 0; i < (1 << (POPTRIE_S - depth - 1)); i++) {
            if (alt) {
                poptrie->altdir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
            } else {
                poptrie->dir[idx + i] = ((u32)1 << 31) | EXT_NH(tnode);
                _update_clean_subtree(poptrie, poptrie->dir[idx + i]);
                if ((int)poptrie->dir[idx + i] >= 0) {
                    buddy_free2((buddy *)poptrie->cnodes, poptrie->dir[idx + i]);
                }
            }
        }
    }

    return 0;
}

static int _route_add(struct poptrie *poptrie,
                      struct radix_node **node,
                      u32 prefix,
                      int len,
                      poptrie_leaf_t nexthop,
                      int depth,
                      struct radix_node *ext) {
    if (NULL == *node) {
        *node = (radix_node *)malloc(sizeof(struct radix_node));
        if (NULL == *node) {
            return -1;
        }
        (*node)->valid = 0;
        (*node)->left = NULL;
        (*node)->right = NULL;
        (*node)->ext = ext;
        (*node)->mark = 0;
    }

    if (len == depth) {
        if ((*node)->valid) {
            return -1;
        }
        (*node)->valid = 1;
        (*node)->nexthop = nexthop;
        (*node)->len = len;

        (*node)->mark = poptrie_route_add_propagate(*node, *node);

        return _update_subtree(poptrie, *node, prefix, depth);
    } else {
        if ((*node)->valid) {
            ext = *node;
        }
        if (BT(prefix, KEYLENGTH - depth - 1)) {
            return _route_add(poptrie, &((*node)->right), prefix, len, nexthop, depth + 1, ext);
        } else {
            return _route_add(poptrie, &((*node)->left), prefix, len, nexthop, depth + 1, ext);
        }
    }
}

static int _route_change(struct poptrie *poptrie,
                         struct radix_node **node,
                         u32 prefix,
                         int len,
                         poptrie_leaf_t nexthop,
                         int depth) {
    int ret;
    int n;

    if (NULL == *node) {
        return -1;
    }

    if (len == depth) {
        if (!(*node)->valid) {
            return -1;
        }

        if ((*node)->nexthop != nexthop) {
            n = (*node)->nexthop;
            (*node)->nexthop = nexthop;
            (*node)->mark = poptrie_route_change_propagate(*node, *node);

            ret = _update_subtree(poptrie, *node, prefix, depth);

            poptrie->fib.entries[n].refs--;

            return ret;
        } else {
            n = nexthop;

            poptrie->fib.entries[n].refs--;

            return 0;
        }
    } else {
        if (BT(prefix, KEYLENGTH - depth - 1)) {
            return _route_change(poptrie, &((*node)->right), prefix, len, nexthop, depth + 1);
        } else {
            return _route_change(poptrie, &((*node)->left), prefix, len, nexthop, depth + 1);
        }
    }
}

static int _route_update(struct poptrie *poptrie,
                         struct radix_node **node,
                         u32 prefix,
                         int len,
                         poptrie_leaf_t nexthop,
                         int depth,
                         struct radix_node *ext) {
    int ret;
    int n;

    if (NULL == *node) {
        *node = (radix_node *)malloc(sizeof(struct radix_node));
        if (NULL == *node) {
            return -1;
        }
        (*node)->valid = 0;
        (*node)->left = NULL;
        (*node)->right = NULL;
        (*node)->ext = ext;
        (*node)->mark = 0;
    }

    if (len == depth) {
        if ((*node)->valid) {
            if ((*node)->nexthop != nexthop) {
                n = (*node)->nexthop;
                (*node)->nexthop = nexthop;
                (*node)->mark = poptrie_route_change_propagate(*node, *node);

                ret = _update_subtree(poptrie, *node, prefix, depth);

                poptrie->fib.entries[n].refs--;

                return ret;
            } else {
                n = nexthop;

                poptrie->fib.entries[n].refs--;

                return 0;
            }
        } else {
            (*node)->valid = 1;
            (*node)->nexthop = nexthop;
            (*node)->len = len;

            (*node)->mark = poptrie_route_add_propagate(*node, *node);

            return _update_subtree(poptrie, *node, prefix, depth);
        }
    } else {
        if ((*node)->valid) {
            ext = *node;
        }
        if (BT(prefix, KEYLENGTH - depth - 1)) {
            return _route_update(poptrie, &((*node)->right), prefix, len, nexthop, depth + 1, ext);
        } else {
            return _route_update(poptrie, &((*node)->left), prefix, len, nexthop, depth + 1, ext);
        }
    }
}

static int _route_del(struct poptrie *poptrie,
                      struct radix_node **node,
                      u32 prefix,
                      int len,
                      int depth,
                      struct radix_node *ext) {
    int ret;
    int n;

    if (NULL == *node) {
        return -1;
    }

    if (len == depth) {
        if (!(*node)->valid) {
            return -1;
        }

        (*node)->mark = poptrie_route_del_propagate(*node, *node, ext);

        n = (*node)->nexthop;
        (*node)->valid = 0;
        (*node)->nexthop = 0;

        ret = _update_subtree(poptrie, *node, prefix, depth);
        if (ret < 0) {
            return -1;
        }

        poptrie->fib.entries[n].refs--;

        return 0;
    } else {
        if ((*node)->valid) {
            ext = *node;
        }

        if (BT(prefix, KEYLENGTH - depth - 1)) {
            ret = _route_del(poptrie, &((*node)->right), prefix, len, depth + 1, ext);
        } else {
            ret = _route_del(poptrie, &((*node)->left), prefix, len, depth + 1, ext);
        }
        if (ret < 0) {
            return ret;
        }

        if (NULL == (*node)->left && NULL == (*node)->right) {
            free(*node);
            *node = NULL;
        }
        return ret;
    }

    return -1;
}

static poptrie_fib_index_t _rib_lookup(struct radix_node *node, u32 addr, int depth, struct radix_node *en) {
    if (NULL == node) {
        return 0;
    }
    if (node->valid) {
        en = node;
    }

    if (BT(addr, KEYLENGTH - depth - 1)) {
        if (NULL == node->right) {
            if (NULL != en) {
                return en->nexthop;
            } else {
                return 0;
            }
        } else {
            return _rib_lookup(node->right, addr, depth + 1, en);
        }
    } else {
        if (NULL == node->left) {
            if (NULL != en) {
                return en->nexthop;
            } else {
                return 0;
            }
        } else {
            return _rib_lookup(node->left, addr, depth + 1, en);
        }
    }
}
