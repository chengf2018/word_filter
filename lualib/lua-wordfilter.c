#define _CRT_SECURE_NO_WARNINGS
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rwlock.h"

#define MAX_TRIE_SIZE 0xFF
#define MAX_WORD_LENGTH 0xFF //屏蔽字的大小限制
#define MAX_FILTER_NUM 10

#define LOCK(pl) while (__sync_lock_test_and_set(pl,1)) {};
#define UNLOCK(pl) __sync_lock_release(pl);
#define RWLOCKINIT(pl) rwlock_init(pl);
#define RLOCK(pl) rwlock_rlock(pl);
#define RUNLOCK(pl) rwlock_runlock(pl);
#define WLOCK(pl) rwlock_wlock(pl);
#define WUNLOCK(pl) rwlock_wunlock(pl);

typedef int lock;
typedef unsigned char byte;

typedef struct _trie {
	byte data;
	byte nchildren;
	byte capacity;
	byte isword;
	struct _trie *children;
}*trieptr;

typedef struct _str_node {
	char* str;
	struct _str_node* next;
}*strnodeptr;

typedef struct _wordfilter_ctx {
	int ignorecase;
	char mask_word;
	trieptr word_root;
	trieptr skip_word_root;
}*wordfilterctxptr;

static size_t g_memsize = 0;
static wordfilterctxptr g_ctx_instance[MAX_FILTER_NUM] = {NULL};
static lock g_ctx_lock;

static inline void*
word_filter_malloc(size_t size) {
	g_memsize += size;
	return malloc(size);
}

static inline void
word_filter_free(void* p, size_t size) {
	g_memsize -= size;
	free(p);
}

static inline void*
word_filter_realloc(void* p, size_t newsize, size_t oldsize) {
	g_memsize += newsize;
	g_memsize -= oldsize;
	return realloc(p, newsize);
}

