CFLAGS = -g -O2 -fPIC -Wall -fPIC -std=gnu99

all : word_filter.a test

word_filter.o : word_filter.c
	gcc $(CFLAGS) -c $^ -o $@

word_filter.a : word_filter.o
	ar cr $@ $^

lib : word_filter.a

test : test.c word_filter.a
	gcc $(CFLAGS) $^ -o $@