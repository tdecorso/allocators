#include <allocators.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdalign.h>

static int failed = 0;
static int total  = 0;

/* ANSI colors */
#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[2m"
#define C_GREEN   "\033[32m"
#define C_RED     "\033[31m"
#define C_CYAN    "\033[36m"

#define ASSERT(cond, msg)                                                  \
    do {                                                                   \
        printf(C_DIM "    • %-80s" C_RESET, (msg));                        \
        if (!(cond)) {                                                     \
            printf(C_RED "✗" C_RESET C_DIM " (%s:%d)\n" C_RESET,           \
                   __FILE__, __LINE__);                                    \
            return 1;                                                      \
        }                                                                  \
        printf(C_GREEN "✓" C_RESET "\n");                                  \
    } while (0)

#define RUN_TEST(name, fun)                                                \
    do {                                                                   \
        printf(C_BOLD C_CYAN "▶ %s\n" C_RESET, (name));                    \
        total++;                                                           \
        if (fun()) {                                                       \
            failed++;                                                      \
            printf("  " C_RED C_BOLD "✗ FAILED" C_RESET "\n\n");           \
        } else {                                                           \
            printf("  " C_GREEN C_BOLD "✓ PASSED" C_RESET "\n\n");         \
        }                                                                  \
    } while (0)

#define SUMMARY()                                                          \
    do {                                                                   \
        printf(C_DIM "───────────────────────────────────────\n" C_RESET); \
        if (failed > 0) {                                                  \
            printf(C_RED C_BOLD "✗ %d/%d tests failed" C_RESET "\n",       \
                   failed, total);                                         \
        } else {                                                           \
            printf(C_GREEN C_BOLD "✓ All %d tests passed" C_RESET "\n",    \
                   total);                                                 \
        }                                                                  \
    } while (0)

/* A realistic slot type — simulates an XML node or widget */
typedef struct {
    int     id;
    float   x, y, w, h;
    char    name[32];
} node_t;

/* ---------------------------------------------------------------- */
/* pool_create                                                       */
/* ---------------------------------------------------------------- */

int test_pool_create(void) {
    error_t  err;
    pool_t  *pool;

    /* valid creation */
    pool = pool_create(sizeof(node_t), 16, &err);
    ASSERT(pool != NULL,         "pool_create returns non-NULL");
    ASSERT(err.code == ERROR_OK, "pool_create sets ERROR_OK");
    pool_destroy(pool);

    /* slot_size 0 is invalid */
    err.code = ERROR_OK;
    pool = pool_create(0, 16, &err);
    ASSERT(pool == NULL,                    "pool_create with slot_size 0 returns NULL");
    ASSERT(err.code == ERROR_INVALID_ARGS,  "pool_create with slot_size 0 sets invalid args error");

    /* block_slots 0 is invalid */
    err.code = ERROR_OK;
    pool = pool_create(sizeof(node_t), 0, &err);
    ASSERT(pool == NULL,                    "pool_create with block_slots 0 returns NULL");
    ASSERT(err.code == ERROR_INVALID_ARGS,  "pool_create with block_slots 0 sets invalid args error");

    /* small slot_size is rounded up to hold a free-list pointer */
    pool = pool_create(1, 8, &err);
    ASSERT(pool != NULL,         "pool_create with slot_size 1 succeeds");
    ASSERT(err.code == ERROR_OK, "pool_create with slot_size 1 sets ERROR_OK");
    pool_destroy(pool);

    /* NULL err pointer is safe */
    pool = pool_create(sizeof(node_t), 16, NULL);
    ASSERT(pool != NULL, "pool_create works fine with NULL err pointer");
    pool_destroy(pool);

    return 0;
}

/* ---------------------------------------------------------------- */
/* pool_destroy                                                      */
/* ---------------------------------------------------------------- */

