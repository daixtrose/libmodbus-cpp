# Building libmodbus v3.1.12 via CMake ExternalProject on GitHub Actions Windows (MSYS2/MinGW-w64)

## Setup

**Project**: [libmodbus-cpp](https://github.com/daixtrose/libmodbus-cpp) — a C++23 wrapper around the [libmodbus](https://github.com/stephane/libmodbus) C library (v3.1.12).

**Build system**: CMake 4.2.3 with `CMakePresets.json` (schema v8). The C library libmodbus uses autotools and has no CMakeLists.txt of its own. The wrapper project uses `FetchContent` to download the libmodbus source at configure time, then `ExternalProject_Add` to run its autotools build (configure/make/make install) at build time.

**CI environment**: GitHub Actions `windows-latest` runner with `msys2/setup-msys2@v2`, `MINGW64` msystem. Installed packages:
- MinGW: `mingw-w64-x86_64-gcc` (GCC 15.2.0), `mingw-w64-x86_64-cmake`
- MSYS: `autoconf`, `automake`, `libtool`, `make`, `pkg-config`, `gettext`, `git`

**CMake generator**: `"MSYS Makefiles"` (not Ninja, not MSVC).

The Linux CI (Ubuntu 24.04, GCC 14, shared library) works without issues. The problems are Windows-specific.

---

## Problem 1: `aclocal` cannot find `progtest.m4`

### Symptom

During `cmake --build`, ExternalProject runs `autoreconf -fi` (via `./autogen.sh`) as part of the configure step. This fails with:

```
aclocal-1.18: error: aclocal: file '/a/_temp/msys64/usr/share/aclocal/progtest.m4' does not exist
autoreconf: error: aclocal failed with exit status: 1
```

The path `/a/_temp/msys64/usr/share/aclocal/progtest.m4` is wrong — on the GitHub Actions runner, MSYS2 is installed at `D:\a\_temp\msys64`, so the real path should be `D:\a\_temp\msys64\usr\share\aclocal\progtest.m4` (or `/usr/share/aclocal/progtest.m4` from within MSYS2).

### Attempted fixes that did NOT work

1. **Adding `gettext-devel` to MSYS2 packages** — The `progtest.m4` file is provided by the `gettext` package (not `gettext-devel`). Confirmed that `ls /usr/share/aclocal/progtest.m4` succeeds in a plain MSYS2 shell step.

2. **Passing `-I /usr/share/aclocal` to autoreconf** — Did not help; the problem is not about the search path but about path resolution within aclocal's Perl code.

3. **Setting `ACLOCAL='aclocal --system-acdir=/usr/share/aclocal'` as environment variable** — Same issue; aclocal's internal Perl `abs_path()` still produces the wrong path in the MinGW process context.

4. **Removing the `bash -c` wrapper** — The WIN32 branch originally used `bash -c "autoreconf -fi && ./configure ..."` as the CONFIGURE_COMMAND. Replacing it with a direct command list (`./autogen.sh COMMAND ./configure ...`) did not help because the process context is the same either way.

### Root cause

CMake's `ExternalProject_Add` does **not** run its `CONFIGURE_COMMAND` through make's shell. Instead, it wraps the command in a generated script (`cmake -P ...impl.cmake`) which calls `execute_process()`. On Windows, MinGW CMake's `execute_process()` creates child processes in the **Windows/MinGW context**, not in the MSYS2 context.

This was confirmed by a diagnostic CI run which showed:
- Running `autoreconf -fi` **directly in an MSYS2 shell step** → **succeeds**
- Running it **through ExternalProject** (via `execute_process()`) → **fails** with the mangled path

The MSYS2 path translation layer (which the MSYS2 shell provides) is not active when processes are spawned by MinGW CMake's `execute_process()`. Aclocal, written in Perl, uses `Cwd::abs_path()` to resolve `/usr/share/aclocal/` — but in the MinGW context without MSYS2 path translation, this resolves to `/a/_temp/msys64/usr/share/aclocal/` (the MSYS2 root is `D:\a\_temp\msys64` which gets mapped to `/a/_temp/msys64` in MSYS path notation, then `/usr/share/aclocal` is appended, producing a doubled path).

### Working fix

Split the work: run `autoreconf` (via `./autogen.sh`) in a **CI workflow step** (which runs in a proper MSYS2 shell) **between** `cmake --preset` (configure) and `cmake --build` (build). The ExternalProject `CONFIGURE_COMMAND` only runs `./configure` (which is a generated shell script that doesn't need aclocal/autoreconf):

```yaml
- name: Configure
  run: cmake --preset windows-x86_64-release

- name: Prepare autotools
  run: |
    cd build/windows-x86_64-release/_deps/libmodbus-src
    ./autogen.sh

- name: Build
  run: cmake --build --preset windows-x86_64-release --parallel
```

CMakeLists.txt WIN32 branch:
```cmake
set(_LIBMODBUS_CONFIGURE_COMMAND
    ./configure --prefix=${LIBMODBUS_INSTALL_PREFIX} --enable-static --disable-shared
)
```

This resolved Problem 1. `./configure` completes successfully and `make` starts compiling.

---

## Problem 2: `setsockopt` incompatible pointer type (GCC 15)

### Symptom

After Problem 1 is solved, `make` compiles `modbus-tcp.c` and fails with:

```
modbus-tcp.c:239:50: error: passing argument 4 of 'setsockopt' from incompatible pointer type [-Wincompatible-pointer-types]
  239 |     rc = setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(int));
      |                                                  ^~~~~~~
      |                                                  |
      |                                                  int *
D:/a/_temp/msys64/mingw64/include/winsock2.h:1035:88: note: expected 'const char *' but argument is of type 'int *'
```

Same error at lines 601 and 718 for `&enable`.

### Cause

Winsock2's `setsockopt()` signature uses `const char *optval` (a Microsoft API difference from POSIX which uses `const void *optval`). libmodbus v3.1.12 passes `int *` — this is valid C with an implicit conversion on POSIX, and older GCC versions on Windows only warned about it. **GCC 15 promotes `-Wincompatible-pointer-types` to an error by default.**

This is a known upstream issue in libmodbus — the upstream project only runs CI on Ubuntu and does not test on Windows/MinGW.

### Attempted fixes that did NOT work

1. **Passing `CFLAGS` as argument to `./configure`** in the CMake CONFIGURE_COMMAND:
   ```cmake
   set(_LIBMODBUS_CONFIGURE_COMMAND
       ./configure --prefix=... --enable-static --disable-shared
       "CFLAGS=-O2 -Wno-incompatible-pointer-types"
   )
   ```
   This did not work because CMake splits the list elements into separate `execute_process()` arguments. The `CFLAGS=...` was passed as a separate argument to `./configure` rather than as an environment variable assignment.

2. **Using `cmake -E env` to set CFLAGS** before `./configure`:
   ```cmake
   set(_LIBMODBUS_CONFIGURE_COMMAND
       ${CMAKE_COMMAND} -E env "CFLAGS=-O2 -Wno-incompatible-pointer-types"
       ./configure --prefix=... --enable-static --disable-shared
   )
   ```
   This failed with `inappropriate file type or format` because `cmake -E env` tries to use `CreateProcess()` to run `./configure`, which is a shell script — not a Windows executable. On Linux this works because the kernel honors the shebang (`#!/bin/sh`), but Windows `CreateProcess()` cannot execute shell scripts directly.

3. **Setting `CFLAGS` as environment variable in the GitHub Actions Build step**:
   ```yaml
   - name: Build
     run: cmake --build --preset windows-x86_64-release --parallel
     env:
       CFLAGS: "-O2 -Wno-incompatible-pointer-types"
   ```
   This did not work because libmodbus's `configure.ac` / generated `configure` script sets its own `CFLAGS` internally (visible in the configure output: `cflags: -O2 -Wall -Wmissing-declarations ...`), overriding any environment `CFLAGS` inherited from the parent process.

### Current approach (not yet tested)

Patch the source with `sed` in the CI step, adding `(const char *)` casts to the `setsockopt` calls in `modbus-tcp.c`:

```yaml
- name: Prepare autotools
  run: |
    cd build/windows-x86_64-release/_deps/libmodbus-src
    ./autogen.sh
    # Fix GCC 15 -Wincompatible-pointer-types error: Winsock2 setsockopt
    # expects const char* for optval, but libmodbus v3.1.12 passes int*.
    sed -i '/setsockopt/{s/&option/(const char *)\&option/g; s/&enable/(const char *)\&enable/g}' src/modbus-tcp.c
```

This has **not yet been pushed or verified on CI**.

---

## Summary of all CI run attempts (chronological)

| # | Change | Result |
|---|--------|--------|
| 1 | Initial Windows job with `bash -c "autoreconf && ./configure"` | `aclocal: progtest.m4 not found` |
| 2 | Add `gettext-devel` to MSYS2 packages | Same aclocal error |
| 3 | Change `gettext-devel` to `gettext` | Same aclocal error |
| 4 | Add `-I /usr/share/aclocal` to autoreconf | Same aclocal error |
| 5 | Set `ACLOCAL='aclocal --system-acdir=/usr/share/aclocal'` env | Same aclocal error |
| 6 | Diagnostic CI run (dump tools, paths, run autoreconf manually) | **autoreconf succeeds in MSYS2 shell, fails in ExternalProject** |
| 7 | Remove `bash -c` wrapping, use direct command list | Same aclocal error |
| 8 | Pre-run `./autogen.sh` in CI step between configure and build | **aclocal/configure fixed!** New error: `setsockopt` type mismatch |
| 9 | Add `CFLAGS=...` as argument to `./configure` in CMakeLists.txt | Same setsockopt error (CFLAGS not picked up) |
| 10 | Use `cmake -E env CFLAGS=... ./configure` | `inappropriate file type or format` (can't exec shell script) |
| 11 | Set `CFLAGS` as env var on the Build workflow step | Same setsockopt error (configure overrides CFLAGS) |
| 12 | Patch source with `sed` to add `(const char *)` casts | **Not yet tested** |
