#include <allocators.h>
#include <string.h>

/* ── Internal types ────────────────────────────────────────────────────────── */

typedef struct arena_block {
    struct arena_block *next;     /* next (older) block in the chain, or NULL */
    size_t              capacity; /* total usable bytes in buf                */
    size_t              used;     /* bytes already dispensed from buf         */
    _Alignas(max_align_t) char buf[]; /* usable memory, aligned for any type */
} arena_block_t;

struct arena {
    arena_block_t *head;        /* current (newest) block, or NULL if empty */
    size_t         block_size;  /* capacity of each new block in bytes      */
    size_t         total_alloc; /* total bytes dispensed (diagnostic)       */
};
/* ── Internal helpers ──────────────────────────────────────────────────────── */

static void error_create(error_t *err, error_e code, const char *msg) {
    if (!err) return;
    err->code = code;
    size_t len = strlen(msg);
    memcpy(err->msg, msg, len);
    err->msg[len] = '\0';
}

/*
 * Rounds `size` up to the next multiple of `_Alignof(max_align_t)` so
 * that every allocation returned by arena_alloc is suitably aligned for
 * any scalar or struct type.
 */
static size_t align_up(size_t size) {
    const size_t align = _Alignof(max_align_t);
    return (size + align - 1) & ~(align - 1);
}

/*
 * Allocates a new block with at least `capacity` usable bytes and prepends
 * it to the arena's block chain.
 */
static arena_block_t *block_create(arena_t *arena, size_t capacity,
                                   error_t *err) {
    arena_block_t *block = malloc(sizeof(arena_block_t) + capacity);
    if (!block) {
        error_create(err, ERROR_OUT_OF_MEMORY, "Memory allocation failed.");
        return NULL;
    }
    block->capacity = capacity;
    block->used     = 0;
    block->next     = arena->head;
    arena->head     = block;
    return block;
}

/* ── arena_t ───────────────────────────────────────────────────────────────── */

arena_t *arena_create(size_t block_size, error_t *err) {
    arena_t *arena = malloc(sizeof(arena_t));
    if (!arena) {
        error_create(err, ERROR_OUT_OF_MEMORY, "Memory allocation failed.");
        return NULL;
    }
    arena->head        = NULL;
    arena->block_size  = block_size > 0 ? block_size : ARENA_DEFAULT_BLOCK_SIZE;
    arena->total_alloc = 0;
    error_create(err, ERROR_OK, "No error found.");
    return arena;
}

void arena_destroy(arena_t *arena) {
    if (!arena) return;
    arena_block_t *block = arena->head;
    while (block) {
        arena_block_t *next = block->next;
        free(block);
        block = next;
    }
    free(arena);
}

void arena_reset(arena_t *arena, error_t *err) {
    if (!arena) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL arena.");
        return;
    }
    arena_block_t *block = arena->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    arena->total_alloc = 0;
    error_create(err, ERROR_OK, "No error found.");
}

void *arena_alloc(arena_t *arena, size_t size, error_t *err) {
    if (!arena) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL arena.");
        return NULL;
    }
    if (size == 0) {
        error_create(err, ERROR_INVALID_ARGS, "Allocation size cannot be 0.");
        return NULL;
    }

    size_t aligned = align_up(size);

    /* Try the current block first */
    if (arena->head) {
        arena_block_t *block = arena->head;
        if (block->used + aligned <= block->capacity) {
            void *ptr     = block->buf + block->used;
            block->used  += aligned;
            arena->total_alloc += aligned;
            error_create(err, ERROR_OK, "No error found.");
            return ptr;
        }
    }

    /*
     * Current block is exhausted (or no block exists yet).
     * Allocate a new block. If the request is larger than the standard
     * block size, give it a dedicated oversized block so no allocation
     * ever fails due to size.
     */
    size_t capacity = arena->block_size > aligned ? arena->block_size : aligned;
    arena_block_t *block = block_create(arena, capacity, err);
    if (!block) return NULL;

    void *ptr    = block->buf;
    block->used  = aligned;
    arena->total_alloc += aligned;
    error_create(err, ERROR_OK, "No error found.");
    return ptr;
}

char *arena_strdup(arena_t *arena, const char *s, error_t *err) {
    if (!arena) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL arena.");
        return NULL;
    }
    if (!s) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL string.");
        return NULL;
    }
    size_t len = strlen(s);
    char  *dst = (char *)arena_alloc(arena, len + 1, err);
    if (!dst) return NULL;
    memcpy(dst, s, len + 1);
    return dst;
}

char *arena_strndup(arena_t *arena, const char *s, size_t n, error_t *err) {
    if (!arena) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL arena.");
        return NULL;
    }
    if (!s) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL string.");
        return NULL;
    }

    /* Determine actual copy length — whichever comes first: n or '\0' */
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;

    char *dst = (char *)arena_alloc(arena, len + 1, err);
    if (!dst) return NULL;
    memcpy(dst, s, len);
    dst[len] = '\0';
    return dst;
}