int test_pool_destroy(void) {
    /* NULL is safe */
    pool_destroy(NULL);
    ASSERT(1, "pool_destroy on NULL does not crash");

    /* destroy a pool that never allocated anything */
    pool_t *pool = pool_create(sizeof(node_t), 16, NULL);
    ASSERT(pool != NULL, "pool_create succeeds before destroy test");
    pool_destroy(pool);
    ASSERT(1, "pool_destroy on empty pool does not crash");

    /* destroy after allocating across multiple blocks */
    pool = pool_create(sizeof(node_t), 4, NULL);
    ASSERT(pool != NULL, "pool_create with small block_slots succeeds");
    for (int i = 0; i < 20; i++)
        pool_alloc(pool, NULL);
    pool_destroy(pool);
    ASSERT(1, "pool_destroy on multi-block pool does not crash");

    return 0;
}

/* ---------------------------------------------------------------- */
/* pool_alloc                                                        */
/* ---------------------------------------------------------------- */

int test_pool_alloc(void) {
    error_t  err;
    pool_t  *pool = pool_create(sizeof(node_t), 8, &err);
    ASSERT(pool != NULL, "pool_create succeeds before alloc test");

    /* invalid args */
    err.code = ERROR_OK;
    pool_alloc(NULL, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "pool_alloc on NULL pool sets invalid args error");

    /* basic allocation returns non-NULL and is writable */
    void *p = pool_alloc(pool, &err);
    ASSERT(p != NULL,            "pool_alloc returns non-NULL");
    ASSERT(err.code == ERROR_OK, "pool_alloc sets ERROR_OK");
    memset(p, 0xAB, sizeof(node_t));
    ASSERT(((unsigned char *)p)[0] == 0xAB, "allocated slot is writable");

    /* alignment: every slot must be aligned to max_align_t */
    for (int i = 0; i < 16; i++) {
        void *q = pool_alloc(pool, &err);
        ASSERT(q != NULL, "pool_alloc returns non-NULL on repeated calls");
        ASSERT(((uintptr_t)q % _Alignof(max_align_t)) == 0,
               "pool_alloc returns aligned pointer");
    }

    /* all returned pointers are distinct */
    const int  N    = 32;
    void      *ptrs[32];
    pool_t    *p2   = pool_create(sizeof(node_t), 8, NULL);
    for (int i = 0; i < N; i++) {
        ptrs[i] = pool_alloc(p2, &err);
        ASSERT(ptrs[i] != NULL, "pool_alloc returns non-NULL for each call");
    }
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++)
            ASSERT(ptrs[i] != ptrs[j], "all allocated slots are distinct");
    pool_destroy(p2);

    /* growth: exhaust the first block and confirm alloc still succeeds */
    pool_t *p3 = pool_create(sizeof(node_t), 4, &err);
    for (int i = 0; i < 12; i++) {
        void *s = pool_alloc(p3, &err);
        ASSERT(s != NULL,            "pool_alloc succeeds across block boundaries");
        ASSERT(err.code == ERROR_OK, "no error when new block is allocated");
    }
    pool_destroy(p3);

    /* NULL err pointer is safe */
    void *q = pool_alloc(pool, NULL);
    ASSERT(q != NULL, "pool_alloc works fine with NULL err pointer");

    pool_destroy(pool);
    return 0;
}

/* ---------------------------------------------------------------- */
/* pool_free                                                         */
/* ---------------------------------------------------------------- */

