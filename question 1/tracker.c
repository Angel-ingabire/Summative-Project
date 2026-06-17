#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define MAX_INCIDENTS 25
#define TIMESTAMP_LEN 32
#define SOURCE_LEN 32
#define DESC_LEN 256
#define SESSION_FILE "session.json"

typedef struct Incident {
    int id;
    char timestamp[TIMESTAMP_LEN];
    char source[SOURCE_LEN];
    char description[DESC_LEN];
    struct Incident *prev;
    struct Incident *next;
} Incident;

typedef struct {
    Incident *head; // oldest
    Incident *tail; // newest
    int size;
    int capacity;
} IncidentList;

static IncidentList list = {0};
static Incident *active = NULL; // active pointer
static int next_id = 1;
static volatile int live_flag = 0;
static HANDLE live_thread = NULL;
static CRITICAL_SECTION cs;

// Helpers
static void iso_timestamp(char *out, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_s(&tm, &t);
    strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static char *json_escape(const char *in) {
    size_t len = strlen(in);
    // worst-case every char is escaped
    char *out = malloc(len * 2 + 1);
    char *p = out;
    for (; *in; ++in) {
        if (*in == '"' || *in == '\\') {
            *p++ = '\\';
            *p++ = *in;
        } else if (*in == '\n') {
            *p++ = '\\'; *p++ = 'n';
        } else {
            *p++ = *in;
        }
    }
    *p = '\0';
    return out;
}

// List operations
static void init_list(int capacity) {
    list.head = list.tail = NULL;
    list.size = 0;
    list.capacity = capacity;
}

static void discard_oldest() {
    Incident *old = list.head;
    if (!old) return;
    list.head = old->next;
    if (list.head) list.head->prev = NULL;
    else list.tail = NULL;
    if (active == old) active = list.head;
    free(old);
    list.size--;
}

static Incident *append_incident_preserve(int id, const char *timestamp, const char *source, const char *desc) {
    Incident *node = malloc(sizeof(Incident));
    node->id = id;
    strncpy(node->timestamp, timestamp, TIMESTAMP_LEN-1); node->timestamp[TIMESTAMP_LEN-1] = '\0';
    strncpy(node->source, source, SOURCE_LEN-1); node->source[SOURCE_LEN-1] = '\0';
    strncpy(node->description, desc, DESC_LEN-1); node->description[DESC_LEN-1] = '\0';
    node->next = NULL;
    node->prev = list.tail;
    if (!list.head) list.head = node;
    if (list.tail) list.tail->next = node;
    list.tail = node;
    list.size++;
    if (list.size > list.capacity) discard_oldest();
    return node;
}

static Incident *append_incident_auto(const char *source, const char *desc) {
    char ts[TIMESTAMP_LEN];
    iso_timestamp(ts, sizeof(ts));
    Incident *node = append_incident_preserve(next_id++, ts, source, desc);
    if (!active) active = list.head;
    return node;
}

static void clear_all() {
    Incident *cur = list.head;
    while (cur) {
        Incident *n = cur->next;
        free(cur);
        cur = n;
    }
    list.head = list.tail = NULL;
    list.size = 0;
    active = NULL;
}

// Persistence (naive JSON) - writes stable JSON & reads minimally tolerant JSON produced by this program
static void save_session() {
    EnterCriticalSection(&cs);
    FILE *f = fopen(SESSION_FILE, "w");
    if (!f) {
        LeaveCriticalSection(&cs);
        return;
    }
    fprintf(f, "[\n");
    Incident *cur = list.head;
    while (cur) {
        char *esc_desc = json_escape(cur->description);
        char *esc_src = json_escape(cur->source);
        fprintf(f, "  {\"id\": %d, \"timestamp\": \"%s\", \"source\": \"%s\", \"description\": \"%s\"}",
            cur->id, cur->timestamp, esc_src, esc_desc);
        free(esc_desc); free(esc_src);
        cur = cur->next;
        if (cur) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "]\n");
    fclose(f);
    LeaveCriticalSection(&cs);
}

static char *extract_string_value(const char *buf, const char *key) {
    const char *p = strstr(buf, key);
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p == '"') {
        p++;
        const char *q = p;
        // find ending quote
        while (*q && *q != '"') {
            if (*q == '\\' && q[1]) q += 2; else q++;
        }
        size_t len = (q - p);
        char *out = malloc(len + 1);
        size_t i = 0;
        const char *r = p;
        while (r < q) {
            if (*r == '\\' && r[1]) {
                r++;
                out[i++] = *r++;
            } else {
                out[i++] = *r++;
            }
        }
        out[i] = '\0';
        return out;
    }
    return NULL;
}

static int extract_int_value(const char *buf, const char *key) {
    const char *p = strstr(buf, key);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return atoi(p);
}

