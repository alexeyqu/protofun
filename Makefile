CXXFLAGS := -fPIC -O2 -std=c++11
INC := -I/usr/lib/llvm-3.9/include
SRCS = protofun.cpp

TMPDIR = /tmp
LOCK_FILE = $(TMPDIR)/mylock
# RES_FILE = $(TMPDIR)/result.txt
ROOT = `pwd`/
RES_FILE = $(ROOT)callgraph.dot
PS_FILE = $(ROOT)callgraph.ps
PNG_FILE = $(ROOT)callgraph.png

CLANGPP = clang++-3.9
CLANGPP_FLAGS = -std=c++11 -fsyntax-only 
CLANGPP_DEBUG = -Xclang -ast-dump 
CLANGPP_OPTS = -fplugin=./libprotofun.so -Xclang -plugin-arg-protofun -Xclang $(ROOT) -Xclang -plugin-arg-protofun -Xclang $(RES_FILE) -Xclang -plugin-arg-protofun -Xclang $(LOCK_FILE)

build: libprotofun.so

protofun.o: $(SRCS)
	$(CXX) $(CXXFLAGS) $(INC) -c $^ -o $@

libprotofun.so: protofun.o
	$(CXX) -shared $^ -o $@

clean:
	$(RM) libprotofun.so *.o *.dot *.ps *.png

test: libprotofun.so
	touch $(LOCK_FILE)
	$(RM) $(RES_FILE)
	$(CLANGPP) -v $(CLANGPP_FLAGS) $(CLANGPP_OPTS) -c tests/foo.cpp
	$(RM) $(LOCK_FILE)
	dot -Tps $(RES_FILE) -o $(PS_FILE)
	dot -Tpng $(RES_FILE) -o $(PNG_FILE)

	# diff $(RES_FILE) tests/foo.ref
	# $(RM) $(RES_FILE)

.PHONY: build clean
