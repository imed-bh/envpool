# Modern C++26 Async Patterns in EnvPool

This document explains the modern C++ concurrency patterns used in the refactored EnvPool implementation.

---

## Table of Contents

1. [std::jthread - RAII Thread Management](#stdjthread---raii-thread-management)
2. [std::stop_token - Cooperative Cancellation](#stdstop_token---cooperative-cancellation)
3. [std::expected - Error Handling](#stdexpected---error-handling)
4. [std::counting_semaphore - Synchronization](#stdcounting_semaphore---synchronization)
5. [Memory Ordering - Performance](#memory-ordering---performance)
6. [Concepts - Type Safety](#concepts---type-safety)
7. [Ranges - Data Processing](#ranges---data-processing)
8. [RAII Everywhere - Resource Safety](#raii-everywhere---resource-safety)

---

## std::jthread - RAII Thread Management

### The Problem with std::thread

```cpp
// Original code (problematic)
class AsyncEnvPool {
  std::vector<std::thread> workers_;
  std::atomic<int> stop_;

  ~AsyncEnvPool() {
    stop_ = 1;  // Signal stop
    // Send dummy actions to wake up threads
    action_queue_->EnqueueBulk(empty_actions);
    // Manually join all threads
    for (auto& worker : workers_) {
      worker.join();
    }
  }
};
```

**Issues**:
- Manual stop flag management
- Need to wake up blocked threads
- Manual join in destructor
- Exception-unsafe (if join throws)
- Easy to forget join (std::terminate!)

### The Solution: std::jthread

```cpp
// Modern code (RAII)
class AsyncEnvPool {
  std::vector<std::jthread> workers_;

  ~AsyncEnvPool() {
    // That's it! std::jthread destructor:
    // 1. Requests stop via stop_token
    // 2. Joins automatically
    // No manual cleanup needed!
  }
};
```

**Benefits**:
- Automatic stop request
- Automatic join on destruction
- Exception-safe
- Less boilerplate
- Impossible to forget cleanup

### Worker Thread Pattern

```cpp
void SpawnWorkers() {
  for (std::size_t i = 0; i < num_threads_; ++i) {
    workers_.emplace_back([this, i](std::stop_token stoken) {
      WorkerLoop(i, stoken);
    });
  }
}

void WorkerLoop(std::size_t id, std::stop_token stoken) {
  while (!stoken.stop_requested()) {
    // Do work...

    // Periodically check stop_token
    if (auto action = queue_->TryDequeueFor(100ms)) {
      ProcessAction(*action);
    }
  }
}
```

---

## std::stop_token - Cooperative Cancellation

### The Pattern

```cpp
// Background thread with cooperative cancellation
void BackgroundTask(std::stop_token stoken) {
  while (!stoken.stop_requested()) {
    // Do work...

    // Check frequently for cancellation
    if (stoken.stop_requested()) break;

    // Or use stop_token in blocking calls
    if (cv.wait_for(lock, 100ms, stoken, predicate)) {
      // Condition met or stop requested
    }
  }
}
```

### Advantages Over Manual Flags

| Feature | Atomic Flag | std::stop_token |
|---------|-------------|-----------------|
| Type-safe | No | Yes |
| Composable | No | Yes |
| Integrated with jthread | No | Yes |
| Multiple stop sources | Hard | Easy |
| Standard | No | Yes (C++20) |

### Example: StateBufferQueue Background Threads

```cpp
void SpawnBufferCreationThreads() {
  for (size_t i = 0; i < num_threads; ++i) {
    create_buffer_threads_.emplace_back([this](std::stop_token stoken) {
      while (!stoken.stop_requested()) {
        // Create buffer
        auto buf = std::make_unique<StateBuffer>(...);

        // Put in stock (may block)
        stock_buffer_.Put(std::move(buf));

        // Check if we should stop
        if (stoken.stop_requested()) break;
      }
    });
  }
}
```

**No manual cleanup needed!** Destructor automatically:
1. Requests stop on all threads
2. Waits for all threads to finish
3. Cleans up resources

---

## std::expected - Error Handling

### The Problem with Exceptions

```cpp
// Original code
WritableSlice Allocate(size_t num_players) {
  if (alloc_count >= batch_) {
    throw std::out_of_range("out of storage");  // Exception!
  }
  // ... allocate and return
}

// Usage (hidden error path)
auto slice = buffer.Allocate(1);  // May throw!
```

**Issues**:
- Hidden control flow
- Performance overhead (stack unwinding)
- Forces error handling or crash
- Hard to see error paths in code

### The Solution: std::expected

```cpp
// Modern code
std::expected<WritableSlice, StateBufferError> Allocate(
    size_t num_players) noexcept {  // noexcept!

  if (alloc_count >= batch_) {
    return std::unexpected(StateBufferError::OutOfStorage);
  }

  return WritableSlice{...};
}

// Usage (explicit error handling)
auto result = buffer.Allocate(1);
if (!result) {
  // Handle error explicitly
  switch (result.error()) {
    case StateBufferError::OutOfStorage:
      // Handle full buffer
      break;
    case StateBufferError::InvalidPlayerCount:
      // Handle invalid input
      break;
  }
  return;
}

// Success path
auto slice = std::move(*result);
```

**Benefits**:
- Explicit error handling
- No exceptions (noexcept!)
- Zero-cost (optimizes to simple branch)
- Type-safe error codes
- Composable (monadic operations)

### Error Propagation

```cpp
// Automatic error propagation with operator?
std::expected<void, Error> Process() {
  auto slice = buffer.Allocate(1);  // Returns expected
  if (!slice) return std::unexpected(slice.error());  // Propagate

  // Or with monadic operations (C++23)
  return buffer.Allocate(1)
    .and_then([](auto slice) { return DoWork(slice); })
    .or_else([](auto err) { return HandleError(err); });
}
```

---

## std::counting_semaphore - Synchronization

### The Pattern

```cpp
class CircularBuffer {
  std::counting_semaphore<> sem_get_{0};      // Items available
  std::counting_semaphore<> sem_put_{size_};  // Space available

  void Put(T value) {
    sem_put_.acquire();     // Wait for space
    // Write value...
    sem_get_.release();     // Signal item available
  }

  T Get() {
    sem_get_.acquire();     // Wait for item
    // Read value...
    sem_put_.release();     // Signal space available
    return value;
  }
};
```

### vs. Condition Variables

| Feature | Condition Variable | Semaphore |
|---------|-------------------|-----------|
| Spurious wakeups | Yes | No |
| Lock required | Yes (mutex) | No |
| Count-based | No | Yes |
| Performance | Slower | Faster |
| Use case | Complex conditions | Simple counting |

### Performance Note

**CRITICAL**: Benchmark `std::counting_semaphore` vs `LightweightSemaphore`!

```cpp
// LightweightSemaphore (from concurrentqueue)
// - Highly optimized (~10x faster on some platforms)
// - Futex-based on Linux
// - May outperform std::counting_semaphore

// Decision: Keep the faster one after benchmarking
```

---

## Memory Ordering - Performance

### Relaxed Ordering for Counters

```cpp
// Non-synchronizing atomics (fastest)
uint64_t tail = tail_.fetch_add(1, std::memory_order_relaxed);

// Why relaxed?
// - Just need atomic increment
// - Semaphore provides synchronization
// - No need for expensive memory barriers
```

### Acquire-Release for Synchronization

```cpp
// Producer
data_ = value;  // Write data
sem_.release(); // Release: makes writes visible

// Consumer
sem_.acquire(); // Acquire: sees all writes before release
auto value = data_;  // Read data
```

### Memory Ordering Guide

| Operation | Ordering | Reason |
|-----------|----------|--------|
| Counter increment | relaxed | No synchronization needed |
| Flag check | acquire | See writes before flag set |
| Flag set | release | Make writes visible |
| Shutdown signal | acq_rel | Both directions |
| Reference count | relaxed → acq_rel | Optimize common case |

### Example: StateBuffer Dual-Counter

```cpp
// Original: Bit-packed into single atomic (clever!)
uint64_t increment = (uint64_t(num_players) << 32) | 1;
uint64_t offsets = offsets_.fetch_add(increment);  // One atomic op

// Modern: Separate atomics (clearer, benchmark required)
uint32_t player_off = player_offset_.fetch_add(
    num_players, std::memory_order_relaxed);
uint32_t shared_off = shared_offset_.fetch_add(
    1, std::memory_order_relaxed);

// Trade-off: Clarity vs. Performance
// Decision: Benchmark and choose!
```

---

## Concepts - Type Safety

### Environment Concept

```cpp
template <typename E>
concept Environment = requires(E env) {
  typename E::Spec;
  typename E::Action;
  typename E::State;
  { env.IsDone() } -> std::same_as<bool>;
};

// Usage: Constrained template
template <Environment Env>
class AsyncEnvPool { ... };

// Compile-time error if not an Environment!
// AsyncEnvPool<int> pool;  // Error: int is not an Environment
```

### Benefits Over SFINAE

```cpp
// Old way (SFINAE)
template <typename E,
          typename = std::enable_if_t<
              std::is_same_v<decltype(std::declval<E>().IsDone()), bool>>>
class AsyncEnvPool { ... };
// Unreadable error messages!

// New way (Concepts)
template <Environment E>
class AsyncEnvPool { ... };
// Clear error: "E does not satisfy Environment concept"
```

### Callback Concept

```cpp
template <typename F>
concept DoneCallback = requires(F f) {
  { f() } -> std::same_as<void>;
};

// Usage
template <DoneCallback Callback>
struct WritableSlice {
  Callback done_write;  // Type-safe callback
};
```

---

## Ranges - Data Processing

### Transform with Ranges

```cpp
// Original
std::vector<bool> is_player_state_;
for (const auto& spec : specs) {
  is_player_state_.push_back(!spec.shape.empty() && spec.shape[0] == -1);
}

// Modern (ranges)
auto is_player_state = specs
  | std::views::transform([](const auto& s) {
      return !s.shape.empty() && s.shape[0] == -1;
    })
  | std::ranges::to<std::vector>();
```

### Lazy Evaluation

```cpp
// Generate action slices
auto actions = env_ids
  | std::views::enumerate
  | std::views::transform([](auto&& pair) {
      auto [i, eid] = pair;
      return ActionSlice{.env_id = eid, .order = i};
    });

// Lazy: No allocation until consumed!
queue.EnqueueBulk(actions);  // Now evaluated
```

---

## RAII Everywhere - Resource Safety

### Principle

**"Resource Acquisition Is Initialization"**

Every resource is owned by an object whose lifetime is managed automatically.

### Examples in EnvPool

#### 1. Thread Management

```cpp
// RAII: std::jthread
std::vector<std::jthread> workers_;
// Destructor automatically stops and joins

// Non-RAII: std::thread
std::vector<std::thread> workers_;
// Must manually join in destructor (error-prone!)
```

#### 2. Memory Management

```cpp
// RAII: unique_ptr
std::unique_ptr<StateBuffer> buffer =
    std::make_unique<StateBuffer>(...);
// Destructor automatically deletes

// RAII: shared_ptr for shared ownership
std::shared_ptr<std::vector<Array>> action_batch;
// Reference counted, deleted when last reference gone
```

#### 3. Lock Management

```cpp
// RAII: std::scoped_lock
{
  std::scoped_lock lock(mutex1, mutex2);  // Acquire both
  // Critical section...
}  // Both mutexes released automatically (even if exception)

// Non-RAII: manual lock/unlock
mutex.lock();
// If exception here, mutex never unlocked! DEADLOCK!
mutex.unlock();
```

#### 4. Semaphore Management

```cpp
// RAII: Automatic release
{
  SemaphoreGuard guard(sem);  // Acquire
  // Work...
}  // Released automatically

// Or use built-in acquire/release
sem.acquire();  // Acquire
// ... work ...
sem.release();  // Release
```

#### 5. WritableSlice

```cpp
// RAII: done_write callback
{
  auto slice = buffer.Allocate(1);  // Returns WritableSlice
  // Write to slice...
  slice.arr[0] = data;
}  // Destructor calls done_write() automatically!

// Can also call manually if needed
slice.Complete();  // Explicit completion
```

---

## Complete Pattern: AsyncEnvPool Modern

Putting it all together:

```cpp
template <Environment Env>
class AsyncEnvPool {
  // RAII: unique_ptr for environments
  std::vector<std::unique_ptr<Env>> envs_;

  // RAII: unique_ptr for queues
  std::unique_ptr<ActionBufferQueue> action_queue_;
  std::unique_ptr<StateBufferQueue> state_queue_;

  // RAII: jthread for workers
  std::vector<std::jthread> workers_;

  // Modern concurrency
  std::atomic<size_t> stepping_env_num_{0};

  // std::expected for errors
  [[nodiscard]]
  std::expected<void, EnvPoolError> Send(const Action& action) noexcept {
    // Validate
    if (invalid) {
      return std::unexpected(EnvPoolError::InvalidAction);
    }

    // Process with ranges
    auto actions = env_ids
      | std::views::transform(MakeActionSlice);

    // Enqueue
    return action_queue_->EnqueueBulk(actions);
  }

  // Worker loop with stop_token
  void WorkerLoop(std::stop_token stoken) noexcept {
    while (!stoken.stop_requested()) {
      // Try dequeue with timeout
      if (auto action = action_queue_->TryDequeueFor(100ms)) {
        ProcessAction(*action);
      }
    }
  }

  // RAII destructor: Everything cleaned up automatically!
  ~AsyncEnvPool() = default;
};
```

**Zero manual cleanup!**
- jthread stops and joins workers
- unique_ptr deletes resources
- No leak possible
- Exception-safe

---

## Performance Considerations

### 1. std::expected is Zero-Cost

```cpp
// Compiles to:
if (error_condition) {
  return error_code;  // Simple branch
} else {
  return value;  // Simple return
}

// No heap allocation
// No exception unwinding
// Same performance as manual error codes
```

### 2. Memory Ordering Matters

```cpp
// Relaxed: ~1 cycle
counter.fetch_add(1, std::memory_order_relaxed);

// Seq_cst: ~10-100 cycles (memory fence)
counter.fetch_add(1, std::memory_order_seq_cst);

// On x86: Relaxed → Seq_cst is cheap (TSO memory model)
// On ARM: Huge difference! (weak memory model)
```

### 3. Concepts: Compile-Time Only

```cpp
// No runtime cost!
template <Environment Env>  // Checked at compile-time
class AsyncEnvPool { ... };

// Same generated code as unconstrained template
```

### 4. Ranges: Lazy Evaluation

```cpp
// No allocation until consumed
auto result = data
  | std::views::filter(predicate)
  | std::views::transform(func)
  | std::views::take(10);

// Allocation happens here
std::vector<int> vec(result.begin(), result.end());
```

---

## Migration Guide

### From Original to Modern

1. **Replace std::thread with std::jthread**
   ```cpp
   - std::thread worker;
   + std::jthread worker;
   - // Manual join in destructor
   + // Automatic!
   ```

2. **Replace exceptions with std::expected**
   ```cpp
   - void Allocate() { throw ...; }
   + std::expected<T, E> Allocate() noexcept;
   ```

3. **Add stop_token to worker loops**
   ```cpp
   - void WorkerLoop() {
   -   while (!stop_flag_) { ... }
   - }
   + void WorkerLoop(std::stop_token stoken) {
   +   while (!stoken.stop_requested()) { ... }
   + }
   ```

4. **Use concepts for constraints**
   ```cpp
   - template <typename E>
   + template <Environment E>
   ```

5. **Add explicit memory ordering**
   ```cpp
   - counter.fetch_add(1);
   + counter.fetch_add(1, std::memory_order_relaxed);
   ```

---

## Checklist for Modern C++

- [ ] Use std::jthread instead of std::thread
- [ ] Use std::stop_token for cancellation
- [ ] Use std::expected instead of exceptions
- [ ] Use concepts for template constraints
- [ ] Use std::span for array views
- [ ] Use ranges for data processing
- [ ] Use RAII for all resources
- [ ] Specify memory ordering explicitly
- [ ] Mark functions noexcept where possible
- [ ] Use [[nodiscard]] for important returns
- [ ] Use alignas for cache optimization
- [ ] Document thread safety guarantees

---

## References

- **C++20 Concurrency**: https://en.cppreference.com/w/cpp/thread
- **std::expected**: https://en.cppreference.com/w/cpp/utility/expected
- **C++20 Concepts**: https://en.cppreference.com/w/cpp/language/constraints
- **C++20 Ranges**: https://en.cppreference.com/w/cpp/ranges
- **Memory Ordering**: https://en.cppreference.com/w/cpp/atomic/memory_order

---

## Conclusion

Modern C++ provides powerful tools for safe, efficient concurrency:

1. **RAII** - Automatic resource management
2. **jthread** - Safe thread lifetime
3. **expected** - Explicit error handling
4. **Concepts** - Clear type requirements
5. **Ranges** - Composable data processing

**Result**: Code that is both safer AND more performant! 🚀
