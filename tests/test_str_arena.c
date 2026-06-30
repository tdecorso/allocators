#include <allocators.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

/* ---------------------------------------------------------------- */
/* str_arena_create                                                  */
/* ---------------------------------------------------------------- */

int test_str_arena_create(void) {
    error_t      err;
    str_arena_t *sa;

    sa = str_arena_create(&err);
    ASSERT(sa != NULL,        "str_arena_create returns non-NULL");
    ASSERT(err.code == ERROR_OK, "str_arena_create sets ERROR_OK");
    str_arena_destroy(sa);

    /* NULL err pointer is safe */
    sa = str_arena_create(NULL);
    ASSERT(sa != NULL, "str_arena_create works fine with NULL err pointer");
    str_arena_destroy(sa);

    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_destroy                                                 */
/* ---------------------------------------------------------------- */

int test_str_arena_destroy(void) {
    /* NULL is safe */
    str_arena_destroy(NULL);
    ASSERT(1, "str_arena_destroy on NULL does not crash");

    /* destroy an arena that never interned anything */
    str_arena_t *sa = str_arena_create(NULL);
    ASSERT(sa != NULL, "str_arena_create succeeds before destroy test");
    str_arena_destroy(sa);
    ASSERT(1, "str_arena_destroy on empty str_arena does not crash");

    /* destroy after interning many strings */
    sa = str_arena_create(NULL);
    ASSERT(sa != NULL, "str_arena_create succeeds before multi-intern destroy test");
    for (int i = 0; i < 100; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "string_%d", i);
        str_arena_intern(sa, buf, NULL);
    }
    str_arena_destroy(sa);
    ASSERT(1, "str_arena_destroy after many interns does not crash");

    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_intern — invalid args                                   */
/* ---------------------------------------------------------------- */

int test_str_arena_intern_invalid(void) {
    error_t      err;
    str_arena_t *sa = str_arena_create(&err);
    ASSERT(sa != NULL, "str_arena_create succeeds before invalid args test");

    err.code = ERROR_OK;
    str_arena_intern(NULL, "hello", &err);
    ASSERT(err.code == ERROR_INVALID_ARGS,
           "str_arena_intern on NULL str_arena sets invalid args error");

    err.code = ERROR_OK;
    str_arena_intern(sa, NULL, &err);
    ASSERT(err.code == ERROR_INVALID_ARGS,
           "str_arena_intern with NULL string sets invalid args error");

    /* NULL err pointer is safe */
    const char *p = str_arena_intern(sa, "hello", NULL);
    ASSERT(p != NULL, "str_arena_intern works fine with NULL err pointer");

    str_arena_destroy(sa);
    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_intern — basic correctness                              */
/* ---------------------------------------------------------------- */

int test_str_arena_intern_basic(void) {
    error_t      err;
    str_arena_t *sa = str_arena_create(&err);
    ASSERT(sa != NULL, "str_arena_create succeeds before basic intern test");

    /* intern a string — returns a valid pointer with correct content */
    const char *a = str_arena_intern(sa, "visible", &err);
    ASSERT(a != NULL,                 "intern returns non-NULL");
    ASSERT(err.code == ERROR_OK,      "intern sets ERROR_OK");
    ASSERT(strcmp(a, "visible") == 0, "interned string has correct content");

    /* empty string */
    const char *e = str_arena_intern(sa, "", &err);
    ASSERT(e != NULL,        "intern of empty string returns non-NULL");
    ASSERT(strlen(e) == 0,   "interned empty string has length 0");
    ASSERT(e[0] == '\0',     "interned empty string is null-terminated");

    /* long string */
    char long_str[512];
    memset(long_str, 'z', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';
    const char *l = str_arena_intern(sa, long_str, &err);
    ASSERT(l != NULL,                    "intern of long string returns non-NULL");
    ASSERT(strcmp(l, long_str) == 0,     "interned long string has correct content");

    str_arena_destroy(sa);
    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_intern — pointer identity (the core guarantee)         */
/* ---------------------------------------------------------------- */

int test_str_arena_intern_identity(void) {
    error_t      err;
    str_arena_t *sa = str_arena_create(&err);
    ASSERT(sa != NULL, "str_arena_create succeeds before identity test");

    /* same string interned twice returns the same pointer */
    const char *a = str_arena_intern(sa, "visible", &err);
    const char *b = str_arena_intern(sa, "visible", &err);
    ASSERT(a != NULL && b != NULL, "both interns return non-NULL");
    ASSERT(a == b, "same string interned twice returns same pointer");

    /* pointer equality replaces strcmp */
    ASSERT(a == b, "pointer equality is sufficient after interning");

    /* different strings return different pointers */
    const char *c = str_arena_intern(sa, "hidden", &err);
    ASSERT(c != NULL,  "different string returns non-NULL");
    ASSERT(a != c,     "different strings return different pointers");
    ASSERT(b != c,     "different strings return different pointers (b vs c)");

    /* intern the same string many times — always the same pointer */
    for (int i = 0; i < 50; i++) {
        const char *r = str_arena_intern(sa, "visible", &err);
        ASSERT(r == a, "repeated intern always returns the same pointer");
    }

    /* interning a copy of the original string (different buffer, same content) */
    char copy[8];
    strcpy(copy, "visible");
    const char *d = str_arena_intern(sa, copy, &err);
    ASSERT(d == a, "intern of a copy returns the same pointer as the original");

    str_arena_destroy(sa);
    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_intern — many distinct strings                          */
/* ---------------------------------------------------------------- */

int test_str_arena_intern_many(void) {
    error_t      err;
    str_arena_t *sa = str_arena_create(&err);
    ASSERT(sa != NULL, "str_arena_create succeeds before many-strings test");

    /* intern 200 distinct strings, store their pointers */
    const int    N = 200;
    const char  *ptrs[200];
    char         bufs[200][32];

    for (int i = 0; i < N; i++) {
        snprintf(bufs[i], sizeof(bufs[i]), "string_%d", i);
        ptrs[i] = str_arena_intern(sa, bufs[i], &err);
        ASSERT(ptrs[i] != NULL,                   "intern succeeds for each string");
        ASSERT(strcmp(ptrs[i], bufs[i]) == 0,     "interned content is correct");
        ASSERT(err.code == ERROR_OK,               "no error on each intern");
    }

    /* re-intern all 200 — each must return the original pointer */
    for (int i = 0; i < N; i++) {
        const char *r = str_arena_intern(sa, bufs[i], &err);
        ASSERT(r == ptrs[i], "re-intern returns the original pointer");
    }

    /* all pointers are distinct */
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            ASSERT(ptrs[i] != ptrs[j],
                   "distinct strings have distinct interned pointers");
        }
    }

    str_arena_destroy(sa);
    return 0;
}

/* ---------------------------------------------------------------- */
/* str_arena_intern — XML-realistic strings                         */
/* ---------------------------------------------------------------- */

int test_str_arena_intern_xml(void) {
    error_t      err;
    str_arena_t *sa = str_arena_create(&err);
    ASSERT(sa != NULL, "str_arena_create succeeds before XML strings test");

    /* typical XML tag names and attribute names */
    const char *tag_widget  = str_arena_intern(sa, "widget",  &err);
    const char *tag_panel   = str_arena_intern(sa, "panel",   &err);
    const char *attr_id     = str_arena_intern(sa, "id",      &err);
    const char *attr_vis    = str_arena_intern(sa, "visible", &err);
    const char *ns_uri      = str_arena_intern(sa,
        "http://www.w3.org/2001/XMLSchema", &err);

    ASSERT(tag_widget != NULL && tag_panel != NULL, "tag names interned");
    ASSERT(attr_id    != NULL && attr_vis  != NULL, "attr names interned");
    ASSERT(ns_uri     != NULL,                      "namespace URI interned");

    /* re-intern the same names — pointer equality holds */
    ASSERT(str_arena_intern(sa, "widget",  &err) == tag_widget,
           "re-intern 'widget' returns same pointer");
    ASSERT(str_arena_intern(sa, "visible", &err) == attr_vis,
           "re-intern 'visible' returns same pointer");
    ASSERT(str_arena_intern(sa,
        "http://www.w3.org/2001/XMLSchema", &err) == ns_uri,
           "re-intern namespace URI returns same pointer");

    /* simulate 50 widget elements each with a 'visible' attribute —
     * the intern should allocate 'visible' only once */
    const char *first = str_arena_intern(sa, "visible", NULL);
    for (int i = 0; i < 50; i++) {
        const char *v = str_arena_intern(sa, "visible", &err);
        ASSERT(v == first,
               "repeated 'visible' interns return the same pointer");
    }

    str_arena_destroy(sa);
    return 0;
}

/* ---------------------------------------------------------------- */
/* main                                                              */
/* ---------------------------------------------------------------- */

int main(void) {
    printf(C_BOLD "\ntest_str_arena\n\n" C_RESET);

    RUN_TEST("str_arena_create",          test_str_arena_create);
    RUN_TEST("str_arena_destroy",         test_str_arena_destroy);
    RUN_TEST("str_arena_intern/invalid",  test_str_arena_intern_invalid);
    RUN_TEST("str_arena_intern/basic",    test_str_arena_intern_basic);
    RUN_TEST("str_arena_intern/identity", test_str_arena_intern_identity);
    RUN_TEST("str_arena_intern/many",     test_str_arena_intern_many);
    RUN_TEST("str_arena_intern/xml",      test_str_arena_intern_xml);

    SUMMARY();
    return failed > 0 ? 1 : 0;
}
