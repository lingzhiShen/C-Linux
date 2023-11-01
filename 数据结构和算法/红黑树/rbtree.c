#include "rbtree.h"
#include <stdlib.h>

rbtree *rbtree_create()
{
    rbtree *T = (rbtree *)malloc(sizeof(rbtree));
    T->nil = (rbtree_node *)malloc(sizeof(rbtree_node));
    T->root = T->nil;

    return T;
}

void rbtree_left_rotate(rbtree *T, rbtree_node *x)
{
    //б╘ак
    if (x == T->nil)
    {
        return ;
    }
    rbtree_node *y = x->right;

    // step 1
    x->right = y->left;
    if (y->left != T->nil)
    {
        y->left->parent = x;
    }

    // step 2
    y->parent = x->parent;
    if (x->parent == T->nil)
    {
        T->root = y;
    }
    else
    {
        if (x->parent->left == x)
        {
            x->parent->left = y;
        }
        else
        {
            x->parent->right = y;
        }
    }

    // step 3
    y->left = x;
    x->parent = y;
}

void rbtree_right_rotate(rbtree *T, rbtree_node *y) {
        //б╘ак
    if (y == T->nil)
    {
        return ;
    }
    rbtree_node *x = y->left;

    // step 1
    y->left = x->right;
    if (x->right != T->nil)
    {
        x->right->parent = y;
    }

    // step 2
    x->parent = y->parent;
    if (y->parent == T->nil)
    {
        T->root = x;
    }
    else
    {
        if (y->parent->left == y)
        {
            y->parent->left = x;
        }
        else
        {
            y->parent->right = x;
        }
    }

    // step 3
    x->right = y;
    y->parent = x;
}

void rbtree_insert(rbtree *T, rbtree_node *z)
{
    rbtree_node *x = T->root;
    rbtree_node *y = T->nil;
    while (x != T->nil)
    {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        }
        else if (z->key > x->key)
        {
            x = x->right;
        }
        else
        { //Exist
            return ;
        }
    }

    if (y == T->nil) {
        T->root = z;
        //z->color = BLACK;
    }
    else
    {
        if (z->key < y->key)
        {
            y->left = z;
        }
        else
        {
            y->right = z;
        }
        //z->color = RED;
    }

    z->parent = y;
    z->left = T->nil;
    z->right = T->nil;
    z->color = RED;

    rbtree_insert_fixup(T, z);
}

void rbtree_insert_fixup(rbtree *T, rbtree_node *z)
{
    while (z != T->root && z->parent->color == RED) {
        if (z->parent->parent->left == z->parent)
        {
            rbtree_node *y = z->parent->parent->right;
            if (y->color == RED)
            {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
            }
            else
            {
                if (z->parent->right == z)
                {
                    z = z->parent;
                    rbtree_left_rotate(T, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rbtree_right_rotate(T, z->parent->parent);
            }
        }
        else
        {
            //ср╨╒вс
            rbtree_node *y = z->parent->parent->left;
            if (y->color == RED)
            {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
            }
            else
            {
                if (z->parent->left == z)
                {
                    z = z->parent;
                    rbtree_right_rotate(T, z);
                }

                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rbtree_left_rotate(T, z->parent->parent);
            }
        }
    }
    T->root->color = BLACK;
}

void btree_node_inorder_visit(rbtree *T, rbtree_node *x)
{
    if (x == T->nil)
    {
        return ;
    }

    if (x->left != T->nil)
    {
        btree_node_inorder_visit(T, x->left);
    }

    printf(" %d ", x->key);

    if (x->right != T->nil)
    {
        btree_node_inorder_visit(T, x->right);
    }
}

void btree_inorder_travel(rbtree *T)
{
    btree_node_inorder_visit(T, T->root);
}

int main()
{
    int keys[20] = {2, 1, 3, 4, 8, 20, 11, 13, 18, 9, 7, 5, 12, 6, 15, 14, 10, 19, 17, 16};
    rbtree *T = rbtree_create();

    int i = 0;
    while (i < 20) {
        rbtree_node *z = (rbtree_node *)calloc(1, sizeof(rbtree_node));
        z->key = keys[i];
        rbtree_insert(T, z);

        i++;
    }

    btree_inorder_travel(T);

    return 0;
}
