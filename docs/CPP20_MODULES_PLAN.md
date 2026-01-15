# C++20 Modules Migration Plan

**Goal**: Convert EnvPool's modern C++ implementations to C++20 modules with `import std`

**Benefits**:
- Faster compilation (modules are compiled once)
- Better encapsulation (clear interface vs implementation)
- Import std (no header parsing overhead)
- Cleaner dependencies
- Better tooling support

---

## Module Structure Design

### Module Hierarchy

```
envpool                          // Root module
├── envpool.core                 // Core utilities
│   ├── envpool.core.types      // Type utilities, concepts
│   └── envpool.core.errors     // Error types (std::expected)
├── envpool.async                // Async components
│   ├── envpool.async.buffer    // CircularBuffer
│   ├── envpool.async.action    // ActionBufferQueue
│   ├── envpool.async.state     // StateBuffer, StateBufferQueue
│   └── envpool.async.pool      // AsyncEnvPool
└── envpool.env                  // Environments
    └── envpool.env.dummy        // DummyEnv
```

### Module Files Mapping

| Old Header | New Module File | Module Name |
|------------|-----------------|-------------|
| circular_buffer_modern.h | circular_buffer.cppm | envpool.async.buffer |
| action_buffer_queue_modern.h | action_queue.cppm | envpool.async.action |
| state_buffer_modern.h | state_buffer.cppm | envpool.async.state.buffer |
| state_buffer_queue_modern.h | state_queue.cppm | envpool.async.state.queue |
| async_envpool_modern.h | async_pool.cppm | envpool.async.pool |
| dummy_envpool.h | dummy.cppm | envpool.env.dummy |

---

## Iteration 1: Module Foundation

### Goals
1. Convert all modern implementations to C++20 modules
2. Use `import std;` throughout
3. Update CMake with experimental module support
4. Ensure everything compiles

### Tasks

#### 1.1 Core Type Module
Create `envpool/core/types.cppm`:
- Concepts (Environment, Movable, etc.)
- Type utilities
- Common type aliases

#### 1.2 Core Error Module
Create `envpool/core/errors.cppm`:
- BufferError, QueueError, StateBufferError
- Error handling utilities
- std::expected wrappers

#### 1.3 CircularBuffer Module
Create `envpool/async/circular_buffer.cppm`:
- Export CircularBuffer template
- Use `import std;`
- Clean interface

#### 1.4 ActionBufferQueue Module
Create `envpool/async/action_queue.cppm`:
- Export ActionBufferQueue
- Export ActionSlice
- Modern clean code

#### 1.5 StateBuffer Module
Create `envpool/async/state_buffer.cppm`:
- Export StateBuffer
- Export WritableSlice
- RAII patterns

#### 1.6 StateBufferQueue Module
Create `envpool/async/state_queue.cppm`:
- Export StateBufferQueue
- Import circular_buffer module
- Background threads

#### 1.7 AsyncEnvPool Module
Create `envpool/async/async_pool.cppm`:
- Export AsyncEnvPool template
- Import all dependencies
- Complete async pipeline

#### 1.8 DummyEnv Module
Create `envpool/env/dummy.cppm`:
- Export DummyEnv
- Test environment

---

## Iteration 2: Clean Code & Review

### Goals
1. Review every module for clean code principles
2. Improve naming, structure, documentation
3. Eliminate code smells
4. Apply SOLID principles

### Clean Code Checklist

#### Naming
- [ ] All names are intention-revealing
- [ ] No abbreviations (except std abbreviations)
- [ ] Consistent naming conventions
- [ ] No mental mapping required

#### Functions
- [ ] Small functions (<20 lines preferred)
- [ ] Single Responsibility Principle
- [ ] Descriptive names (verbs for actions)
- [ ] Minimal parameters (<3 preferred)
- [ ] No boolean flags (split into separate functions)

#### Classes
- [ ] Single Responsibility Principle
- [ ] Open/Closed Principle
- [ ] Clear interfaces
- [ ] Minimal public surface
- [ ] RAII for all resources

#### Comments
- [ ] Code self-documents (minimal comments needed)
- [ ] Only "why" comments, not "what"
- [ ] Module documentation at top
- [ ] Public API documentation

#### Error Handling
- [ ] std::expected everywhere
- [ ] Clear error types
- [ ] No error codes
- [ ] Explicit error handling

---

## Iteration 3: Final Polish

### Goals
1. Optimize module structure
2. Update all documentation
3. Performance validation
4. Migration guide

