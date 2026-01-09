# Style Guide & Agent Rules

All code must adhere to the **Antigravity Framework** rules defined in `.agent/rules/`.

## C++ Standards (C++20)

### 1. Ownership & Lifetime (`cpp-core.rules.yaml`)

- **Rule**: Never use `new` or `delete` manually.
- **Why**: Leaks are unacceptable. Use `std::unique_ptr` or `std::shared_ptr`.
- **Exception**: Inside custom `std::pmr::memory_resource` implementations.

### 2. Move Semantics

- **Rule**: Move constructors must be `noexcept`.
- **Reason**: `std::vector` (and our arenas) will fall back to copying if move is not guaranteed safe.

### 3. Factory Functions

- **Rule**: Must be `[[nodiscard]]`.
- **Reason**: Creating a resource and ignoring the handle is always a bug.

```cpp
// Good
[[nodiscard]] Entity create_entity();

// Bad
Entity create_entity();
```

---

## Vulkan & OpenGL (`vulkan.rules.yaml`)

### 1. Validation Layers

- **Rule**: Validation errors are treated as crashes.
- **Enforcement**: CI will fail if the validation layer emits any error during testing.

### 2. Synchronization

- **Rule**: All barriers must explicit stage masks.
- **Forbidden**: `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` (unless debugging).

---

## Naming Conventions

- **Types**: `PascalCase` (e.g., `MeshComponent`)
- **Variables/Functions**: `snake_case` (e.g., `create_entity`, `m_world`)
- **Macros**: `UPPER_SNAKE_CASE` (e.g., `HZ_ASSERT`)
- **Namespaces**: `lower_case` (e.g., `hz`, `hz::render`)
