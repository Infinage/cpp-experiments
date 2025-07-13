# C++ Memory Management Playground

This repository is a curated collection of experiments inspired by *C++ Memory Management* by Patrice Roy.

---

## 📁 Directory Structure

```bash
cpp-experiments/memory-management/
├── arena_allocator.cpp           # Custom allocator using arena/bump strategy
├── fwd_list_explicit_mem.hpp     # forward_list with raw new/delete
├── fwd_list_implicit_mem.hpp     # forward_list with smart pointers
├── fwd_list_alloc_support.hpp    # allocator-aware forward_list (custom allocator)
├── gc_at_program_end.cpp         # GC using RAII — runs on program end
├── gc_at_scope_end.cpp           # GC that triggers at scope end via custom ptrs
├── gc_trivially_dest.cpp         # GC specialized for trivially destructible types
├── leak_detector.hpp             # Global memory leak detection using operator overloading
├── shared_ptr.hpp                # Custom shared_ptr implementation
├── unique_ptr.hpp                # Custom unique_ptr implementation
```

---

## Highlights & Concepts

### 🪵 `forward_list_*`

A comparison of three `forward_list` implementations:

* **Raw pointers** (`fwd_list_explicit_mem.hpp`)
* **Smart pointers** (`fwd_list_implicit_mem.hpp`)
* **Allocator-aware** (`fwd_list_alloc_support.hpp`)

> 💡 Tip: Diff them side by side — the structural difference is smaller than you'd think.

---

### 🛠️ `arena_allocator.cpp`

A size-based bump allocator using a fixed memory arena. Allocations are fast, cache-friendly, and thread-safe. Ideal for allocating a large number of small objects (e.g., a million `Orc`s).

---

### ♻️ `gc_at_program_end.cpp`

A minimalist GC system using RAII. All allocations are tracked using a singleton, and freed only at program termination.

---

### 🧼 `gc_at_scope_end.cpp`

Implements a simple mark-and-sweep style deferred GC with `counting_ptr<T>`, a custom smart pointer. When all references go out of scope, the GC collects eligible memory.

Also features:
* Scope-bound GC via `scoped_collect`
* Support for arrays and non-trivial types

---

### 🧹 `gc_trivially_dest.cpp`

A specialization for trivially destructible types. Cleans up memory using `std::free` and skips destructor calls.

---

### 🕵️ `leak_detector.hpp`

A leak detector that globally overloads `new`/`delete` to track total memory usage via a singleton `MemoryAccountant`. Usage can be toggled per build or globally.

---

### 🔄 `shared_ptr.hpp` / `unique_ptr.hpp`

Custom implementations of smart pointers, built from the ground up. 

Concepts:
* Ownership semantics
* Move/copy mechanics
* RAII and exception safety

---

## 📚 Book Reference

All experiments are derived from:
- ** C++ Memory Management** by *Patrice Roy*
- [📎 GitHub Repository (official book code)](https://github.com/PacktPublishing/C-Plus-Plus-Memory-Management/tree/main)
