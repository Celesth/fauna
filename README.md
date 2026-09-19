# Fauna

A small, local version control system written in C++20.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Usage

```bash
fauna init
fauna status
fauna add <file>
fauna commit <file>
```

Fauna stores repository data inside `.fauna/`.
