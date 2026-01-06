# EnvPool Async Architecture - Deep Dive Documentation

## Table of Contents
1. [Overview](#overview)
2. [Lock-Free Queue Implementations](#lock-free-queue-implementations)
3. [Async Execution Model](#async-execution-model)
4. [Threading Model](#threading-model)
5. [Memory Management](#memory-management)
6. [Performance Optimizations](#performance-optimizations)
7. [Code Flow Analysis](#code-flow-analysis)
8. [Concurrency Guarantees](#concurrency-guarantees)

---

## Overview

EnvPool achieves exceptional performance (1M+ FPS) through a sophisticated lock-free architecture that enables parallel execution of multiple RL environments. The core design principles are:

- **Lock-free data structures** using atomic operations
- **Zero-copy memory sharing** via slicing and shared_ptr
- **Pre-allocated buffer pools** to eliminate runtime allocation
- **Dedicated worker threads** for parallel execution
- **Batch-first API** that amortizes synchronization costs

### High-Level Architecture

```
User Thread                Worker Threads (N)              Background Threads
    |                            |                                |
    v                            v                                v
[Send(actions)] ──────> [ActionBufferQueue] ──> [Worker 1: Env Step]
                              │                  [Worker 2: Env Step]
                              │                  [Worker N: Env Step]
                              │                        │
                              v                        v
[Recv()] <────────── [StateBufferQueue] <─── [Write Results]
                              ^
                              │
                    [Buffer Recycling] <─── [Create StateBuffers]
```

---

## Lock-Free Queue Implementations

EnvPool uses three lock-free queue implementations, each optimized for specific access patterns.

### 1. ActionBufferQueue (envpool/core/action_buffer_queue.h)

**Purpose**: Distributes actions from user thread to worker threads.

**Key Design Characteristics**:
- **Access Pattern**: Single producer (user thread) → Multiple consumers (worker threads)
- **Data Structure**: Circular buffer of size `2 * num_envs`
- **Synchronization**: Lightweight semaphores + atomic counters

**Implementation Details**:

```cpp
class ActionBufferQueue {
protected:
  std::atomic<uint64_t> alloc_ptr_;     // Producer position (monotonic)
  std::atomic<uint64_t> done_ptr_;      // Consumer position (monotonic)
  std::size_t queue_size_;              // 2 * num_envs (power-of-2 friendly)
  std::vector<ActionSlice> queue_;      // Pre-allocated circular buffer

  // Semaphores for coordination
  moodycamel::LightweightSemaphore sem_;          // Available items
  moodycamel::LightweightSemaphore sem_enqueue_;  // Single producer lock
  moodycamel::LightweightSemaphore sem_dequeue_;  // Single consumer lock per dequeue
};
```

**EnqueueBulk Algorithm** (lines 59-69):
```
1. Wait on sem_enqueue_ to ensure single producer
2. Atomically fetch-and-add alloc_ptr_ by batch size
3. Write all ActionSlice objects to circular buffer:
   queue_[(alloc_ptr_ + i) % queue_size_] = action[i]
4. Signal sem_ with batch size (notify consumers)
5. Release sem_enqueue_
```

**Dequeue Algorithm** (lines 71-80):
```
1. Wait on sem_ (blocks until items available)
2. Wait on sem_dequeue_ (ensures mutual exclusion)
3. Atomically fetch-and-add done_ptr_ by 1
4. Read from circular buffer: queue_[done_ptr_ % queue_size_]
5. Release sem_dequeue_
6. Return ActionSlice
```

**Why This Works (Lock-Free Proof)**:
- **Monotonic counters**: `alloc_ptr_` and `done_ptr_` only increment (no ABA problem)
- **Sufficient buffer size**: `2 * num_envs` ensures no wrap-around collision
  - Producer can be up to `num_envs` ahead
  - Consumer removes items, maintaining distance
- **Atomic operations**: `fetch_add` is lock-free on all modern CPUs
- **Semaphores provide blocking**: But don't protect data structure itself

**Concurrency Invariants**:
```cpp
// Always true:
assert(alloc_ptr_ - done_ptr_ <= queue_size_);
assert(alloc_ptr_ >= done_ptr_);
```

---

### 2. StateBufferQueue (envpool/core/state_buffer_queue.h)

**Purpose**: Collects results from worker threads and provides batched states to user.

**Key Design Characteristics**:
- **Access Pattern**: Multiple producers (workers) → Single consumer (user thread)
- **Data Structure**: Circular queue of pre-allocated StateBuffer objects
- **Buffer Recycling**: Completed buffers swapped with fresh ones from stock

**Implementation Details**:

```cpp
class StateBufferQueue {
protected:
  std::size_t batch_;                           // Batch size
  std::size_t queue_size_;                      // (num_envs/batch + 2) * 2
  std::vector<std::unique_ptr<StateBuffer>> queue_;  // Circular buffer

  std::atomic<uint64_t> alloc_count_;           // Total allocations
  std::atomic<uint64_t> done_ptr_;              // Consumer position

  // Buffer recycling system
  CircularBuffer<std::unique_ptr<StateBuffer>> stock_buffer_;
  std::vector<std::thread> create_buffer_thread_;
  std::atomic<bool> quit_;
};
```

**Allocate Algorithm** (lines 116-130):
```
1. Worker calls Allocate(num_players, order)
2. Atomically fetch-and-add alloc_count_ by 1
3. Calculate buffer offset: (alloc_count_ / batch_) % queue_size_
4. Call queue_[offset]->Allocate() to get WritableSlice
5. Return slice with done_write callback
```

**Wait Algorithm** (lines 141-152):
```
1. User calls Wait()
2. Get fresh buffer from stock: newbuf = stock_buffer_.Get()
3. Atomically fetch-and-add done_ptr_ by 1
4. Calculate offset: done_ptr_ % queue_size_
5. Call queue_[offset]->Wait() - blocks until batch complete
6. Swap completed buffer with fresh one: std::swap(queue_[offset], newbuf)
7. Return arrays from completed buffer
```

**Buffer Lifecycle**:
```
[Background Thread] ──create──> [stock_buffer_] ──Get()──> [Fresh Buffer]
                                                                  │
                                                                  v
[queue_[i]] ──Wait()──> [Completed Buffer] ──swap──> [newbuf (will be recycled)]
     ^                                                           │
     │                                                           v
     └──────────────────────── reuse ─────────────────────────┘
```

**Background Buffer Creation** (lines 88-97):
```cpp
// Continuously create StateBuffer objects
for (std::size_t i = 0; i < create_buffer_thread_num; ++i) {
  create_buffer_thread_.emplace_back([&]() {
    while (!quit_) {
      stock_buffer_.Put(std::make_unique<StateBuffer>(...));
    }
  });
}
```

---

### 3. StateBuffer (envpool/core/state_buffer.h)

**Purpose**: Batch storage for environment states with lock-free allocation.

**Key Design Characteristics**:
- **Clever Trick**: Packs two 32-bit counters into single 64-bit atomic
- **Pre-allocated arrays**: All memory allocated at construction
- **Slicing**: Returns views into arrays without copying

**Implementation Details**:

```cpp
class StateBuffer {
protected:
  std::size_t batch_;                    // Number of environments in batch
  std::size_t max_num_players_;          // Max players per environment
  std::vector<Array> arrays_;            // Pre-allocated state arrays

  // Dual-counter in single atomic (genius optimization!)
  std::atomic<uint64_t> offsets_;        // [player_offset:32][shared_offset:32]

  std::atomic<std::size_t> alloc_count_; // Allocations made
  std::atomic<std::size_t> done_count_;  // Writes completed
  moodycamel::LightweightSemaphore sem_; // Signals batch completion
};
```

**Dual-Counter Atomic Trick** (lines 81-97):
```cpp
WritableSlice Allocate(std::size_t num_players, int order) {
  std::size_t alloc_count = alloc_count_.fetch_add(1);

  if (alloc_count < batch_) {
    // CLEVER: Pack two 32-bit increments into one 64-bit atomic!
    // Upper 32 bits: player_offset increment (num_players)
    // Lower 32 bits: shared_offset increment (1)
    uint64_t increment = static_cast<uint64_t>(num_players) << 32 | 1;
    uint64_t offsets = offsets_.fetch_add(increment);

    // Extract both offsets from single atomic read
    uint32_t player_offset = offsets >> 32;        // Upper 32 bits
    uint32_t shared_offset = offsets & 0xFFFFFFFF; // Lower 32 bits

    // Create slices of arrays
    std::vector<Array> state;
    for (std::size_t i = 0; i < arrays_.size(); ++i) {
      if (is_player_state_[i]) {
        state.emplace_back(arrays_[i].Slice(player_offset, player_offset + num_players));
      } else {
        state.emplace_back(arrays_[i][shared_offset]);
      }
    }

    return WritableSlice{
      .arr = std::move(state),
      .done_write = [this]() { Done(); }
    };
  }
  throw std::out_of_range("StateBuffer out of storage");
}
```

**Why Dual-Counter Works**:
- Single `fetch_add` is atomic → both counters updated together
- Avoids race conditions from separate increments
- No ABA problem (counters monotonic within buffer lifetime)
- Reduces contention (one atomic vs two)

**Done/Wait Synchronization** (lines 126-159):
```cpp
void Done(std::size_t num = 1) {
  std::size_t done_count = done_count_.fetch_add(num);
  if (done_count + num == batch_) {
    sem_.signal();  // Last writer signals completion
  }
}

std::vector<Array> Wait(std::size_t additional_done_count = 0) {
  if (additional_done_count > 0) {
    Done(additional_done_count);  // Handle sync mode
  }
  while (!sem_.wait()) {}  // Block until batch complete

  // Truncate arrays to actual written size
  uint64_t offsets = offsets_.load();
  uint32_t player_offset = offsets >> 32;
  uint32_t shared_offset = offsets & 0xFFFFFFFF;

  std::vector<Array> ret;
  for (std::size_t i = 0; i < arrays_.size(); ++i) {
    if (is_player_state_[i]) {
      ret.emplace_back(arrays_[i].Truncate(player_offset));
    } else {
      ret.emplace_back(arrays_[i].Truncate(shared_offset));
    }
  }
  return ret;
}
```

---

### 4. CircularBuffer (envpool/core/circular_buffer.h)

**Purpose**: Generic lock-free bounded queue for buffer recycling.

**Implementation Details**:

```cpp
template <typename V>
class CircularBuffer {
protected:
  std::size_t size_;
  moodycamel::LightweightSemaphore sem_get_;   // Items available
  moodycamel::LightweightSemaphore sem_put_;   // Space available
  std::vector<V> buffer_;
  std::atomic<uint64_t> head_;                 // Consumer position
  std::atomic<uint64_t> tail_;                 // Producer position
};
```

**Put/Get Algorithms**:
```cpp
template <typename T>
void Put(T&& v) {
  while (!sem_put_.wait()) {}           // Wait for space
  uint64_t tail = tail_.fetch_add(1);   // Claim slot
  buffer_[tail % size_] = std::forward<T>(v);
  sem_get_.signal();                    // Notify consumer
}

V Get() {
  while (!sem_get_.wait()) {}           // Wait for item
  uint64_t head = head_.fetch_add(1);   // Claim slot
  V v = std::move(buffer_[head % size_]);
  sem_put_.signal();                    // Notify producer
  return v;
}
```

---

## Async Execution Model

### AsyncEnvPool Architecture (envpool/core/async_envpool.h)

**Core Components**:

```cpp
template <typename Env>
class AsyncEnvPool : public EnvPool<typename Env::Spec> {
protected:
  std::size_t num_envs_;                          // Total environments
  std::size_t batch_;                             // Batch size for Recv()
  std::size_t num_threads_;                       // Worker threads
  bool is_sync_;                                  // Sync mode flag

  std::vector<std::unique_ptr<Env>> envs_;        // Environment instances
  std::vector<std::thread> workers_;              // Worker threads

  std::unique_ptr<ActionBufferQueue> action_buffer_queue_;
  std::unique_ptr<StateBufferQueue> state_buffer_queue_;

  std::atomic<int> stop_;                         // Shutdown signal
  std::atomic<std::size_t> stepping_env_num_;     // In-flight envs (sync mode)
};
```

### Execution Modes

#### Async Mode (default)
- Condition: `batch_size < num_envs`
- Behavior: Environments execute as soon as actions available
- Order: Non-deterministic (workers grab next available action)
- Use case: Maximum throughput, off-policy algorithms

#### Sync Mode
- Condition: `batch_size == num_envs && max_num_players == 1`
- Behavior: Results returned in same order as actions sent
- Order: Deterministic (order parameter enforced)
- Use case: On-policy algorithms, reproducibility

### Send/Recv Flow

**Send(action) Implementation** (lines 58-81):
```cpp
void SendImpl(V&& action) {
  // 1. Extract env_ids from action batch
  int* env_id = static_cast<int*>(action[0].Data());
  int shared_offset = action[0].Shape(0);

  // 2. Create ActionSlice for each environment
  std::vector<ActionSlice> actions;
  std::shared_ptr<std::vector<Array>> action_batch =
      std::make_shared<std::vector<Array>>(std::forward<V>(action));

  for (int i = 0; i < shared_offset; ++i) {
    int eid = env_id[i];
    envs_[eid]->SetAction(action_batch, i);  // Store shared_ptr
    actions.emplace_back(ActionSlice{
        .env_id = eid,
        .order = is_sync_ ? i : -1,
        .force_reset = false,
    });
  }

  // 3. Track in-flight envs (sync mode only)
  if (is_sync_) {
    stepping_env_num_ += shared_offset;
  }

  // 4. Enqueue to action buffer queue (bulk operation)
  action_buffer_queue_->EnqueueBulk(actions);
}
```

**Recv() Implementation** (lines 163-175):
```cpp
std::vector<Array> Recv() {
  // 1. Calculate if additional wait needed (sync mode)
  int additional_wait = 0;
  if (is_sync_ && stepping_env_num_ < batch_) {
    additional_wait = batch_ - stepping_env_num_;
  }

  // 2. Block until batch ready
  auto ret = state_buffer_queue_->Wait(additional_wait);

  // 3. Update in-flight counter (sync mode)
  if (is_sync_) {
    stepping_env_num_ -= ret[0].Shape(0);
  }

  return ret;
}
```

### Worker Thread Loop (lines 118-129)

```cpp
workers_.emplace_back([this] {
  for (;;) {
    // 1. Dequeue next action (blocks if queue empty)
    ActionSlice raw_action = action_buffer_queue_->Dequeue();

    // 2. Check shutdown signal
    if (stop_ == 1) break;

    // 3. Execute environment step
    int env_id = raw_action.env_id;
    int order = raw_action.order;
    bool reset = raw_action.force_reset || envs_[env_id]->IsDone();

    // 4. EnvStep handles: allocate state, reset/step, write results
    envs_[env_id]->EnvStep(state_buffer_queue_.get(), order, reset);
  }
});
```

---

## Threading Model

### Thread Types

1. **Main Thread (User Thread)**
   - Calls `Send(actions)` and `Recv()`
   - Single producer to ActionBufferQueue
   - Single consumer from StateBufferQueue

2. **Worker Threads (N threads)**
   - Execute environment steps in parallel
   - Multiple consumers from ActionBufferQueue
   - Multiple producers to StateBufferQueue
   - Thread count: `num_threads` (defaults to `min(batch_size, num_cores)`)

3. **Background Buffer Threads (M threads)**
   - Continuously create fresh StateBuffer objects
   - Multiple producers to stock_buffer_
   - Thread count: `max(1, num_cores / 64)` (typically 1-2 threads)

4. **Initialization Thread Pool**
   - Temporary pool for parallel environment construction
   - Thread count: `min(num_cores, num_envs)`
   - Destroyed after initialization

### Thread Synchronization

**No Traditional Locks in Fast Path!**
- All coordination via atomic operations and semaphores
- Semaphores provide blocking, but don't protect data structures
- Data structures themselves are lock-free

**Atomic Variables**:
```cpp
std::atomic<uint64_t> alloc_ptr_;       // ActionBufferQueue producer
std::atomic<uint64_t> done_ptr_;        // ActionBufferQueue consumer
std::atomic<uint64_t> alloc_count_;     // StateBufferQueue allocation counter
std::atomic<uint64_t> offsets_;         // StateBuffer dual-counter
std::atomic<std::size_t> done_count_;   // StateBuffer completion counter
std::atomic<int> stop_;                 // Shutdown signal
```

**Memory Ordering**:
- `fetch_add`: Sequential consistency (default)
- Ensures all threads see consistent ordering of operations
- No explicit memory barriers needed (handled by atomics)

### Thread Affinity (lines 131-142)

```cpp
if (spec.config["thread_affinity_offset"_] >= 0) {
  std::size_t thread_affinity_offset = spec.config["thread_affinity_offset"_];
  for (std::size_t tid = 0; tid < num_threads_; ++tid) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    std::size_t cid = (thread_affinity_offset + tid) % processor_count;
    CPU_SET(cid, &cpuset);
    pthread_setaffinity_np(workers_[tid].native_handle(),
                          sizeof(cpu_set_t), &cpuset);
  }
}
```

**Benefits**:
- Reduces cache invalidation (thread stays on same core)
- Improves L1/L2 cache hit rates
- Reduces context switch overhead
- Critical for high-throughput scenarios (>100K FPS)

---

## Memory Management

### Zero-Copy Design

**Principle**: Avoid copying large arrays at all costs.

**Techniques**:

1. **Array Slicing** (envpool/core/array.h):
```cpp
Array Array::Slice(std::size_t begin, std::size_t end) const {
  Array ret = *this;  // Shallow copy (shares data pointer)
  ret.ptr = static_cast<char*>(ptr) + begin * ElementSize();
  ret.shape[0] = end - begin;
  return ret;
}
```

2. **Shared Ownership** (action batches):
```cpp
std::shared_ptr<std::vector<Array>> action_batch_;
// Multiple environments hold shared_ptr to same action batch
// No copying, reference counting ensures lifetime
```

3. **Pre-Allocated Buffers**:
```cpp
// StateBuffer: All arrays allocated at construction
StateBuffer(std::size_t batch, std::size_t max_num_players,
            const std::vector<ShapeSpec>& specs)
    : arrays_(MakeArray(specs)) {  // One-time allocation
  // Workers write directly into pre-allocated arrays
}
```

### Buffer Recycling Strategy

**Why Recycle?**
- Allocation is expensive (especially for large arrays)
- Deallocation causes memory fragmentation
- CPU cache benefits from reusing same memory addresses

**How It Works**:
```
[Initial Pool]
  queue_[0] = StateBuffer  ┐
  queue_[1] = StateBuffer  │ Pre-allocated at startup
  ...                      │
  queue_[N] = StateBuffer  ┘

[During Execution]
  User calls Wait()
    1. Get fresh buffer from stock_buffer_
    2. Swap with completed buffer in queue_
    3. Completed buffer returned to user (arrays extracted)
    4. After user done, buffer eventually recycled

[Background Thread]
  Continuously creates new StateBuffers
  Puts them in stock_buffer_ (circular buffer)
  Ensures stock always has fresh buffers available
```

### Memory Barriers and Visibility

**C++ Memory Model Guarantees**:
- `std::atomic<T>::fetch_add()`: Sequential consistency
- All modifications before `fetch_add()` visible to threads that observe result
- Semaphore `signal()` synchronizes-with `wait()`

**Example Flow**:
```cpp
// Thread 1 (Worker)
envs_[id]->Step();              // Write to state
slice_.done_write();            // Calls Done()
  done_count_.fetch_add(1);     // [SYNC POINT]
  if (done_count == batch_)
    sem_.signal();              // [SYNC POINT]

// Thread 2 (User)
auto ret = Wait();
  sem_.wait();                  // [SYNC POINT]
  // All writes from Thread 1 visible here
  return arrays_;
```

---

## Performance Optimizations

### 1. Batch-First API
- Amortizes synchronization costs over batch
- Single `EnqueueBulk()` instead of N `Enqueue()` calls
- Reduces semaphore operations by factor of batch_size

### 2. Lock-Free Data Structures
- No kernel-level blocking in fast path
- Workers never stall on mutex contention
- Scales linearly with core count

### 3. Pre-Allocation
- All memory allocated at startup
- No malloc/free in critical path
- Predictable memory usage

### 4. Zero-Copy Array Operations
- Slicing returns views, not copies
- shared_ptr for shared ownership
- Direct writes to output buffers

### 5. Thread Affinity
- Reduces cache invalidation
- Improves instruction cache hit rate
- Minimizes NUMA penalties on multi-socket systems

### 6. Dual-Counter Atomic
- Two increments in one atomic operation
- Halves atomic contention
- Critical for StateBuffer scalability

### 7. Circular Buffer Sizing
- Powers of 2 for fast modulo (becomes bitwise AND)
- Size `2 * num_envs` prevents wraparound collisions

### 8. Semaphore Choice
- Uses Cameron314's LightweightSemaphore
- ~10x faster than std::mutex + std::condition_variable
- Optimized for high-frequency wait/signal patterns

---

## Code Flow Analysis

### Full Execution Trace (Async Mode)

```
[User Thread]
1. envpool.make("CartPole-v0", num_envs=100, batch_size=32)
   → AsyncEnvPool<CartPoleEnv>(spec)
     → Initialize 100 CartPoleEnv instances in parallel (ThreadPool)
     → Spawn worker threads (default: min(32, num_cores))
     → Create ActionBufferQueue(num_envs=100)
     → Create StateBufferQueue(batch=32, num_envs=100)

2. action = {"env_id": [0,1,2,...,31], "action": [0,1,0,...]}
   env.send(action)
   → SendImpl(action)
     → Create shared_ptr to action batch
     → For each env_id: envs_[eid]->SetAction(action_batch, i)
     → Create ActionSlice objects (env_id, order=-1, force_reset=false)
     → action_buffer_queue_->EnqueueBulk(actions)
       → sem_enqueue_.wait() - ensure single producer
       → alloc_ptr_.fetch_add(32) - claim 32 slots
       → Write 32 ActionSlice to circular buffer
       → sem_.signal(32) - notify workers
       → sem_enqueue_.signal() - release producer lock

[Worker Thread 1]
3. (Runs in parallel with other workers)
   → raw_action = action_buffer_queue_->Dequeue()
     → sem_.wait() - wait for available action
     → sem_dequeue_.wait() - ensure mutual exclusion
     → ptr = done_ptr_.fetch_add(1) - claim slot
     → ret = queue_[ptr % queue_size_]
     → sem_dequeue_.signal() - release
     → return ActionSlice{env_id=5, order=-1, force_reset=false}

   → envs_[5]->EnvStep(sbq, order=-1, reset=false)
     → PreProcess(sbq, -1, false)
       → sbq_ = sbq; current_step_++
     → ParseAction()
       → Extract action[5] from shared action batch
     → Step(action)
       → [User environment logic - e.g., physics update]
       → state = Allocate(num_players=1)
         → slice_ = sbq_->Allocate(1, -1)
           → pos = alloc_count_.fetch_add(1)  // pos = 5
           → offset = (5 / 32) % queue_size = 0
           → queue_[0]->Allocate(1, -1)
             → alloc_count = alloc_count_.fetch_add(1)  // count = 5
             → increment = (1 << 32) | 1
             → offsets = offsets_.fetch_add(increment)
               → player_offset = offsets >> 32 = 5
               → shared_offset = offsets & 0xFFFFFFFF = 5
             → Create slices: arrays_[i].Slice(5, 6) or arrays_[i][5]
             → return WritableSlice{arr, done_write=[this]{Done();}}
       → Write to state arrays: state["obs"_] = obs; state["reward"_] = r;
     → PostProcess()
       → slice_.done_write()
         → Done()
           → done_count = done_count_.fetch_add(1)  // count = 5
           → if (done_count + 1 == 32) sem_.signal()

[Worker Thread 2-N]
   (Same as Worker Thread 1, executing envs in parallel)
   (Eventually all 32 envs complete, done_count reaches 32)

[User Thread]
4. state = env.recv()
   → Recv()
     → additional_wait = 0 (async mode)
     → state_buffer_queue_->Wait(0)
       → newbuf = stock_buffer_.Get()
         → sem_get_.wait() - wait for fresh buffer
         → head = head_.fetch_add(1) - claim slot
         → v = std::move(buffer_[head % size_])
         → sem_put_.signal() - notify background thread
       → pos = done_ptr_.fetch_add(1)
       → offset = pos % queue_size = 0
       → arr = queue_[0]->Wait(0)
         → sem_.wait() - blocks until all 32 Done() called
         → offsets = offsets_.load() = (32 << 32) | 32
         → player_offset = 32, shared_offset = 32
         → Truncate arrays to actual size
         → return truncated arrays
       → std::swap(queue_[0], newbuf)
       → return arr

5. [Repeat steps 2-4]

[Background Thread]
6. (Runs continuously)
   → while (!quit_)
       → buf = std::make_unique<StateBuffer>(32, 1, specs, ...)
       → stock_buffer_.Put(std::move(buf))
         → sem_put_.wait() - wait for space
         → tail = tail_.fetch_add(1)
         → buffer_[tail % size_] = buf
         → sem_get_.signal() - notify consumer
```

---

## Concurrency Guarantees

### Safety Properties

1. **No Data Races**: All shared data accessed via atomics or after synchronization
2. **No Deadlocks**: Semaphores always signaled in correct order
3. **No Livelocks**: Forward progress guaranteed (no retry loops with same input)
4. **No ABA Problems**: Monotonic counters + sufficient buffer size

### Correctness Invariants

```cpp
// ActionBufferQueue
assert(alloc_ptr_ >= done_ptr_);
assert(alloc_ptr_ - done_ptr_ <= queue_size_);

// StateBufferQueue
assert(alloc_count_ >= done_ptr_ * batch_);
assert(done_ptr_ * batch_ <= alloc_count_);

// StateBuffer
assert(alloc_count_ <= batch_);
assert(done_count_ <= batch_);
assert(done_count_ <= alloc_count_);
uint32_t player_offset = offsets_ >> 32;
uint32_t shared_offset = offsets_;
assert(shared_offset == alloc_count_);
assert(player_offset <= shared_offset * max_num_players_);
```

### Happens-Before Relationships

```
[Send Thread]
  SetAction() ──hb──> EnqueueBulk() ──hb──> sem_.signal()

[Worker Thread]
  sem_.wait() ──hb──> Dequeue() ──hb──> Step() ──hb──> Allocate() ──hb──> Done()

[Recv Thread]
  Wait() ──hb──> sem_.wait() ──hb──> Return Arrays

[Memory Order]
  Write to arrays ──hb──> done_write() ──hb──> sem_.signal() ──sb──> sem_.wait() ──hb──> Read arrays
```

Where:
- `hb` = happens-before (guaranteed ordering)
- `sb` = synchronizes-with (memory barrier)

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `Send(batch)` | O(batch) | Amortized O(1) per env |
| `Recv()` | O(1) | Blocking if batch not ready |
| `Dequeue()` | O(1) | Lock-free atomic operation |
| `Allocate()` | O(num_arrays) | Lock-free, pre-allocated |
| `Done()` | O(1) | Lock-free atomic increment |

### Space Complexity

| Data Structure | Space | Notes |
|----------------|-------|-------|
| ActionBufferQueue | O(num_envs) | 2x buffer |
| StateBufferQueue | O(num_envs * state_size) | Circular buffer |
| StateBuffer | O(batch * state_size) | Pre-allocated arrays |
| CircularBuffer | O(num_envs/batch) | Buffer pool |

### Scalability

- **Core Scaling**: Linear up to num_cores (no lock contention)
- **Environment Scaling**: O(1) per-env overhead (shared queues)
- **Batch Scaling**: Amortized synchronization costs

---

## Known Limitations and Caveats

1. **StateBufferQueue Wait() is Single-Threaded**
   - Comment on line 136: "Wait should be accessed from only one thread"
   - Multiple consumers would violate ordering guarantees

2. **Fixed Buffer Sizes**
   - Queue sizes determined at construction
   - No dynamic resizing (prevents runtime allocation)

3. **Memory Overhead**
   - Pre-allocation trades memory for speed
   - Total memory: `O(num_envs * state_size + batch * state_size * queue_depth)`

4. **Sync Mode Restrictions**
   - Only works with single-player environments
   - Requires `batch_size == num_envs`

5. **Commented Out Code**
   - Lines 119-128 in state_buffer_queue.h (lazy buffer allocation)
   - Suggests possible future optimization

---

## References

- **LightweightSemaphore**: [concurrentqueue library](https://github.com/cameron314/concurrentqueue)
- **ThreadPool**: [progschj/ThreadPool](https://github.com/progschj/ThreadPool)
- **Lock-Free Algorithms**: "The Art of Multiprocessor Programming" by Herlihy & Shavit
- **C++ Memory Model**: "C++ Concurrency in Action" by Anthony Williams

---

## Conclusion

EnvPool's architecture demonstrates several advanced concurrency techniques:

1. **Lock-free programming** for maximum scalability
2. **Memory pre-allocation** to eliminate runtime overhead
3. **Zero-copy design** to minimize data movement
4. **Clever atomic tricks** (dual-counter) for efficiency
5. **Buffer recycling** for cache-friendly memory access

The result is a high-performance RL environment pool that can achieve over 1 million frames per second on commodity hardware.

**Key Takeaway**: By eliminating all locks from the critical path and using carefully designed lock-free queues, EnvPool achieves near-linear scaling with the number of CPU cores.
