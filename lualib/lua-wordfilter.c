#define _CRT_SECURE_NO_WARNINGS
#include <lua.h>
#include <lauxlib.h>
#include <pthread.h>
#include "word_filter.h"

#define MAX_FILTER_NUM 10

#define LOCK(pl) while (__sync_lock_test_and_set(pl,1)) {};
#define UNLOCK(pl) __sync_lock_release(pl);


struct rwlock {
	pthread_rwlock_t lock;
};

static inline void
rwlock_init(struct rwlock *lock) {
	pthread_rwlock_init(&lock->lock, NULL);
}

static inline void
rwlock_rlock(struct rwlock *lock) {
	 pthread_rwlock_rdlock(&lock->lock);
}

static inline void
rwlock_wlock(struct rwlock *lock) {
	 pthread_rwlock_wrlock(&lock->lock);
}

static inline void
rwlock_wunlock(struct rwlock *lock) {
	pthread_rwlock_unlock(&lock->lock);
}

static inline void
rwlock_runlock(struct rwlock *lock) {
	pthread_rwlock_unlock(&lock->lock);
}


typedef int lock;
static lock g_ctx_lock;
static wordfilterctxptr g_ctx_instance[MAX_FILTER_NUM] = {NULL};
static struct rwlock g_rwlock[MAX_FILTER_NUM];


int
lnewctx(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	int ignorecase = lua_tointeger(L, 2);

	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.newctx]: filter id overstep the boundary:[%d]",
						filter_id);
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.newctx]: already create filter,filter id:[%d]",
						filter_id);
	}

	ctx = wf_create_ctx();
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.newctx]: alloc context error");
	}
	rwlock_init(&g_rwlock[filter_id-1]);
	wf_set_ignore_case(ctx, ignorecase);
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
		}
		rwlock_wlock(&g_rwlock[filter_id-1]);
		wf_clean_ctx(ctx);
		rwlock_wunlock(&g_rwlock[filter_id-1]);
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
		}
		rwlock_wlock(&g_rwlock[filter_id-1]);
		wf_free_ctx(ctx);
		rwlock_wunlock(&g_rwlock[filter_id-1]);
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
	}
	int ignorecase = lua_tointeger(L, 2);
	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.setignorecase]: filter no created,filter id:[%d]",
						filter_id);
	}
	wf_set_ignore_case(ctx, ignorecase);
	UNLOCK(&g_ctx_lock);
	return 0;
}

int
lsetmaskword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.setmaskword]: filter id overstep the boundary:[%d]",
						filter_id);		
	}
	const char* maskword = lua_tostring(L, 2);
	if (maskword == NULL || strlen(maskword) > 1) {
		luaL_error(L, "[wordfilter.setmaskword]:maskword must be a character:[%d]",
						filter_id);
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.setmaskword]: filter no created,filter id:[%d]",
						filter_id);
	}
	wf_set_mask_word(ctx, maskword[0]);
	UNLOCK(&g_ctx_lock);
	return 0;
}

int
lupdateskipword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.updateskipword]: filter id overstep the boundary:[%d]",
						filter_id);		
	}
	if (!lua_istable(L, 2)) {
		luaL_error(L, "[wordfilter.updateskipword]: table expect, got type[%s]",
						lua_typename(L, lua_type(L, 2)));
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.updateskipword]: filter no created,filter id:[%d]",
						filter_id);
	}
	rwlock_wlock(&g_rwlock[filter_id-1]);
	UNLOCK(&g_ctx_lock);

	int success = 1;
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		const char* word = lua_tostring(L, -1);
		if (!wf_insert_skip_word(ctx, word)) {
			success = 0;
			rwlock_wunlock(&g_rwlock[filter_id-1]);
			luaL_error(L, "[wordfilter.updateskipword]: insert word error[%s]",
							word);
		}
		lua_pop(L, 1);
	}
	rwlock_wunlock(&g_rwlock[filter_id-1]);

	lua_pushboolean(L, success);
	return 0;
}

int
lupdateword(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.updateword]: filter id overstep the boundary:[%d]",
						filter_id);
	}

	if (!lua_istable(L, 2)) {
		luaL_error(L, "[wordfilter.updateword]: table expect, got type[%s]",
						lua_typename(L, lua_type(L, 2)));
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.updateword]: filter no created,filter id:[%d]",
						filter_id);
	}

	rwlock_wlock(&g_rwlock[filter_id-1]);
	UNLOCK(&g_ctx_lock);

	int success = 1;
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		if (lua_type(L, -1) != LUA_TSTRING) {
			rwlock_wunlock(&g_rwlock[filter_id-1]);
			luaL_error(L, "[wordfilter.updateword]: string expect, got type[%s]",
							lua_typename(L, lua_type(L, -1)));
		}
		const char* word = lua_tostring(L, -1);
		if (!wf_insert_word(ctx, word)) {
			success = 0;
			rwlock_wunlock(&g_rwlock[filter_id-1]);
			luaL_error(L, "[wordfilter.updateword]: insert word error[%s]",
							word);
		}
		lua_pop(L, 1);
	}
	rwlock_wunlock(&g_rwlock[filter_id-1]);

	lua_pushboolean(L, success);
	return 1;
}

