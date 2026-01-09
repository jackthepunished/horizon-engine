---
description: Example workflow - how to build the project
---

# Build Workflow

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

// turbo
Run tests:

```bash
cd build && ctest --output-on-failure
```
