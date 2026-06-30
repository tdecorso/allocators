# allocators

A memory allocator library in C99, built on top of [containers](https://github.com/tdecorso/containers).

## Overview

`allocators` provides allocator types designed for workloads that create many small objects with identical lifetimes. All allocators are optimised for bulk allocation and single-shot release rather than individual frees.

This is primarily an instructional project. The goal is to produce clean, readable, well-documented C code rather than a production-grade, performance-optimised library.

## Allocators

| Allocator | Description | Alloc | Reset | Free |
|---|---|---|---|---|
| Arena | Bump allocator with block chaining | O(1)† | O(b)‡ | O(b)‡ |
| String arena | Arena + hash index for string interning | O(n)§ / O(1)¶ | — | O(b)‡ |

† Amortised — a new block is allocated only when the current one is exhausted.  
‡ O(b) where b is the number of blocks allocated.  
§ O(n) where n is the string length — on the first intern of a given string.  
¶ O(1) on subsequent interns of the same string (hash lookup).

## Usage

### Including the library

```c
#include "allocators.h"
```

### Error handling

Follows the same convention as `containers` — every fallible function accepts
a trailing `error_t* err` parameter. Passing `NULL` is always safe.

```c
error_t err;
arena_t* arena = arena_create(0, &err);
if (!arena) {
    fprintf(stderr, "error %d: %s\n", err.code, err.msg);
    return 1;
}
```

### Arena allocator

A bump allocator that dispenses memory from a chain of fixed-size blocks.
All memory is released at once when the arena is destroyed.

```c
arena_t* arena = arena_create(0, NULL);   /* 0 = use default block size */

/* allocate raw memory */
float* vec = (float*)arena_alloc(arena, 4 * sizeof(float), NULL);

/* duplicate strings */
const char* tag  = arena_strdup(arena,  "widget",    NULL);
const char* name = arena_strndup(arena, "btn_ok\0x", 6, NULL);

/* rewind without freeing blocks — useful for re-parsing */
arena_reset(arena, NULL);

arena_destroy(arena);
```

Allocations are aligned to `_Alignof(max_align_t)` and are safe for any
scalar or struct type. Passing `size = 0` to `arena_create` uses the default
block size of 4096 bytes. Requests larger than the block size are satisfied
by a dedicated oversized block.

### String arena

A string interning layer built on top of the arena. Equal strings are stored
exactly once; every call to `str_arena_intern` with the same content returns
the same pointer, making pointer equality sufficient for string comparison
after interning.

```c
str_arena_t* sa = str_arena_create(NULL);

const char* a = str_arena_intern(sa, "visible", NULL);
const char* b = str_arena_intern(sa, "visible", NULL);

printf("%d\n", a == b);   /* 1 — same pointer, no strcmp needed */

str_arena_destroy(sa);
```

Internally uses FNV-1a `uint64_t` hashes as hashmap keys, with a `strcmp`
fallback to guard against the negligible probability of hash collisions.

## Building

Requirements: CMake 3.31+, a C99-compatible compiler, and the
[containers](https://github.com/tdecorso/containers) library installed.

```bash
# install containers first (see containers README)
# then build allocators
mkdir build && cd build
cmake ..
cmake --build .
```

To install:

```bash
sudo cmake --install .
```

To link the library into your own CMake project:

```cmake
find_package(allocators 0.1 REQUIRED)
target_link_libraries(my_target PRIVATE allocators::allocators)
```

## Running the tests

After building, run any test binary directly from the build directory.

```bash
./test_arena
./test_str_arena
```

To check for memory leaks with Valgrind:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./test_arena
```

## Documentation

API documentation is generated with [Doxygen](https://www.doxygen.nl) and published at
**[tdecorso.github.io/allocators](https://tdecorso.github.io/allocators/)**.

To build it locally:

```bash
doxygen Doxyfile
```

Then open `docs/html/index.html` in your browser.

## Notes and limitations

- **Not thread-safe.** External synchronisation is required for concurrent access to any allocator.
- **No individual frees.** Arena memory is released all at once on destroy or reset. Use a general-purpose allocator if per-object lifetime is needed.
- **No cycle detection.** Not applicable here, but noted for consistency with the containers library.
- **Early development.** Interfaces may evolve as additional allocators are introduced.

## License

Released under the [MIT License](LICENSE). Copyright © 2026 tdecorso.