static inline char
word_filter_tolower(char c) {
	return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static inline char*
copy_string(const char* str) {
	size_t str_len = strlen(str);
	char * newstr = word_filter_malloc((str_len + 1) * sizeof(char));
	if (!newstr) return NULL;
	strcpy(newstr, str);
	newstr[str_len] = '\0';
	return newstr;
}

static inline trieptr
create_trie() {
	trieptr newtrie = (trieptr)word_filter_malloc(sizeof(*newtrie));
	memset(newtrie, 0, sizeof(*newtrie));
	newtrie->capacity = 1;
	newtrie->children = (trieptr)word_filter_malloc(sizeof(*newtrie));
	memset(newtrie->children, 0, sizeof(*newtrie));
	return newtrie;
}

static inline wordfilterctxptr
create_word_filter_ctx() {
	wordfilterctxptr ctx = (wordfilterctxptr)word_filter_malloc(sizeof(*ctx));
	if (!ctx) return NULL;

	memset(ctx, 0, sizeof(*ctx));
	ctx->ignorecase = 0;
	ctx->word_root = create_trie();
	ctx->skip_word_root = create_trie();
	ctx->mask_word = '*';
	return ctx;
}

static inline void
free_node(trieptr node) {
	if (node && node->children) {
		for (int i = 0; i < node->capacity; i++) {
			trieptr childnode = &node->children[i];
			if (childnode->children) 
				free_node(childnode);
		}
		word_filter_free(node->children, node->capacity * sizeof(*node->children));
		node->children = NULL;
		node->capacity = 0;
		node->nchildren = 0;
	}
}

static inline strnodeptr
insert_str(strnodeptr strnode, const char* str) {
	if (strnode) {
		strnodeptr newstrnode = (strnodeptr)word_filter_malloc(sizeof(*strnode));
		if (!newstrnode) return NULL;
		memset(newstrnode, 0, sizeof(*newstrnode));
		newstrnode->str = copy_string(str);
		newstrnode->next = strnode;
		return newstrnode;
	}
	strnode = (strnodeptr)word_filter_malloc(sizeof(*strnode));
	if (!strnode) return NULL;
	memset(strnode, 0, sizeof(*strnode));
	strnode->str = copy_string(str);
	return strnode;
}

static inline const char*
search_strnode(strnodeptr head, const char* str) {
	if (!str) return NULL;
	while (head) {
		if (strcmp(head->str, str) == 0)
			return head->str;
		head = head->next;
	}
	return NULL;
}

static inline void
free_str_list(strnodeptr strlist) {
	strnodeptr node = strlist;
	while (node) {
		if (node->str)
			word_filter_free(node->str, strlen(node->str) + 1);
		strnodeptr prenode = node;
		node = node->next;
		word_filter_free(prenode, sizeof(*prenode));
	}
}

static inline byte
calcinitsize(byte addsize) {
	for (int i = 1; i <= 8; i++) {
		int size = (1 << i) - 1;
		if (size >= addsize) return (byte)size;
	}
	return (byte)addsize;
}

static int
reserve(trieptr node, byte addsize) {
	if (node->capacity == MAX_TRIE_SIZE) return 1;
	addsize = addsize ? addsize : 1;
	if (!node->children) {
		byte newcapacity = calcinitsize(addsize);
		trieptr newchildren = (trieptr)word_filter_malloc(sizeof(*node->children) * newcapacity);
		if (!newchildren) return 0;
		node->capacity = newcapacity;
		node->children = newchildren;
		memset(node->children, 0, sizeof(*node->children) * node->capacity);
		return 1;
	}
	if (node->nchildren + 1 > node->capacity) {
		byte oldsize = node->capacity;
		byte newcapacity = (node->capacity << 1) + 1;
		trieptr newchildren = (trieptr)word_filter_realloc(node->children,
									sizeof(*node->children) * newcapacity,
									sizeof(*node->children) * oldsize);
		if (!newchildren) return 0;
		node->capacity = newcapacity;
		node->children = newchildren;
		memset(&node->children[oldsize], 0,
			sizeof(*node->children) * (newcapacity - oldsize));
	}
	return 1;
}

static trieptr
add_trie(trieptr node, byte index, byte c, byte isword) {
	if (!reserve(node, 1)) return NULL;

	int movesize = node->nchildren - index;
	if (movesize > 0) {
		memmove(&node->children[index+1], &node->children[index],\
		 movesize * sizeof(struct _trie));
	}
	trieptr newnode = &node->children[index];
	memset((void*)newnode, 0, sizeof(*newnode));
	newnode->data = c;
	newnode->isword = isword;
	node->nchildren++;

	return newnode;
}

static byte
binary_search(trieptr node, byte c, int *exist) {
	if (node->nchildren == 0) {
		if (exist) *exist = 0;
		return 0;
	}
	int l = 0, r = node->nchildren - 1;

	while (l <= r) {
		byte middle = (l + r) >> 1;
		byte data = node->children[middle].data;
		if (data > c) {
			r = middle - 1;
		}
		else if (data == c) {
			if (exist) *exist = 1;
			l = middle;
			break;
		}
		else {
			l = middle + 1;
		}
	}
	return l;
}

static inline int
skip_word(trieptr word_root, const char** str, int ignorecase) {
	if (!str) return 0;
	char c;
	int skipnum = 0;
	const char* wordptr = *str;

	while (1) {
		trieptr node = word_root;
		byte pos_index = 0;
		int find = 0;
		while ((c = *wordptr)) {
			if (ignorecase) c = word_filter_tolower(c);
			int exist = 0;
			byte index = binary_search(node, c, &exist);
			if (!exist) break;

			pos_index++;
			node = &node->children[index];

			if (node->isword) {
				find = pos_index;
			}
			wordptr++;
		}
		if (find) skipnum += find;
		else break;
	}

	if (skipnum)
		*str += skipnum;

	return skipnum;
}

static int
do_insert_word(wordfilterctxptr ctx, trieptr root, const char* word) {
	if (strlen(word) > MAX_WORD_LENGTH) return 0;

	const char* wordptr = word;
	char c;
	trieptr node = root;
	while ((c = *wordptr)) {
		if (ctx->ignorecase) c = word_filter_tolower(c);
		int exist = 0;
		byte index = binary_search(node, c, &exist);
		byte isword = *(wordptr + 1) == '\0';
		if (exist) {
			node = &node->children[index];
			if (isword)
				node->isword = 1;
		}
		else {
			trieptr newnode = add_trie(node, index, c, isword);
			if (!newnode) return 0;
			node = newnode;
		}
		wordptr++;
	}
	return 1;
}

static int
do_search_word(wordfilterctxptr ctx, trieptr word_root, trieptr skip_word_root, const char* word, char** word_key) {
	char c;
	int ignorecase = ctx->ignorecase;
	int find = 0;
	char trie_stack[MAX_WORD_LENGTH + 1] = { 0 };
	byte trie_stack_index = 0;
	trieptr node = word_root;
	const char* wordptr = word;
	int skip_num = 0;
	int exist = 0;
	byte index = 0;

	while ((c = *wordptr)) {
		if (ignorecase) c = word_filter_tolower(c);
		exist = 0;
		index = binary_search(node, c, &exist);
		if (!exist) break;

		trie_stack[trie_stack_index++] = c;
		node = &node->children[index];

		if (node->isword) {
			find = trie_stack_index;
		}
		wordptr++;
		skip_num += skip_word(skip_word_root, &wordptr, ignorecase);
	}

	if (find && word_key) {
		trie_stack[find] = '\0';
		*word_key = copy_string(trie_stack);
	}

	return find ? (find + skip_num) : 0;
}

int
insert_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->word_root, word);
}

