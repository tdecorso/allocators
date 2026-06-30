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
#define C_YELLOW  "\033[33m"

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

/* ---------------------------------------------------------------- */
/* arena_create                                                      */
/* ---------------------------------------------------------------- */

int test_arena_create(void) {
    error_t  err;
    arena_t *arena;

    /* valid creation with explicit block size */
    arena = arena_create(1024, &err);
    ASSERT(arena != NULL,        "arena_create with explicit block size returns non-NULL");
    ASSERT(err.code == ERROR_OK, "arena_create sets ERROR_OK");
    arena_destroy(arena);

    /* block_size 0 falls back to the default */
    arena = arena_create(0, &err);
    ASSERT(arena != NULL,        "arena_create with block_size 0 returns non-NULL");
    ASSERT(err.code == ERROR_OK, "arena_create with block_size 0 sets ERROR_OK");
    arena_destroy(arena);

    /* NULL err pointer is safe */
    arena = arena_create(1024, NULL);
    ASSERT(arena != NULL, "arena_create works fine with NULL err pointer");
    arena_destroy(arena);

    return 0;
}

/* ---------------------------------------------------------------- */
/* arena_destroy                                                     */
/* ---------------------------------------------------------------- */

int test_arena_destroy(void) {
    /* NULL is safe */
    arena_destroy(NULL);
    ASSERT(1, "arena_destroy on NULL does not crash");

    /* destroy an arena that never allocated anything */
    error_t  err;
    arena_t *arena = arena_create(1024, &err);
    ASSERT(arena != NULL, "arena_create succeeds before destroy test");
    arena_destroy(arena);
    ASSERT(1, "arena_destroy on empty arena does not crash");

    /* destroy an arena that allocated several blocks */
    arena = arena_create(64, &err);
    ASSERT(arena != NULL, "arena_create with small block size succeeds");
    /* force multiple block allocations */
    for (int i = 0; i < 10; i++)
        arena_alloc(arena, 32, &err);
    arena_destroy(arena);
    ASSERT(1, "arena_destroy on multi-block arena does not crash");

    return 0;
}

/* ---------------------------------------------------------------- */
/* arena_reset                                                       */
/* ---------------------------------------------------------------- */

