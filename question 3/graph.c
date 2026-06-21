#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Graph {
    char **names; /* array of airport codes */
    int *adj;    /* flat adjacency matrix (n x n) row-major */
    size_t n;
    size_t cap;
} Graph;

static char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t l = strlen(s) + 1;
    char *p = malloc(l);
    if (p) memcpy(p, s, l);
    return p;
}

Graph *graph_create(void) {
    Graph *g = malloc(sizeof(Graph));
    if (!g) return NULL;
    g->names = NULL;
    g->adj = NULL;
    g->n = 0;
    g->cap = 0;
    return g;
}

static int resize_matrix(Graph *g, size_t newcap) {
    size_t newsize = newcap * newcap;
    int *newadj = calloc(newsize, sizeof(int));
    if (!newadj) return -1;
    if (g->adj) {
        for (size_t i = 0; i < g->n; ++i) {
            for (size_t j = 0; j < g->n; ++j) {
                newadj[i*newcap + j] = g->adj[i*g->cap + j];
            }
        }
        free(g->adj);
    }
    g->adj = newadj;
    g->cap = newcap;
    return 0;
}

int graph_add_airport(Graph *g, const char *code) {
    if (!g || !code) return -1;
    if (graph_index(g, code) != -1) return -1; /* exists */
    if (g->n + 1 > g->cap) {
        size_t newcap = g->cap == 0 ? 4 : g->cap * 2;
        if (resize_matrix(g, newcap) != 0) return -1;
        char **newnames = realloc(g->names, newcap * sizeof(char*));
        if (!newnames) return -1;
        g->names = newnames;
    }
    g->names[g->n] = my_strdup(code);
    if (!g->names[g->n]) return -1;
    /* ensure new row/col zeroed (resize_matrix already zeroed extras) */
    g->n += 1;
    return 0;
}

int graph_index(Graph *g, const char *code) {
    if (!g || !code) return -1;
    for (size_t i = 0; i < g->n; ++i) {
        if (strcmp(g->names[i], code) == 0) return (int)i;
    }
    return -1;
}

int graph_remove_airport(Graph *g, const char *code) {
    if (!g || !code) return -1;
    int idx = graph_index(g, code);
    if (idx < 0) return -1;
    /* free the name */
    free(g->names[idx]);
    /* shift names left */
    for (size_t i = idx; i + 1 < g->n; ++i) g->names[i] = g->names[i+1];
    g->names[g->n-1] = NULL;

    /* create new smaller matrix and copy excluding row/col idx */
    size_t newn = g->n - 1;
    int *newadj = NULL;
    if (newn > 0) {
        newadj = calloc(newn * newn, sizeof(int));
        if (!newadj) return -1;
        for (size_t i = 0, ii = 0; i < g->n; ++i) {
            if (i == (size_t)idx) continue;
            for (size_t j = 0, jj = 0; j < g->n; ++j) {
                if (j == (size_t)idx) continue;
                newadj[ii*newn + jj] = g->adj[i*g->cap + j];
                jj++;
            }
            ii++;
        }
    }
    free(g->adj);
    g->adj = newadj;
    g->n = newn;
    /* update capacity to match new allocation */
    if (newn == 0) g->cap = 0;
    else g->cap = newn;
    return 0;
}

int graph_add_route(Graph *g, const char *from, const char *to) {
    if (!g || !from || !to) return -1;
    int i = graph_index(g, from);
    int j = graph_index(g, to);
    if (i < 0 || j < 0) return -1;
    g->adj[i*g->cap + j] = 1;
    return 0;
}

int graph_remove_route(Graph *g, const char *from, const char *to) {
    if (!g || !from || !to) return -1;
    int i = graph_index(g, from);
    int j = graph_index(g, to);
    if (i < 0 || j < 0) return -1;
    g->adj[i*g->cap + j] = 0;
    return 0;
}

void graph_list_airports(Graph *g) {
    if (!g) return;
    for (size_t i = 0; i < g->n; ++i) printf("%s\n", g->names[i]);
}

void graph_list_outgoing(Graph *g, const char *code) {
    if (!g || !code) return;
    int idx = graph_index(g, code);
    if (idx < 0) { printf("Airport not found: %s\n", code); return; }
    int found = 0;
    for (size_t j = 0; j < g->n; ++j) {
        if (g->adj[idx*g->cap + j]) { printf("%s\n", g->names[j]); found = 1; }
    }
    if (!found) printf("(none)\n");
}

void graph_list_incoming(Graph *g, const char *code) {
    if (!g || !code) return;
    int idx = graph_index(g, code);
    if (idx < 0) { printf("Airport not found: %s\n", code); return; }
    int found = 0;
    for (size_t i = 0; i < g->n; ++i) {
        if (g->adj[i*g->cap + idx]) { printf("%s\n", g->names[i]); found = 1; }
    }
    if (!found) printf("(none)\n");
}

void graph_print_matrix(Graph *g) {
    if (!g) return;
    printf("    ");
    for (size_t j = 0; j < g->n; ++j) printf("%5s", g->names[j]);
    printf("\n");
    for (size_t i = 0; i < g->n; ++i) {
        printf("%4s", g->names[i]);
        for (size_t j = 0; j < g->n; ++j) {
            printf("%5d", g->adj[i*g->cap + j]);
        }
        printf("\n");
    }
}

void graph_free(Graph *g) {
    if (!g) return;
    if (g->names) {
        for (size_t i = 0; i < g->n; ++i) free(g->names[i]);
        free(g->names);
    }
    free(g->adj);
    free(g);
}
