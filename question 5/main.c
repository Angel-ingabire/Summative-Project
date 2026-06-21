#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#define SYMBOLS 256

typedef struct Node
{
    unsigned char symbol;
    uint64_t freq;
    struct Node *left, *right;
} Node;

typedef struct Heap
{
    Node **data;
    int size;
    int cap;
} Heap;

static void heap_init(Heap *h)
{
    h->cap = 256;
    h->size = 0;
    h->data = malloc(sizeof(Node *) * h->cap);
}
static void heap_push(Heap *h, Node *n)
{
    if (h->size + 1 > h->cap)
    {
        h->cap *= 2;
        h->data = realloc(h->data, sizeof(Node *) * h->cap);
    }
    int i = h->size++;
    h->data[i] = n;
    while (i > 0)
    {
        int p = (i - 1) / 2;
        if (h->data[p]->freq <= h->data[i]->freq)
            break;
        Node *t = h->data[p];
        h->data[p] = h->data[i];
        h->data[i] = t;
        i = p;
    }
}
static Node *heap_pop(Heap *h)
{
    if (h->size == 0)
        return NULL;
    Node *ret = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (1)
    {
        int l = 2 * i + 1, r = 2 * i + 2, smallest = i;
        if (l < h->size && h->data[l]->freq < h->data[smallest]->freq)
            smallest = l;
        if (r < h->size && h->data[r]->freq < h->data[smallest]->freq)
            smallest = r;
        if (smallest == i)
            break;
        Node *t = h->data[i];
        h->data[i] = h->data[smallest];
        h->data[smallest] = t;
        i = smallest;
    }
    return ret;
}
static void heap_free(Heap *h) { free(h->data); }

static Node *node_create(unsigned char symbol, uint64_t freq)
{
    Node *n = malloc(sizeof(Node));
    n->symbol = symbol;
    n->freq = freq;
    n->left = n->right = NULL;
    return n;
}

// serialize tree into buffer (preorder): internal mark 0x00, leaf mark 0x01 + symbol
typedef struct Buf
{
    unsigned char *b;
    size_t len;
    size_t cap;
} Buf;
static void buf_init(Buf *buf)
{
    buf->cap = 256;
    buf->len = 0;
    buf->b = malloc(buf->cap);
}
static void buf_push(Buf *buf, unsigned char v)
{
    if (buf->len + 1 > buf->cap)
    {
        buf->cap *= 2;
        buf->b = realloc(buf->b, buf->cap);
    }
    buf->b[buf->len++] = v;
}
static void serialize(Node *n, Buf *buf)
{
    if (!n)
        return;
    if (!n->left && !n->right)
    {
        buf_push(buf, 0x01);
        buf_push(buf, n->symbol);
    }
    else
    {
        buf_push(buf, 0x00);
        serialize(n->left, buf);
        serialize(n->right, buf);
    }
}

static Node *deserialize(unsigned char *data, size_t *pos, size_t len)
{
    if (*pos >= len)
        return NULL;
    unsigned char tag = data[(*pos)++];
    if (tag == 0x01)
    {
        if (*pos >= len)
            return NULL;
        unsigned char sym = data[(*pos)++];
        return node_create(sym, 0);
    }
    else if (tag == 0x00)
    {
        Node *n = node_create(0, 0);
        n->left = deserialize(data, pos, len);
        n->right = deserialize(data, pos, len);
        return n;
    }
    return NULL;
}

