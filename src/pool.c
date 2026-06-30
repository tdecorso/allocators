#include <allocators.h>
#include <string.h>

/* ── Internal types ────────────────────────────────────────────────────────── */

typedef struct pool_block {
    struct pool_block *next; /* next (older) block in the chain, or NULL */
} pool_block_t;

struct pool {
    pool_block_t *head;        /* current (newest) block, or NULL if empty */
    void         *free_list;   /* head of the embedded free list, or NULL  */
    size_t        slot_size;   /* aligned size of each slot in bytes       */
    size_t        block_slots; /* number of slots per block                */
};

/* ── Internal helpers ──────────────────────────────────────────────────────── */

static void error_create(error_t *err, error_e code, const char *msg) {
    if (!err) return;
    err->code = code;
    size_t len = strlen(msg);
    memcpy(err->msg, msg, len);
    err->msg[len] = '\0';
}

static size_t align_up(size_t size) {
    const size_t align = _Alignof(max_align_t);
    return (size + align - 1) & ~(align - 1);
}

/*
 * Allocates a new block, carves it into slots, and pushes every slot onto
 * the pool's free list. The block header lives at the start of the
 * allocation; slots follow immediately after, aligned to max_align_t.
 */
static int block_create(pool_t *pool, error_t *err) {
    size_t header   = align_up(sizeof(pool_block_t));
    size_t total    = header + pool->slot_size * pool->block_slots;
    char  *raw      = malloc(total);
    if (!raw) {
        error_create(err, ERROR_OUT_OF_MEMORY, "Memory allocation failed.");
        return 0;
    }

    pool_block_t *block = (pool_block_t *)raw;
    block->next  = pool->head;
    pool->head   = block;

    /* push slots onto the free list in reverse order so the first slot
     * is at the head — allocation then returns slots in address order */
    char *slots = raw + header;
    for (size_t i = pool->block_slots; i > 0; i--) {
        void *slot  = slots + (i - 1) * pool->slot_size;
        *(void **)slot = pool->free_list;
        pool->free_list = slot;
    }

    return 1;
}

/* ── pool_t ────────────────────────────────────────────────────────────────── */

pool_t *pool_create(size_t slot_size, size_t block_slots, error_t *err) {
    if (slot_size == 0) {
        error_create(err, ERROR_INVALID_ARGS, "Slot size cannot be 0.");
        return NULL;
    }
    if (block_slots == 0) {
        error_create(err, ERROR_INVALID_ARGS, "Block slots cannot be 0.");
        return NULL;
    }

    pool_t *pool = malloc(sizeof(pool_t));
    if (!pool) {
        error_create(err, ERROR_OUT_OF_MEMORY, "Memory allocation failed.");
        return NULL;
    }

    /* slot must be large enough to hold a free-list pointer when unused */
    size_t aligned = align_up(slot_size);
    if (aligned < sizeof(void *)) aligned = sizeof(void *);

    pool->head        = NULL;
    pool->free_list   = NULL;
    pool->slot_size   = aligned;
    pool->block_slots = block_slots;

    error_create(err, ERROR_OK, "No error found.");
    return pool;
}

void pool_destroy(pool_t *pool) {
    if (!pool) return;
    pool_block_t *block = pool->head;
    while (block) {
        pool_block_t *next = block->next;
        free(block);
        block = next;
    }
    free(pool);
}

void *pool_alloc(pool_t *pool, error_t *err) {
    if (!pool) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL pool.");
        return NULL;
    }

    if (!pool->free_list) {
        if (!block_create(pool, err)) return NULL;
    }

    void *slot      = pool->free_list;
    pool->free_list = *(void **)slot;
    error_create(err, ERROR_OK, "No error found.");
    return slot;
}

void pool_free(pool_t *pool, void *ptr, error_t *err) {
    if (!pool) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL pool.");
        return;
    }
    if (!ptr) {
        error_create(err, ERROR_INVALID_ARGS, "You passed a NULL pointer.");
        return;
    }

    *(void **)ptr   = pool->free_list;
    pool->free_list = ptr;
    error_create(err, ERROR_OK, "No error found.");
}
