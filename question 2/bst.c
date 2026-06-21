#include "bst.h"
#include <stdlib.h>
#include <string.h>

static char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

Node *bst_insert(Node *root, const char *key) {
    if (!key) return root;
    if (!root) {
        Node *n = malloc(sizeof(Node));
        if (!n) return NULL;
        n->key = my_strdup(key);
        n->left = n->right = NULL;
        return n;
    }
    int cmp = strcmp(key, root->key);
    if (cmp < 0) root->left = bst_insert(root->left, key);
    else if (cmp > 0) root->right = bst_insert(root->right, key);
    return root;
}

Node *bst_find(Node *root, const char *key) {
    if (!root || !key) return NULL;
    int cmp = strcmp(key, root->key);
    if (cmp == 0) return root;
    if (cmp < 0) return bst_find(root->left, key);
    return bst_find(root->right, key);
}

void bst_traverse(Node *root, void (*fn)(const char *key, void *ctx), void *ctx) {
    if (!root) return;
    bst_traverse(root->left, fn, ctx);
    fn(root->key, ctx);
    bst_traverse(root->right, fn, ctx);
}

void bst_free(Node *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root->key);
    free(root);
}
