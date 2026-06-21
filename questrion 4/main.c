#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define NUM_NODES 7
#define INF INT_MAX / 4

const char *names[NUM_NODES] = {
    "Library",
    "Cafeteria",
    "Engineering",
    "Science Block",
    "Dormitory",
    "Administration",
    "Charging Station"};

void normalize(const char *in, char *out)
{
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0'; ++i)
    {
        if (!isspace((unsigned char)in[i]))
        {
            out[j++] = (char)tolower((unsigned char)in[i]);
        }
    }
    out[j] = '\0';
}

int get_index_by_name(const char *input)
{
    char key[128];
    normalize(input, key);
    char nodekey[128];
    for (int i = 0; i < NUM_NODES; ++i)
    {
        normalize(names[i], nodekey);
        if (strcmp(key, nodekey) == 0)
            return i;
    }
    return -1;
}

void print_available()
{
    printf("Available buildings:\n");
    for (int i = 0; i < NUM_NODES; ++i)
        printf(" - %s\n", names[i]);
}

void dijkstra(int graph[NUM_NODES][NUM_NODES], int src, int target)
{
    int dist[NUM_NODES];
    int prev[NUM_NODES];
    int vis[NUM_NODES] = {0};

    for (int i = 0; i < NUM_NODES; ++i)
    {
        dist[i] = INF;
        prev[i] = -1;
    }
    dist[src] = 0;

    for (int step = 0; step < NUM_NODES; ++step)
    {
        int u = -1;
        int best = INF;
        for (int i = 0; i < NUM_NODES; ++i)
        {
            if (!vis[i] && dist[i] < best)
            {
                best = dist[i];
                u = i;
            }
        }
        if (u == -1)
            break;
        vis[u] = 1;
        if (u == target)
            break;
        for (int v = 0; v < NUM_NODES; ++v)
        {
            if (!vis[v] && graph[u][v] < INF)
            {
                if (dist[u] + graph[u][v] < dist[v])
                {
                    dist[v] = dist[u] + graph[u][v];
                    prev[v] = u;
                }
            }
        }
    }

    if (dist[target] >= INF)
    {
        printf("No route from %s to %s.\n", names[src], names[target]);
        return;
    }

    // reconstruct path
    int path[NUM_NODES];
    int count = 0;
    for (int at = target; at != -1; at = prev[at])
    {
        path[count++] = at;
    }
    // reverse
    printf("Path taken: ");
    for (int i = count - 1; i >= 0; --i)
    {
        printf("%s", names[path[i]]);
        if (i)
            printf(" -> ");
    }
    printf("\nTotal travel distance: %d\n", dist[target]);
}

int is_digits(const char *s)
{
    if (!s || !*s)
        return 0;
    for (const char *p = s; *p; ++p)
        if (!isdigit((unsigned char)*p))
            return 0;
    return 1;
}

void print_usage(const char *prog)
{
    printf("Usage: %s [START]\n", prog);
    printf("Options:\n");
    printf("  --list       Print available buildings and indexes\n");
    printf("  --help       Show this help\n");
    printf("If START is omitted the program prompts interactively. START can be a building name or its index.\n");
}

int main(int argc, char **argv)
{
    int graph[NUM_NODES][NUM_NODES];
    for (int i = 0; i < NUM_NODES; ++i)
        for (int j = 0; j < NUM_NODES; ++j)
            graph[i][j] = (i == j) ? 0 : INF;

    // Undirected weighted edges as specified
    // Library(0) --6-- Cafeteria(1)
    graph[0][1] = graph[1][0] = 6;
    // Library(0) --15-- Engineering(2)
    graph[0][2] = graph[2][0] = 15;
    // Cafeteria(1) --4-- Science Block(3)
    graph[1][3] = graph[3][1] = 4;
    // Science Block(3) --8-- Dormitory(4)
    graph[3][4] = graph[4][3] = 8;
    // Engineering(2) --5-- Administration(5)
    graph[2][5] = graph[5][2] = 5;
    // Administration(5) --3-- Dormitory(4)
    graph[5][4] = graph[4][5] = 3;
    // Cafeteria(1) --2-- Charging Station(6)
    graph[1][6] = graph[6][1] = 2;
    // Charging Station(6) --4-- Administration(5)
    graph[6][5] = graph[5][6] = 4;

    char input[128];
    int src = -1;

    if (argc >= 2)
    {
        if (strcmp(argv[1], "--list") == 0)
        {
            print_available();
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        // argument provided as start
        if (is_digits(argv[1]))
        {
            int idx = atoi(argv[1]);
            if (idx >= 0 && idx < NUM_NODES)
                src = idx;
            else
            {
                printf("Invalid index: %s\n\n", argv[1]);
                print_available();
                return 1;
            }
        }
        else
        {
            src = get_index_by_name(argv[1]);
            if (src == -1)
            {
                printf("Invalid building name: %s\n\n", argv[1]);
                print_available();
                return 1;
            }
        }
    }
    else
    {
        // interactive mode
        printf("Enter starting building name or index (type --list to see options): ");
        if (!fgets(input, sizeof(input), stdin))
        {
            fprintf(stderr, "Failed to read input.\n");
            return 1;
        }
        // strip newline
        input[strcspn(input, "\r\n")] = '\0';
        if (strlen(input) == 0)
        {
            fprintf(stderr, "No input provided.\n");
            print_available();
            return 1;
        }

        if (strcmp(input, "--list") == 0)
        {
            print_available();
            return 0;
        }

        if (is_digits(input))
        {
            int idx = atoi(input);
            if (idx >= 0 && idx < NUM_NODES)
                src = idx;
            else
            {
                printf("Invalid index: %s\n\n", input);
                print_available();
                return 1;
            }
        }
        else
        {
            src = get_index_by_name(input);
        }
    }

    if (src == -1)
    {
        printf("Invalid building name or index.\n\n");
        print_available();
        return 1;
    }
    if (src == -1)
    {
        printf("Invalid building name: %s\n\n", input);
        print_available();
        return 1;
    }

    int target = get_index_by_name("Dormitory");
    dijkstra(graph, src, target);
    return 0;
}