int
lfilter(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.filter]: filter id overstep the boundary:[%d]",
						filter_id);
	}
	if (lua_type(L, 2) != LUA_TSTRING) {
		luaL_error(L, "[wordfilter.filter]: string expect, got type:[%s]",
						lua_typename(L, lua_type(L, 2)));
	}
	
	size_t str_len;
	const char* word = lua_tolstring(L, 2, &str_len);
	if (word == NULL) {
		lua_pushboolean(L, 0);
		return 1;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.filter]: filter no created,filter id:[%d]",
						filter_id);
	}

	rwlock_rlock(&g_rwlock[filter_id-1]);
	UNLOCK(&g_ctx_lock);

	char wordstartptr[str_len+1];
	strnodeptr strlist = NULL;
	memset(wordstartptr, 0, (str_len+1)*sizeof(char));
	int isfilter = wf_filter_word(ctx, word, &strlist, wordstartptr);

	rwlock_runlock(&g_rwlock[filter_id-1]);

	lua_pushboolean(L, isfilter);
	lua_pushstring(L, wordstartptr);
	lua_newtable(L);
	strnodeptr p = strlist;
	int i = 1;
	while (p) {
		lua_pushstring(L, p->str);
		lua_rawseti(L, -2, i++);
		p = p->next;
	}
	if (strlist) wf_free_str_list(strlist);
	return 3;
}

int
lcheck(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		luaL_error(L, "[wordfilter.check]: filter id overstep the boundary:[%d]",
						filter_id);
	}
	if (lua_type(L, 2) != LUA_TSTRING) {
		luaL_error(L, "[wordfilter.check]: string expect, got type:[%s]",
						lua_typename(L, lua_type(L, 2)));
	}

	const char* word = lua_tostring(L, 2);
	if (word == NULL) {
		lua_pushboolean(L, 0);
		return 1;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		luaL_error(L, "[wordfilter.check]: filter no created,filter id:[%d]",
						filter_id);
	}
	rwlock_rlock(&g_rwlock[filter_id-1]);
	UNLOCK(&g_ctx_lock);

	strnodeptr strlist = NULL;
	int find = wf_search_word_ex(ctx, word, &strlist);
	rwlock_runlock(&g_rwlock[filter_id-1]);

	lua_pushboolean(L, find);
	lua_newtable(L);
	strnodeptr p = strlist;
	int i = 1;
	while (p) {
		lua_pushstring(L, p->str);
		lua_rawseti(L, -2, i++);
		p = p->next;
	}
	if (strlist) wf_free_str_list(strlist);
	return 2;
}

int
lempty(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		lua_pushboolean(L, 0);
		return 1;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		lua_pushboolean(L, 0);
		return 1;
	}

	int empty = wf_word_isempty(ctx);
	UNLOCK(&g_ctx_lock);

	lua_pushboolean(L, empty);
	return 1;
}

int
lmemory(lua_State *L) {
	lua_pushinteger(L, wf_get_memsize());
	return 1;
}

int
lcapacity(lua_State *L) {
	int filter_id = lua_tointeger(L, 1);
	if (filter_id < 1 || filter_id > MAX_FILTER_NUM) {
		lua_pushboolean(L, 0);
		return 1;
	}

	LOCK(&g_ctx_lock);
	wordfilterctxptr ctx = g_ctx_instance[filter_id-1];
	if (!ctx) {
		UNLOCK(&g_ctx_lock);
		lua_pushboolean(L, 0);
		return 1;
	}
	
	UNLOCK(&g_ctx_lock);

	lua_newtable(L);
	int i;
	for (i=1; i<=8; i++) {
		lua_newtable(L);
		lua_pushinteger(L, ctx->pool[i-1].pool_tail);
		lua_rawseti(L, -2, 1);
		lua_pushinteger(L, ctx->pool[i-1].pool_size);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, i);
	}
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
	  	{"empty",          lempty},
	  	{"memory",         lmemory},
		{"capacity",       lcapacity},
	  	{NULL, NULL}
	};
	lua_createtable(L, 0, (sizeof(l)) / sizeof(luaL_Reg) - 1);
	luaL_setfuncs(L, l, 0);
	return 1;
}