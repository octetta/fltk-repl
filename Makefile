# =============================================================================
# fltk-repl Makefile
# =============================================================================

.PHONY: all clean run info help dist

# Default target
all:
	@cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	@cmake --build build -j

# Build and run
run: all
	@./build/skred_repl

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
	@echo "  run      - Build and run skred_repl"
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
	@echo "Build complete. Binary is at ./build/skred_repl"

# Print current detected Skred root (quick check)
skred-info:
	@if [ -d build ]; then \
		grep SKRED_ROOT build/CMakeCache.txt || echo "SKRED_ROOT not set in current build"; \
	else \
		echo "No build directory yet. Run 'make' first."; \
	fi