static void free_tree(Node *n)
{
    if (!n)
        return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

static void build_codes(Node *root, char **codes, char *stack, int depth)
{
    if (!root)
        return;
    if (!root->left && !root->right)
    {
        stack[depth] = '\0';
        codes[root->symbol] = strdup(stack);
        return;
    }
    if (root->left)
    {
        stack[depth] = '0';
        build_codes(root->left, codes, stack, depth + 1);
    }
    if (root->right)
    {
        stack[depth] = '1';
        build_codes(root->right, codes, stack, depth + 1);
    }
}

static void free_codes(char **codes)
{
    for (int i = 0; i < SYMBOLS; ++i)
        if (codes[i])
            free(codes[i]);
}

static int compress_file(const char *input_path)
{
    FILE *in = fopen(input_path, "rb");
    if (!in)
    {
        fprintf(stderr, "Failed to open input: %s\n", input_path);
        return 1;
    }
    uint64_t freq[SYMBOLS] = {0};
    uint64_t total = 0;
    int c;
    while ((c = fgetc(in)) != EOF)
    {
        freq[(unsigned char)c]++;
        total++;
    }
    if (total == 0)
    {
        fclose(in);
        fprintf(stderr, "Input file empty.\n");
        return 1;
    }
    // build heap
    Heap h;
    heap_init(&h);
    for (int i = 0; i < SYMBOLS; ++i)
    {
        if (freq[i])
            heap_push(&h, node_create((unsigned char)i, freq[i]));
    }
    if (h.size == 0)
    {
        heap_free(&h);
        fclose(in);
        fprintf(stderr, "No symbols to encode.\n");
        return 1;
    }
    while (h.size > 1)
    {
        Node *a = heap_pop(&h);
        Node *b = heap_pop(&h);
        Node *p = node_create(0, a->freq + b->freq);
        p->left = a;
        p->right = b;
        heap_push(&h, p);
    }
    Node *root = heap_pop(&h);
    heap_free(&h);

    // generate codes
    char *codes[SYMBOLS] = {0};
    char stack[512];
    build_codes(root, codes, stack, 0);

    // serialize tree
    Buf treebuf;
    buf_init(&treebuf);
    serialize(root, &treebuf);

    // open output file telemetry.huff in same directory as input
    char outpath[FILENAME_MAX];
    const char *slash = strrchr(input_path, '\\');
    if (!slash)
        slash = strrchr(input_path, '/');
    if (!slash)
        snprintf(outpath, sizeof(outpath), "telemetry.huff");
    else
    {
        size_t dirlen = slash - input_path + 1;
        if (dirlen >= sizeof(outpath))
            dirlen = sizeof(outpath) - 1;
        strncpy(outpath, input_path, dirlen);
        outpath[dirlen] = '\0';
        strncat(outpath, "telemetry.huff", sizeof(outpath) - strlen(outpath) - 1);
    }
    FILE *out = fopen(outpath, "wb");
    if (!out)
    {
        fprintf(stderr, "Failed to open output: %s\n", outpath);
        free(treebuf.b);
        free_codes(codes);
        free_tree(root);
        fclose(in);
        return 1;
    }

    // write header
    fwrite("HUF1", 1, 4, out);
    fwrite(&total, sizeof(uint64_t), 1, out);
    uint32_t tree_size = (uint32_t)treebuf.len;
    fwrite(&tree_size, sizeof(uint32_t), 1, out);
    fwrite(treebuf.b, 1, treebuf.len, out);

    // write bits to memory buffer
    fseek(in, 0, SEEK_SET);
    Buf outbits;
    buf_init(&outbits);
    unsigned char bitbuf = 0;
    int bitpos = 0;
    uint64_t bits_count = 0;
    while ((c = fgetc(in)) != EOF)
    {
        const char *code = codes[(unsigned char)c];
        for (const char *p = code; *p; ++p)
        {
            bitbuf = (bitbuf << 1) | (unsigned char)(*p == '1');
            bitpos++;
            bits_count++;
            if (bitpos == 8)
            {
                buf_push(&outbits, bitbuf);
                bitbuf = 0;
                bitpos = 0;
            }
        }
    }
    if (bitpos > 0)
    {
        bitbuf = bitbuf << (8 - bitpos);
        buf_push(&outbits, bitbuf);
    }

    fwrite(&bits_count, sizeof(uint64_t), 1, out);
    if (outbits.len)
        fwrite(outbits.b, 1, outbits.len, out);
    fclose(out);
    fclose(in);

    uint64_t original_size = total;
    uint64_t compressed_size = (uint64_t)(4 + sizeof(uint64_t) + sizeof(uint32_t) + treebuf.len + sizeof(uint64_t) + outbits.len);
    printf("Original size: %" PRIu64 " bytes\n", original_size);
    printf("Compressed size: %" PRIu64 " bytes\n", compressed_size);
    double ratio = (double)compressed_size / (double)original_size;
    printf("Compression ratio: %.3f\n", ratio);

    // cleanup
    free(treebuf.b);
    free(outbits.b);
    free_codes(codes);
    free_tree(root);

    return 0;
}

static int decompress_file(const char *huffpath)
{
    // open telemetry.huff (huffpath) and restore to telemetry_restored.txt in same dir
    FILE *in = fopen(huffpath, "rb");
    if (!in)
    {
        fprintf(stderr, "Failed to open huff file: %s\n", huffpath);
        return 1;
    }
    char magic[5] = {0};
    if (fread(magic, 1, 4, in) != 4 || strcmp(magic, "HUF1") != 0)
    {
        fprintf(stderr, "Not a supported .huff file\n");
        fclose(in);
        return 1;
    }
    uint64_t original_size = 0;
    fread(&original_size, sizeof(uint64_t), 1, in);
    uint32_t tree_size = 0;
    fread(&tree_size, sizeof(uint32_t), 1, in);
    unsigned char *treebuf = malloc(tree_size);
    if (fread(treebuf, 1, tree_size, in) != tree_size)
    {
        fprintf(stderr, "Failed reading tree\n");
        free(treebuf);
        fclose(in);
        return 1;
    }
    uint64_t bits_count = 0;
    fread(&bits_count, sizeof(uint64_t), 1, in);
    // remaining bytes are bitstream
    size_t bitstream_bytes = 0;
    // compute remaining file size
    fseek(in, 0, SEEK_END);
    long file_end = ftell(in);
    long data_start = 4 + sizeof(uint64_t) + sizeof(uint32_t) + tree_size + sizeof(uint64_t);
    if (file_end < data_start)
    {
        fprintf(stderr, "Corrupt file\n");
        free(treebuf);
        fclose(in);
        return 1;
    }
    bitstream_bytes = (size_t)(file_end - data_start);
    fseek(in, data_start, SEEK_SET);
    unsigned char *bitstream = NULL;
    if (bitstream_bytes)
    {
        bitstream = malloc(bitstream_bytes);
        if (fread(bitstream, 1, bitstream_bytes, in) != bitstream_bytes)
        {
            fprintf(stderr, "Failed reading bitstream\n");
            free(treebuf);
            free(bitstream);
            fclose(in);
            return 1;
        }
    }
    fclose(in);

    // deserialize tree
    size_t pos = 0;
    Node *root = deserialize(treebuf, &pos, tree_size);
    free(treebuf);
    if (!root)
    {
        fprintf(stderr, "Failed to reconstruct tree\n");
        free(bitstream);
        return 1;
    }

    // build output path telemetry_restored.txt in same directory as huffpath
    char outpath[FILENAME_MAX];
    const char *slash = strrchr(huffpath, '\\');
    if (!slash)
        slash = strrchr(huffpath, '/');
    if (!slash)
        snprintf(outpath, sizeof(outpath), "telemetry_restored.txt");
    else
    {
        size_t dirlen = slash - huffpath + 1;
        if (dirlen >= sizeof(outpath))
            dirlen = sizeof(outpath) - 1;
        strncpy(outpath, huffpath, dirlen);
        outpath[dirlen] = '\0';
        strncat(outpath, "telemetry_restored.txt", sizeof(outpath) - strlen(outpath) - 1);
    }
    FILE *out = fopen(outpath, "wb");
    if (!out)
    {
        fprintf(stderr, "Failed to open output: %s\n", outpath);
        free(bitstream);
        free_tree(root);
        return 1;
    }

    // decode bits_count bits
    Node *cur = root;
    uint64_t written = 0;
    for (uint64_t i = 0; i < bits_count; ++i)
    {
        size_t byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        int bit = 0;
        if (byte_idx < bitstream_bytes)
            bit = (bitstream[byte_idx] >> bit_idx) & 1;
        if (bit)
            cur = cur->right;
        else
            cur = cur->left;
        if (!cur->left && !cur->right)
        {
            fputc(cur->symbol, out);
            written++;
            cur = root;
            if (written >= original_size)
                break; // stop if we've written original size
        }
    }

    fclose(out);
    free(bitstream);
    free_tree(root);

    printf("Decompression complete, restored file: %s\n", outpath);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage:\n  %s -c <input.txt>   Compress to telemetry.huff\n  %s -d <telemetry.huff>   Decompress to telemetry_restored.txt\n", argv[0], argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-c") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Missing input file for compress\n");
            return 1;
        }
        int r = compress_file(argv[2]);
        if (r != 0)
            return r;
        printf("Compressed output saved as telemetry.huff\n");
        return 0;
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        const char *huff = "telemetry.huff";
        if (argc >= 3)
            huff = argv[2];
        return decompress_file(huff);
    }
    fprintf(stderr, "Unknown option\n");
    return 1;
}