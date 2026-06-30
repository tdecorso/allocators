/**
 * @file allocators.h
 * @brief Memory allocators for efficient bulk allocation and string interning.
 *
 * Provides two allocator types built for workloads that create many small
 * objects with identical lifetimes:
 *
 * - @ref arena_t — a general-purpose bump allocator. Memory is dispensed
 *   from a chain of fixed-size blocks and released all at once on destroy.
 *   Individual allocations cannot be freed.
 *
 * - @ref str_arena_t — a string interning layer built on top of @ref arena_t.
 *   Equal strings are stored exactly once; every call to @ref str_arena_intern
 *   with the same content returns the same pointer, making pointer equality
 *   sufficient for string comparison after interning.
 *
 * Both allocators are designed to be created once, used heavily, and
 * destroyed in a single operation.
 *
 * @note Neither allocator is thread-safe. External synchronisation is
 *       required for concurrent access.
 */

#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include <containers.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup arena Arena allocator
 * @brief General-purpose bump allocator with block chaining.
 *
 * Memory is allocated from a chain of fixed-size blocks. Each allocation
 * advances a pointer within the current block; when a block is exhausted a
 * new one is appended to the chain. All memory is released at once when the
 * arena is destroyed or reset.
 *
 * Allocations are aligned to `_Alignof(max_align_t)` so the arena can safely
 * hold any scalar or struct type, not just strings.
 * @{
 */

/** @brief Default block size in bytes. Matches a typical memory page. */
#define ARENA_DEFAULT_BLOCK_SIZE 4096

/**
 * @brief General-purpose bump allocator.
 *
 * Dispenses memory from a chain of heap-allocated blocks. Allocations are
 * O(1) and individually non-freeable. The entire arena is released in O(b)
 * where b is the number of blocks allocated.
 *
 * The internal layout is opaque — always access through the arena API.
 */
typedef struct arena arena_t;

/**
 * @brief Creates a new empty arena.
 *
 * No blocks are allocated until the first call to @ref arena_alloc,
 * @ref arena_strdup, or @ref arena_strndup.
 *
 * @param block_size Capacity of each block in bytes. Pass 0 to use
 *                   @ref ARENA_DEFAULT_BLOCK_SIZE.
 * @param err        Optional error output. Populated on failure.
 * @return Pointer to the newly created arena, or NULL on failure.
 */
arena_t *arena_create(size_t block_size, error_t *err);

/**
 * @brief Destroys the arena and frees all associated memory.
 *
 * Frees every block in the chain and the arena itself. All pointers
 * previously returned by this arena become invalid.
 *
 * @param arena The arena to destroy. NULL is safe and does nothing.
 */
void arena_destroy(arena_t *arena);

/**
 * @brief Rewinds the arena without freeing its blocks.
 *
 * Resets every block's `used` counter to zero, making all memory available
 * for reuse. Previously returned pointers become invalid. Blocks are not
 * freed, so subsequent allocations can reuse existing capacity without
 * triggering new heap allocations.
 *
 * Useful when the same arena is reused across repeated parses of different
 * documents.
 *
 * @param arena The arena to reset. Must not be NULL.
 * @param err   Optional error output. Populated on failure.
 */
void arena_reset(arena_t *arena, error_t *err);

/**
 * @brief Allocates a block of memory from the arena.
 *
 * The returned pointer is aligned to `_Alignof(max_align_t)`. If `size`
 * exceeds the arena's block size, a dedicated oversized block is allocated
 * to satisfy the request.
 *
 * @param arena The arena. Must not be NULL.
 * @param size  Number of bytes to allocate. Must be > 0.
 * @param err   Optional error output. Populated on failure.
 * @return Pointer to the allocated memory, or NULL on failure.
 *
 * @note The returned memory is not zero-initialised.
 * @note Complexity: O(1) for allocations that fit in the current block;
 *       O(1) amortised overall (a new block is allocated only when the
 *       current one is exhausted).
 */
void *arena_alloc(arena_t *arena, size_t size, error_t *err);

/**
 * @brief Copies a null-terminated string into the arena.
 *
 * Equivalent to `strdup` but backed by the arena. The returned pointer
 * is valid until the arena is destroyed or reset.
 *
 * @param arena The arena. Must not be NULL.
 * @param s     The string to copy. Must not be NULL.
 * @param err   Optional error output. Populated on failure.
 * @return Pointer to the interned copy, or NULL on failure.
 *
 * @note Complexity: O(n) where n is the length of `s`.
 */
char *arena_strdup(arena_t *arena, const char *s, error_t *err);

/**
 * @brief Copies at most `n` bytes of a string into the arena.
 *
 * Copies up to `n` bytes from `s` and appends a null terminator. If `s`
 * is shorter than `n` bytes the copy is terminated at the first null byte.
 * The result is always null-terminated.
 *
 * @param arena The arena. Must not be NULL.
 * @param s     The string to copy. Must not be NULL.
 * @param n     Maximum number of bytes to copy (excluding the null terminator).
 * @param err   Optional error output. Populated on failure.
 * @return Pointer to the interned copy, or NULL on failure.
 *
 * @note Complexity: O(n).
 */
char *arena_strndup(arena_t *arena, const char *s, size_t n, error_t *err);

/// @} // arena

/**
 * @defgroup str_arena String arena
 * @brief String interning allocator built on top of @ref arena_t.
 *
 * Stores each unique string exactly once. Repeated calls to
 * @ref str_arena_intern with equal content return the same pointer, so
 * pointer equality is sufficient for string comparison after interning.
 *
 * Internally uses an @ref arena_t for string storage and a @ref hashmap_t
 * keyed on FNV-1a `uint64_t` hashes for O(1) lookup. A `strcmp` fallback
 * guards against the negligible probability of hash collisions.
 * @{
 */

/**
 * @brief String interning allocator.
 *
 * Owns a backing bump allocator for storage and a hash index for O(1)
 * lookup. Both are created and destroyed together with the str_arena.
 *
 * The internal layout is opaque — always access through the str_arena API.
 */
typedef struct str_arena str_arena_t;

/**
 * @brief Creates a new empty string arena.
 *
 * @param err Optional error output. Populated on failure.
 * @return Pointer to the newly created string arena, or NULL on failure.
 */
str_arena_t *str_arena_create(error_t *err);

/**
 * @brief Destroys the string arena and frees all associated memory.
 *
 * All pointers previously returned by @ref str_arena_intern become invalid.
 *
 * @param sa The string arena to destroy. NULL is safe and does nothing.
 */
void str_arena_destroy(str_arena_t *sa);

/**
 * @brief Interns a string, returning a stable pointer to its unique copy.
 *
 * If `s` has been interned before, the previously stored pointer is returned
 * and no allocation occurs. Otherwise `s` is copied into the backing arena
 * and the new pointer is recorded in the index.
 *
 * The returned pointer is valid until the string arena is destroyed.
 *
 * @param sa  The string arena. Must not be NULL.
 * @param s   The string to intern. Must not be NULL.
 * @param err Optional error output. Populated on failure.
 * @return Stable pointer to the interned copy of `s`, or NULL on failure.
 *
 * @note Complexity: O(n) on the first call for a given string (n = length);
 *       O(1) on subsequent calls for the same content.
 */
const char *str_arena_intern(str_arena_t *sa, const char *s, error_t *err);

/// @} // str_arena

/**
 * @defgroup pool Pool allocator
 * @brief Fixed-size slot allocator with O(1) alloc and free.
 *
 * Memory is divided into fixed-size slots organised in a chain of
 * heap-allocated blocks. Free slots are linked via an embedded free list
 * so both allocation and individual deallocation are O(1).
 *
 * When all slots in the current block are exhausted a new block is
 * allocated and chained automatically. The pool never fails due to
 * capacity — only due to out-of-memory conditions.
 *
 * All slots are the same size, set at creation time. The pool is raw
 * (`void*`) — the caller is responsible for casting to the correct type.
 *
 * @note The pool has no way to verify that a pointer passed to
 *       @ref pool_free actually originated from this pool. Passing
 *       an invalid pointer is undefined behaviour.
 * @{
 */

/**
 * @brief Fixed-size slot allocator.
 *
 * The internal layout is opaque — always access through the pool API.
 */
typedef struct pool pool_t;

/**
 * @brief Creates a new empty pool.
 *
 * No blocks are allocated until the first call to @ref pool_alloc.
 *
 * @param slot_size   Size in bytes of each slot. Must be > 0. Internally
 *                    rounded up to `_Alignof(max_align_t)` and to at least
 *                    `sizeof(void*)` to accommodate the embedded free list.
 * @param block_slots Number of slots per block. Must be > 0. Controls the
 *                    granularity of growth — larger values reduce block
 *                    allocations at the cost of higher peak memory.
 * @param err         Optional error output. Populated on failure.
 * @return Pointer to the newly created pool, or NULL on failure.
 */
pool_t *pool_create(size_t slot_size, size_t block_slots, error_t *err);

/**
 * @brief Destroys the pool and frees all associated memory.
 *
 * Frees every block in the chain and the pool itself. All pointers
 * previously returned by @ref pool_alloc become invalid, whether or not
 * they have been returned via @ref pool_free.
 *
 * @param pool The pool to destroy. NULL is safe and does nothing.
 */
void pool_destroy(pool_t *pool);

/**
 * @brief Allocates one slot from the pool.
 *
 * Returns a pointer to the first free slot. If no free slots are
 * available a new block is allocated and chained before returning.
 *
 * @param pool The pool. Must not be NULL.
 * @param err  Optional error output. Populated on failure.
 * @return Pointer to the allocated slot, or NULL on failure.
 *
 * @note The returned memory is not zero-initialised.
 * @note Complexity: O(1) when a free slot is available; O(s) amortised
 *       overall where s is the number of slots per block.
 */
void *pool_alloc(pool_t *pool, error_t *err);

/**
 * @brief Returns a slot to the pool.
 *
 * Pushes `ptr` back onto the free list, making it available for a
 * future @ref pool_alloc call. The memory is not zeroed.
 *
 * @param pool The pool. Must not be NULL.
 * @param ptr  Slot to return. Must not be NULL. Must have been returned
 *             by a prior call to @ref pool_alloc on this pool.
 * @param err  Optional error output. Populated on failure.
 *
 * @warning Passing a pointer that did not originate from this pool is
 *          undefined behaviour. No validation is performed.
 * @note Complexity: O(1).
 */
void pool_free(pool_t *pool, void *ptr, error_t *err);


#endif /* ALLOCATORS_H */