int
insert_skip_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->skip_word_root, word);
}

void
clean_ctx(wordfilterctxptr ctx) {
	if (!ctx) return;
	if (ctx->word_root) {
		free_node(ctx->word_root);
	}
	if (ctx->skip_word_root) {
		free_node(ctx->skip_word_root);
	}
}

void free_ctx(wordfilterctxptr ctx) {
	clean_ctx(ctx);
	if (ctx->word_root) {
		word_filter_free(ctx->word_root, sizeof(*ctx->word_root));
	}
	if (ctx->skip_word_root) {
		word_filter_free(ctx->skip_word_root, sizeof(*ctx->skip_word_root));
	}
	word_filter_free(ctx, sizeof(*ctx));
}

int
search_word(wordfilterctxptr ctx, const char* word, char **word_key) {
	return do_search_word(ctx, ctx->word_root, ctx->skip_word_root, word, word_key);
}

int
search_word_ex(wordfilterctxptr ctx, const char* word, strnodeptr* strlist) {
	const char* wordptr = word;
	int find = 0;
	strnodeptr strnode = NULL;
	while (*wordptr) {
		char* string = NULL;
		int ret = strlist ? search_word(ctx, wordptr, &string) 
						  : search_word(ctx, wordptr, NULL);
		if (ret) {
			find = 1; 
			wordptr += ret;
		}
		else {
			wordptr++;
		}

		if (string) {
			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
			word_filter_free(string, strlen(string) + 1);
		}
	}
	if (strlist) {
		*strlist = strnode;
	}
	return find;
}

char*
filter_word(wordfilterctxptr ctx, const char* word, strnodeptr *strlist, int *isfilter) {
	if (!ctx || !word) return NULL;
	const char* wordptr;
	char* wordstartptr;
	char mask_word = ctx->mask_word;
	int str_len = strlen(word) + 1;
	int find = 0, strpos = 0;
	wordptr = word;
	wordstartptr = (char*)word_filter_malloc(str_len*sizeof(char));
	memset(wordstartptr, 0, str_len*sizeof(char));

	strnodeptr strnode = NULL;
	while (*wordptr) {
		char* string = NULL;
		int ret = strlist ? search_word(ctx, wordptr, &string) 
						  : search_word(ctx, wordptr, NULL);
		if (ret) {
			find = 1;
			for (int i = 0; i < ret; i++) {
				char c = *(wordptr + i);
				if (c & 0x80) {
					//多字节编码
					int skip = 1;
					for (int j = 0; j < 3; j++) {
						if (c & (0x40 >> j)) {
							skip++;
						}
						else break;
					}
					i += skip;
				}
				wordstartptr[strpos++] = mask_word;
			}
			wordptr += ret;
		}
		else {
			wordstartptr[strpos++] = *wordptr;
			wordptr++;
		}
		if (string) {
			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
			word_filter_free(string, strlen(string) + 1);
		}
	}

	if (strlist) {
		*strlist = strnode;
	}

	if (isfilter) {
		*isfilter = find;
	}

	return wordstartptr;
}

