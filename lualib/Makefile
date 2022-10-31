LUA_INC=../../lua-5.4.4/src
LUA_CLIB=../../lua-5.4.4/src/liblua.a
CFLAGS= -g -Wall -fPIC -I$(LUA_INC) -std=gnu99
SHARED= --shared

all : wordfilter.so

wordfilter.so : lua-wordfilter.c
	gcc $(CFLAGS) $(SHARED) $^ -o $@

clean :
	rm -rf ./wordfilter.so