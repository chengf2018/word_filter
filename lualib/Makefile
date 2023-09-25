# wordfilter 代码目录
WORD_FILTER_INC=../
# lua 代码目录
LUA_INC=../../lua-5.4.4/src/
# windows下DLL的路径
LUA_WIN_DLL=$(LUA_INC)lua54.dll

##LUA_CLIB=../../lua-5.4.4/src/liblua.a
CFLAGS= -g -O2 -Wall -fPIC -I$(LUA_INC) -I$(WORD_FILTER_INC) -std=gnu99
SHARED= --shared

all : linux
linux : wordfilter.so
windows : wordfilter.dll

#Unix系统静态连接，导出符号。windows lua库导出到DLL后动态连接，$(LUA_INC)lua54.dll
wordfilter.so : $(WORD_FILTER_INC)word_filter.c lua-wordfilter.c
	gcc $(CFLAGS) $(SHARED) $^ -o $@ -lpthread

wordfilter.dll : $(WORD_FILTER_INC)word_filter.c lua-wordfilter.c $(LUA_WIN_DLL)
	gcc $(CFLAGS) $(SHARED) $^ -o $@ -lpthread

clean :
	rm -rf ./wordfilter.so ./wordfilter.dll