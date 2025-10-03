---
layout: page
title: Contribution Guidelines
parent_section: Developer Documentation
---

# Contribution Guidelines

## Introduction
Contributions to the HydroChrono project are highly valued and appreciated.

This guide aims to provide concise guidance and best practices to follow when contributing, by combining aspects of:
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [Effective Modern C++ (Scott Meyers)](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- [Writing Scientific Software (Suely Oliveira, David E. Stewart)](https://www.cambridge.org/core/books/writing-scientific-software/23206704175AF868E43FE3FB399C2F53)


> Scope and legacy
> - These are target standards for quality, consistency, readability and maintainability.
> - However, parts of the codebase predate these conventions and need a dedicated refactoring effort. When touching code:
>   - Apply the standards to new and changed code.
>   - Opportunistically refactor nearby code (where low-risk).


## Core principles

- Clarity first; write for future readers.
- Keep it minimal.
- Make it testable.
- Separate concerns: physics, I/O, UI.
- Ensure reproducibility.
- Maintain portability.

## Google C++ Style Guide - Key Highlights

We follow the Google C++ Style Guide; below are some key points:

| Topic | Rule | Example |
| ----- | ---- | ------- |
| General naming | Prefer clarity/descriptiveness; avoid uncommon abbreviations | Prefer `number_of_steps` over `numStps`<br>Prefer `output_directory` over `outDir` (except common terms like `id`) |
| File names | Lowercase with underscores | `hydro_forces.cpp`, `wave_types.h`<br>Avoid `HydroForces.cpp`, `waveTypes.h` |
| Type names | PascalCase (UpperCamelCase) | `class H5Writer { ... };`<br>`struct SimulationConfig { ... };` |
| Variable names | snake_case | `std::string output_path;`<br>`int num_samples = 0;` (avoid `op`, `n`) |
| Class members | Trailing underscore | `std::string file_path_;`<br>`double mass_ = 0.0;` |
| Function names | PascalCase (UpperCamelCase) | `ComputeForces()`, `LoadFromYaml()`<br>Avoid noun names like `ForceComputation()` |
| Constants | `k` prefix + MixedCase | `constexpr int kMaxIterations = 1000;`<br>`constexpr double kPi = 3.14159;` |
| Namespaces | Lowercase, project-based | `namespace hydroc { /* ... */ }` |
| Enumerations | Type in PascalCase; enumerators in Pascal/Camel as appropriate | `enum class WaveType { Regular, Irregular };`<br>`WaveType wave = WaveType::Regular;` |
| Using `auto` | Use for noisy/obvious types; avoid when type is unclear | Good: `auto it = v.begin();`, `auto p = std::make_unique<T>();`<br>Bad: `auto count = ComputeCount();` (prefer explicit type) |

---

## Best Practices Summary

| Area | MUST | SHOULD |
| ---- | ---- | ------ |
| File structure | - Public headers in `include/hydroc/` with include guards<br>- `.cpp` includes own header first | - Headers self-contained<br>- Include-what-you-use<br>- Avoid heavy third‑party in public headers<br>- No `using namespace` in headers |
| Naming & style | - Types/functions `PascalCase`<br>- Variables `snake_case`<br>- Members end with `_`<br>- Constants `kPascalCase` | - Descriptive names<br>- Prefer `enum class`<br>- Prefer `constexpr` over macros |
| Documentation | - Use Doxygen tags as needed (`@brief`, `@param`, `@return`, `@pre`, `@throws`, `@note`) | - Document units/encoding/thread‑safety<br>- Explain “why” over “what” |
| Error handling & logging | - Catch specific exceptions<br>- Destructors must not throw<br>- Use `LOG_*` for user messages | - Add context (operation, path, sizes)<br>- Avoid `catch(...)` except to log+rethrow |
| Ownership & API | - No raw `new`/`delete`<br>- RAII<br>- `std::unique_ptr` for ownership | - Large params by `const&`<br>- Use `string_view` internally for read‑only text<br>- `[[nodiscard]]`/`noexcept` where appropriate<br>- Keep core APIs minimal |
| Concurrency | - Assume not thread‑safe unless documented<br>- Avoid mutable globals | - Document what is thread‑safe<br>- Pass context explicitly |
| Testing | - Maintain unit tests<br>- Deterministic seeds<br>- FP comparisons use tolerances | - Run across platforms in CI<br>- Record provenance (SHA, config, inputs) when applicable |
| Performance | - Profile before optimizing | - Minimize allocations in hot loops<br>- Prefer contiguous layouts<br>- Avoid hidden deps in parallel/MPI code |
| Tooling & build | - Strong warnings<br>- Sanitizers in CI | - Use `.clang-format`/`.clang-tidy`<br>- Avoid drive‑by reformatting |
| Workflow | - Small focused commits with clear messages<br>- Open PRs for review | - Branch naming `feature/`, `fix/`, `major/`<br>- Keep PR scope reviewable in 30-60 min |

## Project structure

- `src/` - core implementation
- `include/` - public headers installed for consumers
- `app/` - thin CLI/frontends built on top of `HydroChrono` (arg parsing, I/O)
- `tests/` - automated tests (unit + regression)
- `demos/` - runnable examples
- `docs/` - documentation


## Templates

#### Minimal header template (public)
```cpp
#ifndef HYDROC_FOO_H
#define HYDROC_FOO_H

/**
 * @file foo.h
 * @brief Brief description of Foo.
 * @note Key constraints and assumptions (units, encoding, thread-safety).
 */

#include <string>

namespace hydroc {

class Foo {
  public:
    explicit Foo(const std::string& name);
    ~Foo() noexcept;

    /** @brief Get the name. */
    const std::string& GetName() const noexcept { return name_; }

  private:
    std::string name_;
};

} // namespace hydroc

#endif // HYDROC_FOO_H
```

#### Minimal source template
```cpp
/**
 * @file foo.cpp
 * @brief Implementation of Foo.
 */

// Configure include paths via CMake; include installed or build-tree headers uniformly.
#include <hydroc/foo.h>

#include <stdexcept>

namespace hydroc {

Foo::Foo(const std::string& name) : name_(name) {}
Foo::~Foo() noexcept = default;

} // namespace hydroc
```


## Submitting Contributions

1. **Fork and Clone**:
   - Fork the HydroChrono repository on the platform where it's hosted (e.g., GitHub).
   - Clone your forked repository to your local machine.

2. **Create a New Branch**:
   - Create a new branch for each new feature or bugfix. Naming it descriptively can help, e.g., `feature-add-hydro-dynamics` or `bugfix-memory-leak`.

3. **Write Code**
   - Adhere to the code guidelines above and the full Google C++ Style Guide.
   - Provide comprehensive comments for any non-trivial code.

4. **Committing Changes**:
   - Write clear, concise commit messages that explain the change.
   - Split larger changes into multiple commits if possible.

5. **Syncing with Upstream**:
   - Regularly sync your fork and branch with the main repository (`upstream`) to keep up with changes.
   - Merge or rebase your branch with the latest from `upstream` before submitting a pull request.

6. **Testing**:
   - Ensure that your code passes all tests and doesn't introduce new issues.
   - Add new tests for your features to ensure future changes don't break your contribution.

7. **Documentation**:
   - Update relevant documentation pertaining to your changes.
   - Ensure examples, if provided, are clear and understandable.

8. **Code Reviews**:
   - Once you've pushed your branch to your fork, submit a pull request to the main HydroChrono repository.
   - Participate in the code review process. Respond to feedback, and make changes as requested.

### PR checklist

- [ ] File header Doxygen block with `@file`, `@brief`, key `@note`s
- [ ] Public header uses include guard; `.cpp` includes own header first
- [ ] Naming follows conventions; units/encoding documented
- [ ] Ownership and lifetime clear; destructors `noexcept` where reasonable
- [ ] Exceptions are specific; error messages add context
- [ ] Logging used for user-facing messages (no stray `cout`)
- [ ] Tests updated/added when logic changes