int test_pool_free(void) {
    error_t  err;
    pool_t  *pool = pool_create(sizeof(node_t), 8, &err);
    ASSERT(pool != NULL, "pool_create succeeds before free test");

    /* invalid args */
    err.code = ERROR_OK;
    pool_free(NULL, (void *)0x1, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "pool_free on NULL pool sets invalid args error");

    err.code = ERROR_OK;
    void *p = pool_alloc(pool, &err);
    pool_free(pool, NULL, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "pool_free with NULL ptr sets invalid args error");

    /* freed slot is returned to the pool and reused */
    pool_free(pool, p, &err);
    ASSERT(err.code == ERROR_OK, "pool_free sets ERROR_OK");
    void *q = pool_alloc(pool, &err);
    ASSERT(q == p, "freed slot is reused on next alloc");

    /* free and realloc many times — no leaks, no corruption */
    void *slots[8];
    for (int i = 0; i < 8; i++)
        slots[i] = pool_alloc(pool, &err);
    for (int i = 0; i < 8; i++)
        pool_free(pool, slots[i], &err);
    for (int i = 0; i < 8; i++) {
        slots[i] = pool_alloc(pool, &err);
        ASSERT(slots[i] != NULL, "re-alloc after free succeeds");
    }

    /* writing to a reused slot does not corrupt adjacent slots */
    node_t *a = (node_t *)pool_alloc(pool, &err);
    node_t *b = (node_t *)pool_alloc(pool, &err);
    pool_free(pool, a, &err);
    node_t *c = (node_t *)pool_alloc(pool, &err);
    ASSERT(c == a, "reused slot is the freed one");
    c->id = 42;
    c->x  = 1.0f;
    /* b should be unaffected */
    b->id = 99;
    ASSERT(b->id == 99, "writing to reused slot does not corrupt adjacent slot");
    ASSERT(c->id == 42, "reused slot holds correct value");

    /* NULL err pointer is safe */
    void *r = pool_alloc(pool, NULL);
    pool_free(pool, r, NULL);
    ASSERT(1, "pool_free works fine with NULL err pointer");

    pool_destroy(pool);
    return 0;
}

/* ---------------------------------------------------------------- */
/* pool — realistic usage: XML-node-like structs                     */
/* ---------------------------------------------------------------- */

int test_pool_realistic(void) {
    error_t  err;
    pool_t  *pool = pool_create(sizeof(node_t), 64, &err);
    ASSERT(pool != NULL, "pool_create with realistic slot size succeeds");

    /* allocate 100 nodes, fill them, verify content */
    const int  N = 100;
    node_t    *nodes[100];
    for (int i = 0; i < N; i++) {
        nodes[i] = (node_t *)pool_alloc(pool, &err);
        ASSERT(nodes[i] != NULL,     "node alloc succeeds");
        ASSERT(err.code == ERROR_OK, "no error on node alloc");
        nodes[i]->id = i;
        nodes[i]->x  = (float)i;
        snprintf(nodes[i]->name, sizeof(nodes[i]->name), "node_%d", i);
    }

    /* verify all content is intact after all allocations */
    for (int i = 0; i < N; i++) {
        ASSERT(nodes[i]->id == i,            "node id is intact");
        ASSERT(nodes[i]->x  == (float)i,     "node x is intact");
        char expected[32];
        snprintf(expected, sizeof(expected), "node_%d", i);
        ASSERT(strcmp(nodes[i]->name, expected) == 0, "node name is intact");
    }

    /* free every other node, reallocate, verify the pool reuses slots */
    for (int i = 0; i < N; i += 2)
        pool_free(pool, nodes[i], &err);

    for (int i = 0; i < N; i += 2) {
        nodes[i] = (node_t *)pool_alloc(pool, &err);
        ASSERT(nodes[i] != NULL, "realloc after free succeeds");
        nodes[i]->id = i + 1000;
    }

    for (int i = 0; i < N; i += 2)
        ASSERT(nodes[i]->id == i + 1000, "reallocated node holds new value");

    /* odd nodes (never freed) retain their original content */
    for (int i = 1; i < N; i += 2)
        ASSERT(nodes[i]->id == i, "unfree'd node retains original value");

    pool_destroy(pool);
    return 0;
}

/* ---------------------------------------------------------------- */
/* main                                                              */
/* ---------------------------------------------------------------- */

int main(void) {
    printf(C_BOLD "\ntest_pool\n\n" C_RESET);

    RUN_TEST("pool_create",   test_pool_create);
    RUN_TEST("pool_destroy",  test_pool_destroy);
    RUN_TEST("pool_alloc",    test_pool_alloc);
    RUN_TEST("pool_free",     test_pool_free);
    RUN_TEST("pool_realistic",test_pool_realistic);

    SUMMARY();
    return failed > 0 ? 1 : 0;
}