//忽略大小写需要在insert_word之前调用
void
set_ignore_case(wordfilterctxptr ctx, int is_ignore) {
	ctx->ignorecase = is_ignore;
}

void
set_mask_word(wordfilterctxptr ctx, char mask_word) {
	ctx->mask_word = mask_word;
}





int
lnewctx(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	int ignorecase = lua_tointeger(L, 2);

	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.newctx]: filter id overstep the boundary:[%d]",
						filter_id);
		return 0;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.newctx]: already create filter,filter id:[%d]",
						filter_id);
		return 0;
	}

	ctx = create_word_filter_ctx();
	if (!ctx) {
		luaL_error(L, "[wordfilter.newctx]: alloc context error");
		return 0;
	}

	set_ignore_case(ctx, ignorecase);
	g_ctx_instance[filter_id-1] = ctx;
	UNLOCK(&g_ctx_lock);
	lua_pushboolean(L, 1);
	return 1;
}

int
lcleanctx(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id >= 1 && filter_id < MAX_FILTER_NUM) {
		LOCK(&g_ctx_lock);
		wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
		if (!ctx) {
			UNLOCK(&g_ctx_lock);
			luaL_error(L, "[wordfilter.cleanctx]: filter no created,filter id:[%d]",
							filter_id);
			return 0;
		}
		clean_ctx(ctx);
		UNLOCK(&g_ctx_lock);
	} else {
		luaL_error(L, "[wordfilter.cleanctx]: filter id overstep the boundary:[%d]",
						filter_id);
	}
	return 0;
}

int
lfreectx(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id >= 1 && filter_id < MAX_FILTER_NUM) {
		LOCK(&g_ctx_lock);
		wordfilterctxptr ctx =  g_ctx_instance[filter_id-1];
		if (!ctx) {
			UNLOCK(&g_ctx_lock);
			luaL_error(L, "[wordfilter.freectx]: filter no created,filter id:[%d]",
							filter_id);
			return 0;
		}
		free_ctx(ctx);
		g_ctx_instance[filter_id-1] = NULL;
		UNLOCK(&g_ctx_lock);
	} else {
		luaL_error(L, "[wordfilter.freectx]: filter id overstep the boundary:[%d]",
						filter_id);
	}
	return 0;
}

int
lsetignorecase(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.setignorecase]: filter id overstep the boundary:[%d]",
						filter_id);
		return 0;
	}
	int ignorecase = lua_tointeger(L, 2);
	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.setignorecase]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}
	ctx->ignorecase = ignorecase;
	UNLOCK(&g_ctx_lock);
	return 0;
}

int
lsetmaskword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.setmaskword]: filter id overstep the boundary:[%d]",
						filter_id);		
		return 0;
	}
	int ignorecase = lua_tointeger(L, 2);
	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.setmaskword]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}
	ctx->ignorecase = ignorecase;

	UNLOCK(&g_ctx_lock);
	return 0;
}

int
lupdateskipword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.updateskipword]: filter id overstep the boundary:[%d]",
						filter_id);		
		return 0;
	}
	if (!lua_istable(L, 2)) {
		luaL_error(L, "[wordfilter.updateskipword]: table expect, got type[%s]",
						lua_typename(L, lua_type(L, 2)));
		return 0;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.updateskipword]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}

	int success = 1;
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		const char* word = lua_tostring(L, -1);
		if (!insert_skip_word(ctx, word)) {
			success = 0;
			luaL_error(L, "[wordfilter.updateskipword]: insert word error[%s]",
							word);
			break;
		}

		lua_pop(L, 1);
	}
	UNLOCK(&g_ctx_lock);

	lua_pushboolean(L, success);
	return 0;
}

