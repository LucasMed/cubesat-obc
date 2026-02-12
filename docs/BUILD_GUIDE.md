# Build Guide

Prerequisites:

- `cmake` and a C compiler

Build steps:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Run unit tests:

```bash
ctest --output-on-failure
```
