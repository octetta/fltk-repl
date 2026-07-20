all: skred_repl

skred_repl:
	mkdir -p build && \
  cd build && \
  cmake .. && \
  make

run: skred_repl
	./build/skred_repl

clean:
	rm -rf build
