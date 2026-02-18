# libmodbus-cpp

Modern C++23 wrapper for `libmodbus`.

## Prerequisites

Default configuration uses `FetchContent` and builds `libmodbus` from the upstream git repository.

- CMake 3.25+
- C++23 compiler
- `git`
- Autotools prerequisites for building `libmodbus` from upstream git source:
	- `autoconf`
	- `automake`
	- `libtool`
	- `make`
	- C compiler (for example `gcc`)

Example dependency install (Debian/Ubuntu):

```bash
sudo apt update
sudo apt install -y cmake git autoconf automake libtool make gcc
```

Fedora/RHEL:

```bash
sudo dnf install -y cmake git autoconf automake libtool make gcc
```

Arch Linux:

```bash
sudo pacman -S --needed cmake git autoconf automake libtool make gcc
```

## Build

Use a fresh build directory, especially when switching compilers:

```bash
rm -rf build
```

Build:

```bash
cd libmodbus-cpp
rm -rf build
cmake -S . -B build -D CMAKE_BUILD_TYPE=Debug
cmake --build build -j4
```

### Use installed libmodbus (explicit opt-in)

By default, the project uses `FetchContent`.
To use an already installed system `libmodbus`, enable the option below:

```bash
cmake -S . -B build -DLIBMODBUS_USE_SYSTEM=ON
cmake --build build -j4
```

When using `LIBMODBUS_USE_SYSTEM=ON`, you need:

- `pkg-config`
- installed `libmodbus` development package (for example `libmodbus-dev` on Debian/Ubuntu)

### Optional: skip tool checks

By default, CMake checks for `autoreconf` and `make` and fails early with install hints if they are missing.
In controlled environments (for example CI images where tools are guaranteed), you can skip these checks:

```bash
cmake -S . -B build -DLIBMODBUS_SKIP_TOOL_CHECK=ON
```
