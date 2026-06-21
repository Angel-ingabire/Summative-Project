#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

#define MAX_TOKEN 64

static void prompt(void) { printf("> "); }

int main(int argc, char **argv) {
    Graph *g = graph_create();
    if (!g) return 1;

    char line[256];
    printf("Airline Route Relationship Analyzer\nType 'help' for commands.\n");
    while (1) {
        prompt();
        if (!fgets(line, sizeof(line), stdin)) break;
        char *p = line;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == '\0') continue;
        /* tokenize */
        char *cmd = strtok(p, " \t\n\r");
        if (!cmd) continue;
        if (strcmp(cmd, "help") == 0) {
            printf("Commands:\n");
            printf("  add_airport CODE\n  remove_airport CODE\n");
            printf("  add_route FROM TO\n  remove_route FROM TO\n");
            printf("  outgoing CODE\n  incoming CODE\n");
            printf("  matrix\n  list\n  exit\n");
            continue;
        }
        if (strcmp(cmd, "add_airport") == 0) {
            char *code = strtok(NULL, " \t\n\r");
            if (!code) { printf("Usage: add_airport CODE\n"); continue; }
            if (graph_add_airport(g, code) == 0) printf("Added %s\n", code);
            else printf("Failed to add (exists or error): %s\n", code);
            continue;
        }
        if (strcmp(cmd, "remove_airport") == 0) {
            char *code = strtok(NULL, " \t\n\r");
            if (!code) { printf("Usage: remove_airport CODE\n"); continue; }
            if (graph_remove_airport(g, code) == 0) printf("Removed %s\n", code);
            else printf("Failed to remove (not found): %s\n", code);
            continue;
        }
        if (strcmp(cmd, "add_route") == 0) {
            char *from = strtok(NULL, " \t\n\r");
            char *to = strtok(NULL, " \t\n\r");
            if (!from || !to) { printf("Usage: add_route FROM TO\n"); continue; }
            if (graph_add_route(g, from, to) == 0) printf("Added route %s -> %s\n", from, to);
            else printf("Failed to add route (missing airport): %s -> %s\n", from, to);
            continue;
        }
        if (strcmp(cmd, "remove_route") == 0) {
            char *from = strtok(NULL, " \t\n\r");
            char *to = strtok(NULL, " \t\n\r");
            if (!from || !to) { printf("Usage: remove_route FROM TO\n"); continue; }
            if (graph_remove_route(g, from, to) == 0) printf("Removed route %s -> %s\n", from, to);
            else printf("Failed to remove route: %s -> %s\n", from, to);
            continue;
        }
        if (strcmp(cmd, "outgoing") == 0) {
            char *code = strtok(NULL, " \t\n\r");
            if (!code) { printf("Usage: outgoing CODE\n"); continue; }
            graph_list_outgoing(g, code);
            continue;
        }
        if (strcmp(cmd, "incoming") == 0) {
            char *code = strtok(NULL, " \t\n\r");
            if (!code) { printf("Usage: incoming CODE\n"); continue; }
            graph_list_incoming(g, code);
            continue;
        }
        if (strcmp(cmd, "matrix") == 0) { graph_print_matrix(g); continue; }
        if (strcmp(cmd, "list") == 0) { graph_list_airports(g); continue; }
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) break;
        printf("Unknown command: %s\n", cmd);
    }

    graph_free(g);
    return 0;
}
