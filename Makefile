CFLAGS = -O3 -fopencilk -fcilktool=cilkscale
LDLIBS = -lopencilk -lm -lmatio

all: cc

cc: cc.c cca.h
	clang $(CFLAGS) cc.c -o cc $(LDLIBS)

bench: cc.c cca.h
	clang -O3 -fopencilk cc.c -o cc $(LDLIBS)

clean:
	rm -f cc
