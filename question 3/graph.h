#ifndef GRAPH_H
#define GRAPH_H

#include <stddef.h>

typedef struct Graph Graph;

Graph *graph_create(void);
void graph_free(Graph *g);

int graph_add_airport(Graph *g, const char *code); /* returns 0 on success, -1 on error */
int graph_remove_airport(Graph *g, const char *code); /* 0 success, -1 if not found */
int graph_add_route(Graph *g, const char *from, const char *to); /* 0 success */
int graph_remove_route(Graph *g, const char *from, const char *to);

int graph_index(Graph *g, const char *code); /* returns index or -1 */

void graph_list_airports(Graph *g);
void graph_list_outgoing(Graph *g, const char *code);
void graph_list_incoming(Graph *g, const char *code);
void graph_print_matrix(Graph *g);

#endif // GRAPH_H