### Tasks
- [ ] Module compile time measurement
- [ ] Module interface optimization
- [ ] Documentation updates
- [ ] Migration guide from headers
- [ ] Performance benchmarks
- [ ] Final review

---

## CMake Configuration

### Enable C++20 Modules

```cmake
# Require CMake 3.28+ for module support
cmake_minimum_required(VERSION 3.28)

# Enable C++23/26 with modules
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Experimental import std
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP ON)
```

### Module Targets

```cmake
# Create std module (from compiler)
add_library(std)
target_sources(std
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES std.cppm  # Provided by compiler
)

# Core types module
add_library(envpool_types)
target_sources(envpool_types
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES envpool/core/types.cppm
)
target_link_libraries(envpool_types PUBLIC std)

# CircularBuffer module
add_library(envpool_circular_buffer)
target_sources(envpool_circular_buffer
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES envpool/async/circular_buffer.cppm
)
target_link_libraries(envpool_circular_buffer PUBLIC std envpool_types)
```

---

## Compiler Support

### Requirements

| Compiler | Version | Module Support | import std |
|----------|---------|----------------|------------|
| GCC | 14+ | ✅ Full | ✅ Experimental |
| Clang | 17+ | ✅ Full | ✅ Experimental |
| MSVC | 19.38+ | ✅ Full | ✅ Experimental |

### Build Commands

```bash
# GCC 14
cmake -B build -S . \
  -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_CXX_STANDARD=23

# Clang 17
cmake -B build -S . \
  -DCMAKE_CXX_COMPILER=clang++-17 \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++"

# MSVC
cmake -B build -S . \
  -G "Visual Studio 17 2022" \
  -DCMAKE_CXX_STANDARD=23
```

---

## Migration Strategy

### Phase 1: Parallel Implementation
- Keep existing headers
- Create modules alongside
- Tests use modules
- Validate equivalence

### Phase 2: Gradual Migration
- Update tests to use modules
- Update benchmarks to use modules
- Keep headers for compatibility

### Phase 3: Full Migration
- Remove old headers (optional)
- All code uses modules
- Update documentation

---

## Expected Benefits

### Compilation Time
- **Before (headers)**: Parse std headers every translation unit (~500ms per TU)
- **After (modules)**: Import std module (~50ms per TU)
- **Speedup**: ~10× faster compilation for large projects

### Build Size
- **Before**: Every TU has full std definitions
- **After**: Single compiled module interface
- **Reduction**: ~5× smaller build artifacts

### Errors
- **Before**: Template error in std headers (cryptic)
- **After**: Clean module interface errors
- **Improvement**: Much clearer error messages

---

## Clean Code Principles Applied

### 1. Single Responsibility
Each module has one clear purpose.

### 2. Open/Closed
Modules are open for extension (templates), closed for modification.

### 3. Dependency Inversion
Depend on abstractions (concepts), not concrete types.

### 4. Interface Segregation
Small, focused module interfaces.

### 5. DRY (Don't Repeat Yourself)
Modules eliminate header duplication.

---

## Success Criteria

### Compilation
- [ ] All modules compile with GCC 14+
- [ ] All modules compile with Clang 17+
- [ ] All modules compile with MSVC 19.38+
- [ ] import std works on all platforms

### Performance
- [ ] Module compilation <5s total
- [ ] Runtime performance ≥95% of headers
- [ ] Binary size ≤110% of headers

### Code Quality
- [ ] All modules follow clean code principles
- [ ] <10 lines per function average
- [ ] <200 lines per module interface
- [ ] 100% self-documenting code

### Testing
- [ ] All 32 tests pass with modules
- [ ] All 15 benchmarks pass with modules
- [ ] No regressions

---

## Timeline

### Iteration 1: Foundation (2-3 hours)
- Convert all implementations to modules
- Update CMake
- Get it compiling

### Iteration 2: Clean Code (2-3 hours)
- Review and refactor every module
- Apply clean code principles
- Documentation updates

### Iteration 3: Polish (1-2 hours)
- Final optimizations
- Performance validation
- Migration guide

**Total**: 5-8 hours for complete modernization

---

## References

- **C++20 Modules**: https://en.cppreference.com/w/cpp/language/modules
- **CMake Modules**: https://www.kitware.com/import-cmake-c20-modules/
- **Clean Code**: Robert C. Martin
- **Modern C++**: Effective Modern C++ (Scott Meyers)

---

**Status**: Planning Complete - Ready to Begin Implementation
**Next**: Start Iteration 1 - Module Foundation
