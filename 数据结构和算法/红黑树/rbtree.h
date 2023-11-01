#ifndef _RBTREE_H
#define _RBTREE_H

#include <stdio.h>

#define RED     0
#define BLACK   1

#define KEY_TYPE int

typedef struct _rbtree_node
{
    KEY_TYPE key;
    void *value;

    int color;

    struct _rbtree_node *left;
    struct _rbtree_node *right;
    struct _rbtree_node *parent;
} rbtree_node ;

typedef struct _rbtree
{
    struct _rbtree_node *root;
    struct _rbtree_node *nil;
} rbtree ;

rbtree *rbtree_create();
//void rbtree_destory(rbtree *T);
void rbtree_left_rotate(rbtree *T, rbtree_node *x);
void rbtree_right_rotate(rbtree *T, rbtree_node *x);
void rbtree_insert(rbtree *T, rbtree_node *x);
void rbtree_insert_fixup(rbtree *T, rbtree_node *x);

#endif // _RBTREE_H
