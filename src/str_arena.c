#include <allocators.h>
#include <string.h>

struct str_arena {
    arena_t    *arena; /* backing bump allocator (owned) */
    hashmap_t  *index; /* uint64_t hash → char* interned pointer (owned) */
};

static void error_create(error_t *err, error_e code, const char *msg) {
    if (!err) return;
    err->code = code;
    size_t len = strlen(msg);
    memcpy(err->msg, msg, len);
    err->msg[len] = '\0';
}

/*
 * FNV-1a 64-bit hash over a null-terminated string.
 * Produces the uint64_t key used to index into the hashmap.
 */
static uint64_t fnv1a_64(const char *s) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    while (*s) {
        hash ^= (uint8_t)*s++;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

str_arena_t *str_arena_create(error_t *err) {
    str_arena_t *sa = malloc(sizeof(str_arena_t));
    if (!sa) {
        error_create(err, ERROR_OUT_OF_MEMORY, "Memory allocation failed.");
        return NULL;
    }

    sa->arena = arena_create(0, err);
    if (!sa->arena) {
        free(sa);
        return NULL;
    }

    sa->index = hashmap_create(
        sizeof(uint64_t), sizeof(char *), 0, hash_int64, cmp_bytes, err);
    if (!sa->index) {
        arena_destroy(sa->arena);
        free(sa);
        return NULL;
    }

    error_create(err, ERROR_OK, "No error found.");
    return sa;
}

void str_arena_destroy(str_arena_t *sa) {
    if (!sa) return;
    hashmap_destroy(sa->index);
    arena_destroy(sa->arena);
    free(sa);
}

const char *str_arena_intern(str_arena_t *sa, const char *s, error_t *err) {
    if (!sa) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL str_arena.");
        return NULL;
    }
    if (!s) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL string.");
        return NULL;
    }

    uint64_t key = fnv1a_64(s);
    error_t  lookup_err;
    char    *interned = NULL;

    hashmap_get(sa->index, &key, &interned, &lookup_err);

    if (lookup_err.code == ERROR_OK) {
        /*
         * Hash hit — but two distinct strings could share a hash (collision).
         * Confirm with strcmp before returning the stored pointer.
         */
        if (strcmp(interned, s) == 0) {
            error_create(err, ERROR_OK, "No error found.");
            return interned;
        }
        /*
         * Genuine hash collision: different string, same uint64_t key.
         * Fall through to intern the new string. The hashmap_insert below
         * will overwrite the old entry for this key — acceptable given the
         * astronomical improbability of this path in practice.
         */
    }

    /* Miss — copy the string into the arena and record the pointer */
    interned = arena_strdup(sa->arena, s, err);
    if (!interned) return NULL;

    hashmap_insert(sa->index, &key, &interned, err);
    if (err && err->code != ERROR_OK) return NULL;

    error_create(err, ERROR_OK, "No error found.");
    return interned;
}
