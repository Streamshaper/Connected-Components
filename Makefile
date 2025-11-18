CFLAGS = -O3 -fopenmp
LDLIBS = -lm -lmatio

all: cc

cc: cc.c cca.h
	gcc $(CFLAGS) cc.c -o cc $(LDLIBS)

clean:
	rm -f cc
