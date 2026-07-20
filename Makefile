all: repl_demo

repl_demo:
	mkdir -p build && \
  cd build && \
  cmake .. && \
  make

clean:
	rm -rf build
