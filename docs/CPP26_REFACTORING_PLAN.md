# C++26 Refactoring Plan for EnvPool Core

## Executive Summary

This document outlines a comprehensive plan to refactor EnvPool's core async components using modern C++26 features and idioms. The goals are:

1. **Improve Readability**: Make code more human-readable and maintainable
2. **Enhance Safety**: Leverage C++26 type safety and lifetime management
3. **Maintain Performance**: Preserve or improve current performance characteristics
4. **Modernize Idioms**: Use contemporary C++ best practices
5. **Better Tooling**: Enable better static analysis and IDE support

**Critical Constraint**: NO performance regressions. Micro-benchmarks required for every change.

---

## C++26 Features to Leverage

### Core Language Features

1. **std::expected<T, E>** (C++23, available in C++26)
   - Replace exceptions with expected/unexpected
   - Better error handling without performance cost
   - Example: `StateBuffer::Allocate()` returns `expected<WritableSlice, AllocationError>`

2. **std::mdspan** (C++23)
   - Multi-dimensional array views
   - Replace custom Array slicing with standard mdspan
   - Zero-cost abstraction

3. **Deducing this** (C++23)
   - Eliminate CRTP boilerplate
   - Cleaner template code

4. **Pattern Matching with `if constexpr` and Concepts**
   - Replace SFINAE with concepts
   - More readable template constraints

5. **std::atomic improvements** (C++20+)
   - `std::atomic_ref` for non-atomic objects
   - Atomic wait/notify operations
   - Better memory ordering control

6. **Coroutines** (C++20, mature in C++26)
   - **Potentially** for async operations
   - Need careful analysis (overhead concerns)

7. **std::jthread** (C++20)
   - RAII thread management
   - Automatic stop_token integration
   - Replace manual thread + stop flag

8. **Ranges and Views** (C++20/23)
   - Replace manual loops with ranges
   - More expressive, composable code

9. **std::span** (C++20)
   - Replace pointer + size pairs
   - Bounds-checked views

10. **constexpr everything**
    - More compile-time computation
    - Better optimization opportunities

11. **Modules** (C++20, stable in C++26)
    - Faster compilation
    - Better dependency management
    - Replace header guards

### Library Features

1. **std::execution** (C++23/26)
   - Standard parallelism utilities
   - May replace custom ThreadPool

2. **std::hazard_pointer** (C++26)
   - Safe memory reclamation in lock-free structures
   - May improve buffer recycling

3. **std::latch / std::barrier** (C++20)
   - Better synchronization primitives
   - May simplify some coordination

4. **std::counting_semaphore** (C++20)
   - Standard semaphore
   - May replace moodycamel::LightweightSemaphore (benchmark required!)

---

## Refactoring Phases

### Phase 0: Preparation (Week 1-2)

**Objectives**:
- Establish comprehensive test suite
- Create performance baselines
- Set up refactoring infrastructure

**Tasks**:
1. ✅ Complete test coverage analysis
2. ⬜ Implement missing critical tests (from TEST_COVERAGE_ANALYSIS.md)
3. ⬜ Create micro-benchmarks for each component
4. ⬜ Document performance baselines
5. ⬜ Set up C++26 compiler (GCC 14+ or Clang 18+)
6. ⬜ Configure build system for C++26
7. ⬜ Create feature branch: `refactor/cpp26-core`

**Success Criteria**:
- All tests passing
- Benchmarks documented
- C++26 toolchain working

---

### Phase 1: Type Safety & Error Handling (Week 3-4)

**Objective**: Replace exceptions and raw pointers with modern alternatives.

#### 1.1 Replace Exceptions with std::expected

**Current Code** (state_buffer.h:113):
```cpp
WritableSlice Allocate(std::size_t num_players, int order = -1) {
  std::size_t alloc_count = alloc_count_.fetch_add(1);
  if (alloc_count < batch_) {
    // ... allocation logic
    return WritableSlice{...};
  }
  throw std::out_of_range("StateBuffer out of storage");
}
```

**Refactored**:
```cpp
enum class AllocationError {
  OutOfStorage,
  InvalidPlayerCount,
  BufferFull
};

std::expected<WritableSlice, AllocationError> Allocate(
    std::size_t num_players,
    int order = -1) noexcept {

  if (num_players == 0 || num_players > max_num_players_) {
    return std::unexpected(AllocationError::InvalidPlayerCount);
  }

  std::size_t alloc_count = alloc_count_.fetch_add(1, std::memory_order_acquire);

  if (alloc_count >= batch_) {
    // Rollback allocation
    alloc_count_.fetch_sub(1, std::memory_order_release);
    return std::unexpected(AllocationError::OutOfStorage);
  }

  // ... allocation logic
  return WritableSlice{...};
}
```

