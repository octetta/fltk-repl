# =============================================================================
# fltk-repl Makefile
# =============================================================================

.PHONY: all clean run info help dist skrepl krepl repl_demo all-full skred-info

ifeq ($(shell uname -s),Darwin)
REPL_BIN := ./build/skrepl.app/Contents/MacOS/skrepl
else
REPL_BIN := ./build/skrepl
endif

# Default target: configure and build common targets (replfltk + defaults)
all:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	@cmake --build build -j

# Full build: build everything including skrepl, krepl and repl_demo
all-full:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKRED_BUILD_REPL=ON -DREPL_BUILD_DEMO=ON
	@cmake --build build -j

# Build specific executables
skrepl:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKRED_BUILD_REPL=ON
	@cmake --build build --target skred_repl -j

krepl:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKRED_BUILD_REPL=ON
	@cmake --build build --target krepl -j

repl_demo:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DREPL_BUILD_DEMO=ON -DSKRED_BUILD_REPL=OFF
	@cmake --build build --target repl_demo -j

# Build and run
run: skrepl
	@$(REPL_BIN)

# Clean build directory
clean:
	rm -rf build

# Show configuration info (especially Skred detection)
info:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKRED_BUILD_REPL=ON > /dev/null 2>&1 || true
	@echo "=== Build Configuration ==="
	@grep -E "(SKRED_ROOT|SKRED_BUILD_REPL|REPL_FLTK|CMAKE_BUILD_TYPE)" build/CMakeCache.txt 2>/dev/null || echo "No CMakeCache yet. Run 'make' first."
	@echo ""
	@echo "To see full Skred detection log, run:"
	@echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Configure and build everything (default)"
	@echo "  all-full - Configure and build all executables (skrepl, krepl, repl_demo)"
	@echo "  skrepl   - Build skrepl executable (requires SKRED)"
	@echo "  krepl    - Build krepl executable"
	@echo "  repl_demo- Build the repl_demo example"
	@echo "  run      - Build and run skrepl"
	@echo "  clean    - Remove build directory"
	@echo "  info     - Show detected Skred package and configuration"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Useful CMake options:"
	@echo "  cmake -S . -B build -DSKRED_ROOT=/path/to/skred-X.Y.Z-maxed"
	@echo "  cmake -S . -B build -DSKRED_BUILD_REPL=OFF"
	@echo "  cmake -S . -B build -DREPL_BUILD_DEMO=ON"

# Convenience for developers
dist: clean
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	@cmake --build build -j
	@echo ""
	@echo "Build complete. Application is at $(REPL_BIN)"

# Print current detected Skred root (quick check)
skred-info:
	@if [ -d build ]; then \
		grep SKRED_ROOT build/CMakeCache.txt || echo "SKRED_ROOT not set in current build"; \
	else \
		echo "No build directory yet. Run 'make' first."; \
	fi
