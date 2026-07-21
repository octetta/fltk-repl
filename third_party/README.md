# third_party/

This directory is intentionally empty by default.

By default the build fetches FLTK automatically at `cmake` configure
time (see `cmake/FetchFLTK.cmake`) -- you don't need to put anything
here.

If you want a fully offline/air-gapped build, or want to pin an exact
FLTK checkout yourself, drop (or symlink) a full FLTK source tree at:

    third_party/fltk/

so that `third_party/fltk/CMakeLists.txt` exists. The build will
detect it automatically and use it instead of downloading, e.g.:

    git clone --branch release-1.4.5 https://github.com/fltk/fltk.git third_party/fltk
    cmake -B build
    cmake --build build -j

This is a plain directory copy, not a git submodule, so `git clone`
of this repo never requires `--recurse-submodules` or a separate
`git submodule update` step either way.