**Benefits**:
- No exception overhead
- Explicit error handling at call sites
- Better composability
- Forces error consideration

**Performance Impact**: Measure with benchmark (expected: neutral or faster)

---

#### 1.2 Replace std::function with Concepts

**Current Code** (state_buffer.h:61):
```cpp
struct WritableSlice {
  std::vector<Array> arr;
  std::function<void()> done_write;  // Heap allocation!
};
```

**Refactored**:
```cpp
template <typename F>
concept DoneCallback = requires(F f) {
  { f() } -> std::same_as<void>;
};

template <DoneCallback Callback>
struct WritableSlice {
  std::vector<Array> arr;
  Callback done_write;  // No heap allocation
};

// Alternative: use std::move_only_function (C++23)
struct WritableSlice {
  std::vector<Array> arr;
  std::move_only_function<void()> done_write;  // Better than std::function
};
```

**Benefits**:
- Eliminate heap allocation from std::function
- Better inlining opportunities
- Type-safe callbacks

**Performance Impact**: Benchmark (expected: 5-10% faster allocation)

---

#### 1.3 Replace Raw Pointers with std::span

**Current Code** (various places):
```cpp
int* env_id = static_cast<int*>(action[0].Data());
```

**Refactored**:
```cpp
std::span<int> env_ids = action[0].AsSpan<int>();
// Bounds checking in debug builds
// Zero overhead in release builds
```

**Benefits**:
- Bounds checking
- Iterator support
- Clear ownership semantics

---

### Phase 2: Concurrency Modernization (Week 5-7)

**Objective**: Use modern C++ concurrency primitives.

#### 2.1 Replace Manual Thread + Stop Flag with std::jthread

**Current Code** (async_envpool.h:118-129):
```cpp
std::atomic<int> stop_;
std::vector<std::thread> workers_;

// Constructor
for (std::size_t i = 0; i < num_threads_; ++i) {
  workers_.emplace_back([this] {
    for (;;) {
      ActionSlice raw_action = action_buffer_queue_->Dequeue();
      if (stop_ == 1) break;
      // ... work
    }
  });
}

// Destructor
stop_ = 1;
action_buffer_queue_->EnqueueBulk(empty_actions);
for (auto& worker : workers_) {
  worker.join();
}
```

**Refactored**:
```cpp
std::vector<std::jthread> workers_;

// Constructor
for (std::size_t i = 0; i < num_threads_; ++i) {
  workers_.emplace_back([this](std::stop_token stoken) {
    while (!stoken.stop_requested()) {
      auto result = action_buffer_queue_->TryDequeue(std::chrono::milliseconds(100));
      if (!result) continue;  // Timeout, check stop_token

      ActionSlice raw_action = *result;
      // ... work
    }
  });
}

// Destructor: automatic join and stop notification!
~AsyncEnvPool() = default;
```

**Benefits**:
- RAII: automatic join on destruction
- Cooperative cancellation via stop_token
- Cleaner shutdown logic
- No manual stop flag

**Performance Impact**: Benchmark (expected: neutral, may reduce shutdown latency)

---

#### 2.2 Replace Semaphore with std::counting_semaphore

**Current Code** (circular_buffer.h:38-39):
```cpp
moodycamel::LightweightSemaphore sem_get_;
moodycamel::LightweightSemaphore sem_put_;
```

**Refactored**:
```cpp
std::counting_semaphore<> sem_get_{0};
std::counting_semaphore<> sem_put_{size_};

void Put(T&& v) {
  sem_put_.acquire();  // Wait for space
  uint64_t tail = tail_.fetch_add(1, std::memory_order_relaxed);
  buffer_[tail % size_] = std::forward<T>(v);
  sem_get_.release();  // Signal item available
}

V Get() {
  sem_get_.acquire();  // Wait for item
  uint64_t head = head_.fetch_add(1, std::memory_order_relaxed);
  V v = std::move(buffer_[head % size_]);
  sem_put_.release();  // Signal space available
  return v;
}
```

**Critical**: Benchmark against moodycamel::LightweightSemaphore!
- LightweightSemaphore is highly optimized
- std::counting_semaphore may be slower on some platforms
- If slower, keep LightweightSemaphore