static void load_session() {
    EnterCriticalSection(&cs);
    FILE *f = fopen(SESSION_FILE, "r");
    if (!f) { LeaveCriticalSection(&cs); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    // naive parse: find occurrences of '{' and parse fields inside
    const char *p = buf;
    while ((p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        if (!end) break;
        size_t len = end - p + 1;
        char *obj = malloc(len + 1);
        strncpy(obj, p, len);
        obj[len] = '\0';
        int id = extract_int_value(obj, "\"id\"");
        char *ts = extract_string_value(obj, "\"timestamp\"");
        char *src = extract_string_value(obj, "\"source\"");
        char *desc = extract_string_value(obj, "\"description\"");
        if (ts && src && desc) {
            append_incident_preserve(id, ts, src, desc);
            if (id >= next_id) next_id = id + 1;
        }
        free(ts); free(src); free(desc); free(obj);
        p = end + 1;
    }
    if (!list.head) active = NULL; else active = list.head;
    free(buf);
    LeaveCriticalSection(&cs);
}

// Live thread
static DWORD WINAPI live_thread_func(LPVOID arg) {
    (void)arg;
    const char *sources[] = {"Ambulance", "Police", "Fire"};
    const char *descs[] = {"Vehicle collision","Medical emergency","Structure fire","Suspicious activity","Road obstruction"};
    size_t sn = sizeof(sources)/sizeof(sources[0]);
    size_t dn = sizeof(descs)/sizeof(descs[0]);
    while (live_flag) {
        EnterCriticalSection(&cs);
        append_incident_auto(sources[rand()%sn], descs[rand()%dn]);
        LeaveCriticalSection(&cs);
        Sleep(2000);
    }
    return 0;
}

// Navigation
static void move_newer() {
    EnterCriticalSection(&cs);
    if (!active) { LeaveCriticalSection(&cs); return; }
    if (active->next) active = active->next;
    LeaveCriticalSection(&cs);
}

static void move_older() {
    EnterCriticalSection(&cs);
    if (!active) { LeaveCriticalSection(&cs); return; }
    if (active->prev) active = active->prev;
    LeaveCriticalSection(&cs);
}

static int index_of_active() {
    EnterCriticalSection(&cs);
    int idx = -1;
    if (active) {
        Incident *cur = list.head;
        int i = 1;
        while (cur) {
            if (cur == active) { idx = i; break; }
            cur = cur->next; i++;
        }
    }
    LeaveCriticalSection(&cs);
    return idx;
}

static void print_status() {
    EnterCriticalSection(&cs);
    int total = list.size;
    int idx = index_of_active();
    printf("\n--- Emergency Dispatch Incident Tracker ---\n");
    printf("Stored incidents: %d | Live: %s\n", total, live_flag?"ON":"OFF");
    printf("Commands: f=forward(newer), b=back(older), l=live on, s=live off, d=delete all, q=save+quit\n");
    printf("Current:\n");
    if (!active) {
        printf("No incidents recorded.\n");
    } else {
        printf("ID:%d Time:%s Source:%s\n  %s (%d/%d)\n", active->id, active->timestamp, active->source, active->description, idx, total);
    }
    LeaveCriticalSection(&cs);
}

int main(void) {
    srand((unsigned)time(NULL));
    InitializeCriticalSection(&cs);
    init_list(MAX_INCIDENTS);
    load_session();

    // Ensure active points to oldest at startup
    if (!active && list.head) active = list.head;

    char line[64];
    while (1) {
        print_status();
        printf("\nEnter command: ");
        if (!fgets(line, sizeof(line), stdin)) break;
        char cmd = line[0];
        if (cmd == 'f') {
            move_newer();
        } else if (cmd == 'b') {
            move_older();
        } else if (cmd == 'l') {
            if (!live_flag) {
                live_flag = 1;
                live_thread = CreateThread(NULL, 0, live_thread_func, NULL, 0, NULL);
            }
        } else if (cmd == 's') {
            if (live_flag) {
                live_flag = 0;
                if (live_thread) {
                    WaitForSingleObject(live_thread, 2000);
                    CloseHandle(live_thread);
                    live_thread = NULL;
                }
            }
        } else if (cmd == 'd') {
            EnterCriticalSection(&cs);
            clear_all();
            LeaveCriticalSection(&cs);
            printf("All incidents deleted.\n");
        } else if (cmd == 'q') {
            printf("Saving session and quitting...\n");
            if (live_flag) {
                live_flag = 0;
                if (live_thread) {
                    WaitForSingleObject(live_thread, 2000);
                    CloseHandle(live_thread);
                    live_thread = NULL;
                }
            }
            save_session();
            break;
        } else {
            printf("Unknown command.\n");
        }
    }

    // cleanup
    save_session();
    clear_all();
    DeleteCriticalSection(&cs);
    return 0;
}
