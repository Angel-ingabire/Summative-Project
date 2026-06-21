#ifndef BST_H
#define BST_H

#include <stddef.h>

typedef struct Node
{
    char *key;
    struct Node *left;
    struct Node *right;
} Node;

Node *bst_insert(Node *root, const char *key);
Node *bst_find(Node *root, const char *key);
void bst_free(Node *root);
void bst_traverse(Node *root, void (*fn)(const char *key, void *ctx), void *ctx);

#endif // BST_H