**Performance Impact**: **MUST BENCHMARK** (may be 10-50% slower depending on platform)

---

#### 2.3 Improve Memory Ordering

**Current Code**: Uses default memory ordering (sequential consistency)
```cpp
uint64_t pos = alloc_ptr_.fetch_add(action.size());  // seq_cst
```

**Refactored**: Use explicit, minimal memory ordering
```cpp
// Producer: use relaxed for counter, release for synchronization
uint64_t pos = alloc_ptr_.fetch_add(action.size(), std::memory_order_relaxed);
// ... write data ...
sem_.signal();  // Semaphore provides release-acquire sync

// Consumer: use acquire for synchronization
auto ptr = done_ptr_.fetch_add(1, std::memory_order_relaxed);
auto ret = queue_[ptr % queue_size_];  // Read after acquire
```

**Benefits**:
- Relaxed ordering cheaper on ARM/weak memory model architectures
- Makes synchronization intent explicit
- Potential 5-15% performance improvement on ARM

**Performance Impact**: Benchmark on x86 and ARM

---

#### 2.4 Replace Dual-Counter Atomic Trick with std::atomic<std::pair>

**Current Code** (state_buffer.h:87-88):
```cpp
uint64_t increment = static_cast<uint64_t>(num_players) << 32 | 1;
uint64_t offsets = offsets_.fetch_add(increment);
```

**Refactored** (C++26 with atomic pair support):
```cpp
struct Offsets {
  uint32_t player_offset;
  uint32_t shared_offset;

  auto operator<=>(const Offsets&) const = default;
};

// C++26 allows atomic<Offsets> if Offsets is trivially copyable
std::atomic<Offsets> offsets_{{0, 0}};

WritableSlice Allocate(std::size_t num_players, int order = -1) {
  // ... allocation count check ...

  Offsets old = offsets_.fetch_add(
    Offsets{num_players, 1},
    std::memory_order_acquire
  );

  uint32_t player_offset = old.player_offset;
  uint32_t shared_offset = old.shared_offset;

  // ... rest of allocation ...
}
```

**Benefits**:
- More readable (no bit manipulation)
- Type-safe
- Self-documenting

**Performance Impact**: Likely neutral (still single atomic op), benchmark required