int
lupdateword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.updateword]: filter id overstep the boundary:[%d]",
						filter_id);
		return 0;
	}

	if (!lua_istable(L, 2)) {
		luaL_error(L, "[wordfilter.updateword]: table expect, got type[%s]",
						lua_typename(L, lua_type(L, 2)));
		return 0;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.updateword]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}

	int success = 1;
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		if (lua_type(L, -1) != LUA_TSTRING) {
			UNLOCK(&g_ctx_lock);
			luaL_error(L, "[wordfilter.updateword]: string expect, got type[%s]",
							lua_typename(L, lua_type(L, -1)));
			return 0;
		}
		const char* word = lua_tostring(L, -1);
		if (!insert_word(ctx, word)) {
			success = 0;
			luaL_error(L, "[wordfilter.updateword]: insert word error[%s]",
							word);
			break;
		}
		lua_pop(L, 1);
	}

	UNLOCK(&g_ctx_lock);
	lua_pushboolean(L, success);
	return 1;
}

int
lfilter(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.filter]: filter id overstep the boundary:[%d]",
						filter_id);
		return 0;
	}
	if (lua_type(L, 2) != LUA_TSTRING) {
		luaL_error(L, "[wordfilter.filter]: string expect, got type:[%s]",
						lua_typename(L, lua_type(L, 2)));
		return 0;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.filter]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}
	const char* word = lua_tostring(L, 2);
	strnodeptr strlist = NULL;
	int isfilter = 0;
	char* newstr = filter_word(ctx, word, &strlist, &isfilter);
	UNLOCK(&g_ctx_lock);

	lua_pushboolean(L, isfilter);
	lua_pushstring(L, newstr);
	lua_newtable(L);
	strnodeptr p = strlist;
	int i = 1;
	while (p) {
		lua_pushstring(L, p->str);
		lua_rawseti(L, -2, i++);
		p = p->next;
	}
	if (newstr) word_filter_free(newstr, strlen(word)+1);
	if (strlist) free_str_list(strlist);
	return 3;
}

int
lcheck(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.check]: filter id overstep the boundary:[%d]",
						filter_id);
		return 0;
	}
	if (lua_type(L, 2) != LUA_TSTRING) {
		luaL_error(L, "[wordfilter.check]: string expect, got type:[%s]",
						lua_typename(L, lua_type(L, 2)));
		return 0;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.check]: filter no created,filter id:[%d]",
						filter_id);
		return 0;
	}

	const char* word = lua_tostring(L, 2);
	strnodeptr strlist = NULL;
	int find = search_word_ex(ctx, word, &strlist);
	UNLOCK(&g_ctx_lock);

	lua_pushboolean(L, find);
	lua_newtable(L);
	strnodeptr p = strlist;
	int i = 1;
	while (p) {
		lua_pushstring(L, p->str);
		lua_rawseti(L, -2, i++);
		p = p->next;
	}
	if (strlist) free_str_list(strlist);
	return 2;
}

int lmemory(lua_State *L) {
	lua_pushinteger(L, g_memsize);
	return 1;
}

int
luaopen_wordfilter(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{"newctx",         lnewctx},
		{"cleanctx",       lcleanctx},
		{"freectx",        lfreectx},
		{"setignorecase",  lsetignorecase},
		{"setmaskword",    lsetmaskword},
		{"updateskipword", lupdateskipword},
		{"updateword",     lupdateword},
	  	{"filter", 	       lfilter},
	  	{"check",          lcheck},
	  	{"memory",         lmemory},
	  	{NULL, NULL}
	};
	lua_createtable(L, 0, (sizeof(l)) / sizeof(luaL_Reg) - 1);
	luaL_setfuncs(L, l, 0);
	return 1;
}