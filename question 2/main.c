#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bst.h"

#define MAX_LINE 256
#define MAX_PROCS 50
#define DEFAULT_SUGGEST_THRESHOLD 3

static void trim_newline(char *s)
{
    size_t l = strlen(s);
    if (l == 0)
        return;
    if (s[l - 1] == '\n' || s[l - 1] == '\r')
        s[l - 1] = '\0';
}

static int levenshtein(const char *s, const char *t)
{
    size_t n = strlen(s), m = strlen(t);
    if (n == 0)
        return (int)m;
    if (m == 0)
        return (int)n;
    int *v0 = malloc((m + 1) * sizeof(int));
    int *v1 = malloc((m + 1) * sizeof(int));
    if (!v0 || !v1)
    {
        free(v0);
        free(v1);
        return 1000000;
    }
    for (size_t j = 0; j <= m; ++j)
        v0[j] = (int)j;
    for (size_t i = 0; i < n; ++i)
    {
        v1[0] = (int)(i + 1);
        for (size_t j = 0; j < m; ++j)
        {
            int cost = (s[i] == t[j]) ? 0 : 1;
            int deletion = v0[j + 1] + 1;
            int insertion = v1[j] + 1;
            int substitution = v0[j] + cost;
            int min = deletion < insertion ? deletion : insertion;
            if (substitution < min)
                min = substitution;
            v1[j + 1] = min;
        }
        int *tmp = v0;
        v0 = v1;
        v1 = tmp;
    }
    int res = v0[m];
    free(v0);
    free(v1);
    return res;
}

typedef struct ClosestCtx
{
    const char *input;
    const char *best;
    int best_dist;
} ClosestCtx;

static void find_closest_fn(const char *key, void *ctxv)
{
    ClosestCtx *ctx = (ClosestCtx *)ctxv;
    int d = levenshtein(ctx->input, key);
    if (d < ctx->best_dist)
    {
        ctx->best_dist = d;
        ctx->best = key;
    }
}

static const char *find_closest(Node *root, const char *input)
{
    ClosestCtx ctx = {.input = input, .best = NULL, .best_dist = INT_MAX};
    bst_traverse(root, find_closest_fn, &ctx);
    return ctx.best;
}

static void log_rejected(const char *audit_path, const char *attempt)
{
    FILE *f = fopen(audit_path, "a");
    if (!f)
        return;
    time_t now = time(NULL);
    char buf[64];
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(f, "%s | REJECTED | %s\n", buf, attempt);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [-t N] procedures_file\n", argv[0]);
        return 1;
    }

    int suggest_threshold = DEFAULT_SUGGEST_THRESHOLD;
    const char *procfile = NULL;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
        {
            suggest_threshold = atoi(argv[i + 1]);
            i += 1;
            continue;
        }
        procfile = argv[i];
    }
    if (!procfile)
    {
        fprintf(stderr, "Usage: %s [-t N] procedures_file\n", argv[0]);
        return 1;
    }

    /* Allow environment override */
    const char *env_t = getenv("SUGGEST_THRESHOLD");
    if (env_t)
    {
        suggest_threshold = atoi(env_t);
    }
    FILE *f = fopen(procfile, "r");
    if (!f)
    {
        perror("fopen");
        return 1;
    }
    Node *root = NULL;
    char line[MAX_LINE];
    int count = 0;
    while (fgets(line, sizeof(line), f))
    {
        trim_newline(line);
        if (line[0] == '\0')
            continue;
        if (count >= MAX_PROCS)
        {
            fprintf(stderr, "Warning: max procedures (%d) reached, skipping remaining\n", MAX_PROCS);
            break;
        }
        root = bst_insert(root, line);
        ++count;
    }
    fclose(f);
    if (!root)
    {
        fprintf(stderr, "No procedures loaded.\n");
        return 1;
    }

    /* determine audit log path next to procfile */
    char audit_path[MAX_LINE];
    const char *last_slash = NULL;
    for (const char *p = procfile; *p; ++p)
    {
        if (*p == '/' || *p == '\\')
            last_slash = p;
    }
    if (last_slash)
    {
        size_t dirlen = (size_t)(last_slash - procfile + 1);
        if (dirlen + strlen("audit.log") + 1 < sizeof(audit_path))
        {
            memcpy(audit_path, procfile, dirlen);
            audit_path[dirlen] = '\0';
            strncat(audit_path, "audit.log", sizeof(audit_path) - strlen(audit_path) - 1);
        }
        else
        {
            strncpy(audit_path, "audit.log", sizeof(audit_path) - 1);
            audit_path[sizeof(audit_path) - 1] = '\0';
        }
    }
    else
    {
        strncpy(audit_path, "audit.log", sizeof(audit_path) - 1);
        audit_path[sizeof(audit_path) - 1] = '\0';
    }

    printf("Loaded %d procedures. Enter procedures (Ctrl+D to exit).\n", count);
    while (1)
    {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin))
            break;
        trim_newline(line);
        if (line[0] == '\0')
            continue;
        Node *found = bst_find(root, line);
        if (found)
        {
            printf("APPROVED: %s\n", found->key);
            continue;
        }
        const char *closest = find_closest(root, line);
        if (closest)
        {
            int d = levenshtein(line, closest);
            if (d <= suggest_threshold)
            {
                printf("Did you mean: %s ? (distance=%d)\n", closest, d);
                continue;
            }
        }
        printf("REJECTED: %s (logged)\n", line);
        log_rejected(audit_path, line);
    }

    bst_free(root);
    return 0;
}