**Alternative**: Keep current implementation if clearer (it's actually quite elegant)

---

### Phase 3: Data Structures (Week 8-10)

**Objective**: Modernize data structures with standard library equivalents.

#### 3.1 Replace Custom Array with std::mdspan

**Current Code** (array.h): Custom Array class with manual slicing
```cpp
Array Array::Slice(std::size_t begin, std::size_t end) const {
  Array ret = *this;
  ret.ptr = static_cast<char*>(ptr) + begin * ElementSize();
  ret.shape[0] = end - begin;
  return ret;
}
```

**Refactored**:
```cpp
#include <mdspan>

template <typename T, typename Extents>
using ArrayView = std::mdspan<T, Extents>;

// Multi-dimensional slicing with std::submdspan
auto slice = std::submdspan(arr, std::pair{begin, end}, std::full_extent);
```

**Benefits**:
- Standard library
- Better compiler optimization
- Type-safe multi-dimensional indexing
- Works with std::linalg (C++26)

**Challenges**:
- Large refactor (Array used everywhere)
- Need careful migration strategy
- May break Python bindings

**Recommendation**: Phase 3b (after core refactoring stable)

---

#### 3.2 Replace ThreadPool with std::execution

**Current Code**: Uses custom ThreadPool from third_party
```cpp
ThreadPool init_pool(std::min(processor_count, num_envs_));
for (std::size_t i = 0; i < num_envs_; ++i) {
  result.emplace_back(init_pool.enqueue(
    [i, spec, this] { envs_[i].reset(new Env(spec, i)); }));
}
```

**Refactored**:
```cpp
#include <execution>
#include <ranges>

namespace stdex = std::execution;

auto init_envs = std::views::iota(0u, num_envs_)
  | std::views::transform([spec, this](std::size_t i) {
      return [i, spec, this] {
        envs_[i].reset(new Env(spec, i));
      };
    });

// Execute in parallel
stdex::sync_wait(
  stdex::when_all(
    init_envs | std::views::transform(stdex::then)
  )
);
```

**Benefits**:
- Standard library (C++26)
- Better scheduling algorithms
- Composable async operations

**Challenges**:
- std::execution still evolving (may not be in C++26 final)
- Compiler support varies
- May need fallback to ThreadPool

**Recommendation**: Phase 3c (experimental, keep ThreadPool as fallback)

---

### Phase 4: Smart Pointers & Ownership (Week 11-12)

**Objective**: Clarify ownership with modern smart pointers.

#### 4.1 Replace unique_ptr with Proper Ownership

**Current Code**: unique_ptr used correctly, but can be more explicit
```cpp
std::vector<std::unique_ptr<Env>> envs_;
```

**Refactored**: Add ownership documentation and lifetime annotations
```cpp
// Owner of all environment instances
// Lifetime: construction to destruction
// Thread-safety: Environments accessed only by worker threads
std::vector<std::unique_ptr<Env>> envs_;  // Keep as-is, but document
```

#### 4.2 Replace shared_ptr with Safer Alternatives

**Current Code** (env.h:77):
```cpp
std::shared_ptr<std::vector<Array>> action_batch_;
```

**Refactored**: Use std::unique_ptr + raw pointers (observer pattern)
```cpp
// In AsyncEnvPool
std::vector<std::unique_ptr<std::vector<Array>>> action_batches_;

// In Env
std::vector<Array>* action_batch_;  // Non-owning observer
```

**Benefits**:
- No reference counting overhead
- Clear ownership (AsyncEnvPool owns, Env observes)
- Lifetime is bounded by AsyncEnvPool

**Performance Impact**: Benchmark (expected: 5-10% faster Send())

---

### Phase 5: Compile-Time Optimization (Week 13-14)

**Objective**: Move computations to compile-time.

#### 5.1 constexpr and consteval

**Current Code**: Runtime computations that could be compile-time
```cpp
std::size_t queue_size_ = (num_envs / batch_env + 2) * 2;
```

**Refactored**:
```cpp
[[nodiscard]] constexpr std::size_t calculate_queue_size(
    std::size_t num_envs,
    std::size_t batch_env) noexcept {
  return (num_envs / batch_env + 2) * 2;
}

// If params known at compile-time:
inline constexpr std::size_t queue_size = calculate_queue_size(NUM_ENVS, BATCH);
```

#### 5.2 Template Specialization for Common Cases

**Current Code**: All cases use same code path
**Refactored**: Specialize for common scenarios
```cpp
// Specialization for single-player sync mode
template<>
class AsyncEnvPool<Env, Mode::Sync, NumPlayers::Single> {
  // Optimized implementation without player tracking
};
```

---

### Phase 6: Modules (Week 15-16)

**Objective**: Convert headers to C++20 modules.

**Current Code**: Header-based
```cpp
#ifndef ENVPOOL_CORE_ASYNC_ENVPOOL_H_
#define ENVPOOL_CORE_ASYNC_ENVPOOL_H_
// ...
#endif
```

**Refactored**: Module-based
```cpp
// async_envpool.cppm
export module envpool.core.async_envpool;

import envpool.core.action_buffer_queue;
import envpool.core.state_buffer_queue;

export template <typename Env>
class AsyncEnvPool { /* ... */ };
```

**Benefits**:
- Faster compilation (no repeated parsing)
- Better isolation
- Cleaner dependencies

**Challenges**:
- Build system changes (Bazel module support)
- Toolchain support varies
- Migration is all-or-nothing per library

**Recommendation**: Phase 6 (final phase, after all other refactoring stable)

---

## Refactoring Principles

### 1. Incremental Changes

- One feature at a time
- Each change independently testable
- Maintain working codebase at all times
- Use feature flags if needed

### 2. Performance First

- **ALWAYS** benchmark before and after
- Document performance impact
- Rollback if regression > 5%
- Optimize hot paths first

### 3. Test Coverage

- Achieve >90% line coverage
- >80% branch coverage
- Stress tests for concurrency
- Memory leak tests (valgrind/ASan)

### 4. Documentation

- Update docs in same commit as code
- Document non-obvious decisions
- Add examples for new patterns
- Architecture diagrams

### 5. Code Review

- All changes reviewed by 2+ people
- Performance results in PR description
- Explain trade-offs made

---

## Performance Benchmarking Strategy

### Micro-Benchmarks

For each component, measure:

1. **Throughput**: Operations per second
2. **Latency**: p50, p90, p99, p99.9
3. **Scalability**: Performance vs. thread count
4. **Memory**: Allocations per operation

**Tools**: Google Benchmark, custom harness

**Example**:
```cpp
static void BM_ActionBufferQueue_EnqueueDequeue(benchmark::State& state) {
  ActionBufferQueue queue(1000);
  std::vector<ActionSlice> actions(state.range(0));

  for (auto _ : state) {
    queue.EnqueueBulk(actions);
    for (int i = 0; i < state.range(0); ++i) {
      benchmark::DoNotOptimize(queue.Dequeue());
    }
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ActionBufferQueue_EnqueueDequeue)->Range(8, 256);
```

### Integration Benchmarks

Full AsyncEnvPool scenarios:
- CartPole-v0: 100 envs, 32 batch, measure FPS
- Atari Pong: 1000 envs, 256 batch, measure FPS
- Multi-player: 100 envs, 10 players, measure FPS

**Baseline**: Current implementation
**Target**: ≥95% of baseline (allow 5% margin)

---

## Risk Mitigation

### Risk 1: Performance Regression

**Mitigation**:
- Micro-benchmarks for every change
- Integration benchmarks weekly
- Automated performance CI
- Rollback plan

### Risk 2: Compiler Support

**Mitigation**:
- Test on GCC 14, Clang 18, MSVC 19.38
- Feature detection with `__has_cpp_attribute`
- Fallback implementations
- Document minimum compiler versions

### Risk 3: Library Unavailability

**Mitigation**:
- Check std::execution availability
- Keep current ThreadPool as fallback
- Conditional compilation (#ifdef)
- Polyfills for missing features

### Risk 4: API Breakage

**Mitigation**:
- Maintain C API compatibility
- Python bindings unchanged (internal refactor only)
- Version bump (2.0.0)
- Migration guide

### Risk 5: Concurrency Bugs

**Mitigation**:
- ThreadSanitizer on all tests
- Stress tests (24+ hours)
- Memory ordering verification
- Code review by concurrency experts

---

## Success Metrics

### Code Quality

- ✅ Compile with -Wall -Wextra -Werror
- ✅ Pass clang-tidy with modernize checks
- ✅ Code coverage >90%
- ✅ No TSan/ASan warnings

### Performance

- ✅ <5% regression on any micro-benchmark
- ✅ ≥95% performance on integration tests
- ✅ Compile time <2x current (ideally better with modules)

### Maintainability

- ✅ Reduced LOC (target: -20%)
- ✅ Reduced cyclomatic complexity
- ✅ Updated documentation
- ✅ Examples for new patterns

### Compatibility

- ✅ Python API unchanged
- ✅ All existing tests pass
- ✅ Builds on Linux, macOS, Windows

---

## Timeline

**Total Estimated Time**: 16 weeks (4 months)

| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| 0: Preparation | 2 weeks | Tests, benchmarks, baselines |
| 1: Type Safety | 2 weeks | std::expected, concepts |
| 2: Concurrency | 3 weeks | std::jthread, atomics |
| 3: Data Structures | 3 weeks | std::mdspan, std::execution |
| 4: Ownership | 2 weeks | Smart pointers, lifetimes |
| 5: Compile-Time | 2 weeks | constexpr, specializations |
| 6: Modules | 2 weeks | C++20 modules |

**Buffer**: +4 weeks for unexpected issues

**Total**: 20 weeks (5 months with buffer)

---

## Next Steps

1. ✅ Review this plan
2. ⬜ Get team approval
3. ⬜ Set up C++26 toolchain
4. ⬜ Create refactoring branch
5. ⬜ Start Phase 0: Preparation

---

## Open Questions

1. **Which C++26 features are actually available in target compilers?**
   - Need to check GCC 14, Clang 18 feature status
   - May need to target C++23 instead

2. **Is std::counting_semaphore as fast as moodycamel::LightweightSemaphore?**
   - Critical benchmark required
   - May keep current if slower

3. **Should we use coroutines for worker threads?**
   - Potential for cleaner async code
   - Overhead concerns
   - Need POC + benchmarks

4. **Modules in Bazel?**
   - Bazel module support status?
   - Alternative build system?

5. **Backward compatibility requirements?**
   - Major version bump acceptable?
   - Migration period?

---

## Conclusion

This refactoring plan modernizes EnvPool's core to C++26 while maintaining its exceptional performance. By taking an incremental, benchmark-driven approach, we minimize risk and ensure the codebase remains production-ready throughout the process.

**Key Principle**: **"Make it work, make it right, make it fast" — we have "work" and "fast", now we make it "right" without losing "fast".**
