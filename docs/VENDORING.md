# Vendoring `fltk-repl` Guide

`fltk-repl` is designed to be easily vendored into host application repositories, language runtimes (**Elixir**, **Forth**, **Python**, **Common Lisp**, **Lua**, **Scheme**, **Rust**), and desktop tools.

By vendoring `fltk-repl`, your project gains a self-contained, offline-capable, cross-platform desktop GUI terminal with ANSI 256-color support, Unicode Braille graphics, dynamic parameter panels (`panel_dsl`), standalone bitmap windows (`bitmap_win`), and thread-safe FFI bridges (`foreign_bridge`).

---

## Vendoring Strategies

### Strategy 1: CMake Submodule & `add_subdirectory()` (Recommended for C/C++ & CMake Projects)

If your host project uses CMake, vendor `fltk-repl` as a Git submodule or subdirectory under `vendor/` or `third_party/`:

```sh
# Add fltk-repl as a submodule in your repository
git submodule add https://github.com/octetta/fltk-repl.git vendor/fltk-repl
```

In your host `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_host_app LANGUAGES C CXX)

# Turn off Skred and Demo executable targets when vendored
set(SKRED_BUILD_REPL OFF CACHE BOOL "" FORCE)
set(REPL_BUILD_DEMO OFF CACHE BOOL "" FORCE)

# Add vendored fltk-repl subdirectory
add_subdirectory(vendor/fltk-repl)

# Create your application executable or library
add_executable(my_host_app src/main.c)

# Link against the fltk_repl::replfltk target
target_link_libraries(my_host_app PRIVATE fltk_repl::replfltk)
```

#### Why this works seamlessly:
- `fltk_repl::replfltk` automatically exports the public C header search path (`vendor/fltk-repl/include`).
- FLTK 1.4 is fetched or linked automatically according to your configuration.
- Your code simply includes `#include <repl/repl_api.h>` without hardcoding relative include paths.

---

### Strategy 2: Dynamic Shared Library (`libreplfltk.so` / `.dylib` / `.dll`) for FFI Runtimes

For interpreted or dynamic language runtimes (**Elixir BEAM NIFs/Ports**, **Gforth / VFX Forth**, **Python `ctypes`**, **Lua FFI**, **Common Lisp CFFI**), build `libreplfltk` as a shared dynamic library:

```sh
# Build shared dynamic library without Skred
cmake -B build -DBUILD_SHARED_LIBS=ON -DSKRED_BUILD_REPL=OFF -DREPL_BUILD_DEMO=OFF
cmake --build build -j
```

This produces:
- **Linux**: `build/libreplfltk.so`
- **macOS**: `build/libreplfltk.dylib`
- **Windows**: `build/replfltk.dll`

#### Integrating with Package Managers:

- **Elixir (Mix & `elixir_make`)**: Add a custom `Makefile` task in your Mix project that invokes `cmake -B build -DBUILD_SHARED_LIBS=ON -DSKRED_BUILD_REPL=OFF` during `mix compile`.
- **Python (Wheel / Setuptools)**: Include the compiled `libreplfltk.so`/`.dylib`/`.dll` inside your Python package directory so `ctypes.CDLL(os.path.join(__file__, "libreplfltk.so"))` loads cleanly on user machines.
- **Forth Packages**: Include `libreplfltk.so` in your Forth package directory and load via `c-library`.

---

### Strategy 3: Plain Makefile / Non-CMake Build Systems

If your host project uses a plain `Makefile`:

```makefile
FLTK_REPL_DIR = vendor/fltk-repl
CFLAGS        += -I$(FLTK_REPL_DIR)/include
CXXFLAGS      += -I$(FLTK_REPL_DIR)/include -std=c++17
LDFLAGS       += -L$(FLTK_REPL_DIR)/build -lreplfltk -lfltk_images -lfltk -lX11 -lfontconfig -lXft -lfontconfig -lpthread -ldl -lm

all: my_app

my_app: main.o
	$(CXX) main.o -o my_app $(LDFLAGS)
```

---

### Strategy 4: 100% Air-Gapped / Offline Vendoring

To build `fltk-repl` in air-gapped CI/CD environments without network access:

1. **Pre-populate FLTK Source**: Place the FLTK 1.4 source tree inside `vendor/fltk-repl/third_party/fltk/` or pass `-DREPL_FLTK_DIR=/path/to/fltk`.
2. **Disable Network Fallback**: Pass `-DREPL_FETCH_FLTK=OFF` and `-DSKRED_BUILD_REPL=OFF` during configure:

```sh
cmake -S vendor/fltk-repl -B build \
    -DREPL_FLTK_DIR=/path/to/fltk \
    -DREPL_FETCH_FLTK=OFF \
    -DSKRED_BUILD_REPL=OFF
```

---

## Directory & Header File Map for Vendoring

When vendoring `fltk-repl`, ship the following directory layout in your repository:

```text
vendor/fltk-repl/
├── CMakeLists.txt              # Primary CMake build configuration
├── VERSION                     # Semantic version tag
├── include/repl/               # Pure C99 public header API
│   ├── repl_api.h              # Core REPL functions (repl_create, repl_print, repl_run)
│   ├── foreign_bridge.h        # Thread-safe thread marshalling (foreign_bridge_dispatch)
│   ├── panel_dsl.h             # GUI parameter panel DSL runtime
│   ├── bitmap_win.h            # Bitmap graphics and audio metric windows
│   └── repl_prefs.h            # Cross-platform preferences
├── src/                        # Core C++ implementation files
├── cmake/                      # CMake helper modules & macOS plist template
└── third_party/pikchr/         # Vendored Pikchr SVG diagram generator (C source)
```

---

## Summary Checklist for Host Projects

- [x] Copy or submodule `fltk-repl` into `vendor/fltk-repl`.
- [x] Set `SKRED_BUILD_REPL=OFF` and `REPL_BUILD_DEMO=OFF` in CMake or build script.
- [x] Include `<repl/repl_api.h>` in your host application or FFI binding wrapper.
- [x] Register custom line evaluation handler via `repl_set_fallback_handler()`.
- [x] Call `repl_run()` on the main OS thread.