int test_arena_reset(void) {
    error_t  err;
    arena_t *arena = arena_create(256, &err);
    ASSERT(arena != NULL, "arena_create succeeds before reset test");

    /* invalid args */
    err.code = ERROR_OK;
    arena_reset(NULL, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_reset on NULL arena sets invalid args error");

    /* allocate some strings, then reset */
    char *a = arena_strdup(arena, "hello", &err);
    char *b = arena_strdup(arena, "world", &err);
    ASSERT(a != NULL && b != NULL, "strdup succeeds before reset");

    arena_reset(arena, &err);
    ASSERT(err.code == ERROR_OK, "arena_reset sets ERROR_OK");

    /* arena is usable again after reset */
    char *c = arena_strdup(arena, "reused", &err);
    ASSERT(c != NULL,            "strdup succeeds after reset");
    ASSERT(err.code == ERROR_OK, "no error after allocating post-reset");
    ASSERT(strcmp(c, "reused") == 0, "post-reset string has correct content");

    /* NULL err pointer is safe */
    arena_reset(arena, NULL);
    ASSERT(1, "arena_reset works fine with NULL err pointer");

    arena_destroy(arena);
    return 0;
}

/* ---------------------------------------------------------------- */
/* arena_alloc                                                       */
/* ---------------------------------------------------------------- */

int test_arena_alloc(void) {
    error_t  err;
    arena_t *arena = arena_create(256, &err);
    ASSERT(arena != NULL, "arena_create succeeds before alloc test");

    /* invalid args */
    err.code = ERROR_OK;
    arena_alloc(NULL, 8, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_alloc on NULL arena sets invalid args error");

    err.code = ERROR_OK;
    arena_alloc(arena, 0, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_alloc with size 0 sets invalid args error");

    /* basic allocation returns non-NULL and is writable */
    void *p = arena_alloc(arena, 16, &err);
    ASSERT(p != NULL,            "arena_alloc returns non-NULL");
    ASSERT(err.code == ERROR_OK, "arena_alloc sets ERROR_OK");
    memset(p, 0xAB, 16);        /* write to confirm it is usable memory */
    ASSERT(((unsigned char*)p)[0]  == 0xAB, "allocated memory is writable");
    ASSERT(((unsigned char*)p)[15] == 0xAB, "allocated memory is fully writable");

    /* alignment: every pointer must be aligned to max_align_t */
    for (int i = 1; i <= 9; i++) {
        void *q = arena_alloc(arena, (size_t)i, &err);
        ASSERT(q != NULL, "arena_alloc returns non-NULL for odd sizes");
        ASSERT(((uintptr_t)q % _Alignof(max_align_t)) == 0,
               "arena_alloc returns aligned pointer");
    }

    /* consecutive allocations do not overlap */
    char *s1 = (char*)arena_alloc(arena, 8, &err);
    char *s2 = (char*)arena_alloc(arena, 8, &err);
    ASSERT(s1 != NULL && s2 != NULL, "two consecutive allocations succeed");
    ASSERT(s2 >= s1 + 8,             "consecutive allocations do not overlap");

    /* force a new block: allocate more than remaining capacity */
    void *big = arena_alloc(arena, 200, &err);
    ASSERT(big != NULL,            "allocation that exhausts current block succeeds");
    ASSERT(err.code == ERROR_OK,   "no error when new block is allocated");

    /* oversized allocation: larger than the arena's block size */
    void *huge = arena_alloc(arena, 1024, &err);
    ASSERT(huge != NULL,           "oversized allocation succeeds");
    ASSERT(err.code == ERROR_OK,   "no error on oversized allocation");
    memset(huge, 0, 1024);
    ASSERT(1, "oversized allocation is fully writable");

    /* NULL err pointer is safe */
    void *q = arena_alloc(arena, 8, NULL);
    ASSERT(q != NULL, "arena_alloc works fine with NULL err pointer");

    arena_destroy(arena);
    return 0;
}

/* ---------------------------------------------------------------- */
/* arena_strdup                                                      */
/* ---------------------------------------------------------------- */

int test_arena_strdup(void) {
    error_t  err;
    arena_t *arena = arena_create(256, &err);
    ASSERT(arena != NULL, "arena_create succeeds before strdup test");

    /* invalid args */
    err.code = ERROR_OK;
    arena_strdup(NULL, "hello", &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_strdup on NULL arena sets invalid args error");

    err.code = ERROR_OK;
    arena_strdup(arena, NULL, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_strdup with NULL string sets invalid args error");

    /* basic correctness */
    char *a = arena_strdup(arena, "hello", &err);
    ASSERT(a != NULL,               "arena_strdup returns non-NULL");
    ASSERT(err.code == ERROR_OK,    "arena_strdup sets ERROR_OK");
    ASSERT(strcmp(a, "hello") == 0, "arena_strdup content matches source");
    ASSERT(strlen(a) == 5,          "arena_strdup length matches source");

    /* empty string */
    char *e = arena_strdup(arena, "", &err);
    ASSERT(e != NULL,          "arena_strdup of empty string returns non-NULL");
    ASSERT(strlen(e) == 0,     "arena_strdup of empty string has length 0");
    ASSERT(e[0] == '\0',       "arena_strdup of empty string is null-terminated");

    /* two copies of the same string are independent */
    char *b = arena_strdup(arena, "hello", &err);
    ASSERT(b != NULL,               "second strdup of same string succeeds");
    ASSERT(strcmp(b, "hello") == 0, "second strdup content is correct");
    ASSERT(a != b,                  "two strdup calls return different pointers");

    /* long string that may span a block boundary */
    char   long_str[512];
    memset(long_str, 'x', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';
    char *l = arena_strdup(arena, long_str, &err);
    ASSERT(l != NULL,                    "arena_strdup of long string succeeds");
    ASSERT(strcmp(l, long_str) == 0,     "arena_strdup of long string content matches");
    ASSERT(strlen(l) == sizeof(long_str) - 1, "arena_strdup of long string length matches");

    /* NULL err pointer is safe */
    char *n = arena_strdup(arena, "test", NULL);
    ASSERT(n != NULL, "arena_strdup works fine with NULL err pointer");

    arena_destroy(arena);
    return 0;
}

/* ---------------------------------------------------------------- */
/* arena_strndup                                                     */
/* ---------------------------------------------------------------- */

int test_arena_strndup(void) {
    error_t  err;
    arena_t *arena = arena_create(256, &err);
    ASSERT(arena != NULL, "arena_create succeeds before strndup test");

    /* invalid args */
    err.code = ERROR_OK;
    arena_strndup(NULL, "hello", 3, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_strndup on NULL arena sets invalid args error");

    err.code = ERROR_OK;
    arena_strndup(arena, NULL, 3, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS, "arena_strndup with NULL string sets invalid args error");

    /* n shorter than string: copies only n bytes */
    char *a = arena_strndup(arena, "hello", 3, &err);
    ASSERT(a != NULL,             "arena_strndup with n < len returns non-NULL");
    ASSERT(err.code == ERROR_OK,  "arena_strndup sets ERROR_OK");
    ASSERT(strcmp(a, "hel") == 0, "arena_strndup truncates to n bytes");
    ASSERT(strlen(a) == 3,        "arena_strndup result has length n");
    ASSERT(a[3] == '\0',          "arena_strndup result is null-terminated");

    /* n equal to string length: copies the whole string */
    char *b = arena_strndup(arena, "hello", 5, &err);
    ASSERT(b != NULL,               "arena_strndup with n == len returns non-NULL");
    ASSERT(strcmp(b, "hello") == 0, "arena_strndup with n == len copies full string");

    /* n longer than string: stops at null terminator */
    char *c = arena_strndup(arena, "hi", 100, &err);
    ASSERT(c != NULL,           "arena_strndup with n > len returns non-NULL");
    ASSERT(strcmp(c, "hi") == 0,"arena_strndup with n > len stops at null terminator");
    ASSERT(strlen(c) == 2,      "arena_strndup with n > len result has correct length");

    /* n == 0: result is an empty string */
    char *z = arena_strndup(arena, "hello", 0, &err);
    ASSERT(z != NULL,      "arena_strndup with n == 0 returns non-NULL");
    ASSERT(strlen(z) == 0, "arena_strndup with n == 0 result is empty");
    ASSERT(z[0] == '\0',   "arena_strndup with n == 0 result is null-terminated");

    /* NULL err pointer is safe */
    char *n = arena_strndup(arena, "test", 2, NULL);
    ASSERT(n != NULL, "arena_strndup works fine with NULL err pointer");

    arena_destroy(arena);
    return 0;
}

/* ---------------------------------------------------------------- */
/* main                                                              */
/* ---------------------------------------------------------------- */

int main(void) {
    printf(C_BOLD "\ntest_arena\n\n" C_RESET);

    RUN_TEST("arena_create",  test_arena_create);
    RUN_TEST("arena_destroy", test_arena_destroy);
    RUN_TEST("arena_reset",   test_arena_reset);
    RUN_TEST("arena_alloc",   test_arena_alloc);
    RUN_TEST("arena_strdup",  test_arena_strdup);
    RUN_TEST("arena_strndup", test_arena_strndup);

    SUMMARY();
    return failed > 0 ? 1 : 0;
}
