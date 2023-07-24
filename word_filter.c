/*The MIT License (MIT)
 * Copyright (c) 2022 chengf2018
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the “Software”), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * The Software is provided “as is”, without warranty of any kind, express or 
 * implied, including but not limited to the warranties of merchantability, 
 * fitness for a particular purpose and noninfringement. In no event shall the 
 * authors or copyright holders be liable for any claim, damages or other liability, 
 * whether in an action of contract, tort or otherwise, arising from, out of or in 
 * connection with the software or the use or other dealings in the Software.
 */
//author:chengf2018
//update date:2023-07-25
//License:MIT

#include "word_filter.h"
#define _CRT_SECURE_NO_WARNINGS

#define MAX_TRIE_SIZE 0xFF
#define MAX_WORD_LENGTH 0xFF //word length limit
#define MAX_FILTER_NUM 10
#define MAX_INDEX 0xFFFFF

#define twoto(x) (1<<(x))
static uint32_t
ceil_log2(uint32_t x) {
	int i = 0;
	while (x) {x=x>>1; ++i;}
	return i;
}

#define trie_get_data(n)               ( (n)->data & 0xFF )
#define trie_get_isword(n)             ( ((n)->data & 0x100) >> 8 )
#define trie_get_capacity_pool(n)      ( (((n)->data & 0xE00) >> 9))
#define trie_get_capacity(n)           ( twoto(trie_get_capacity_pool(n)+1) - 1 )
#define trie_get_children_index(n)     ( ((n)->data & 0xFFFFF000) >> 12 )
#define trie_get_children(pool, node)  pool_get_trie((pool), trie_get_capacity_pool(node), trie_get_children_index(node))

#define trie_set_data(n, v)            ( (n)->data = ((n)->data & 0xFFFFFF00) | ((v) & 0xFF) )
#define trie_set_isword(n, v)          ( (n)->data = ((n)->data & 0xFFFFFEFF) | (((v) & 0x1) << 8) )
#define trie_set_rawcapacity(n, v)     ( (n)->data = ((n)->data & 0xFFFFF1FF) | (((v) & 0x7) << 9) )
#define trie_set_capacity(n, v)        trie_set_rawcapacity( n, ceil_log2((v))-1 ) /*v:1~255*/
#define trie_set_children_index(n, v)  ( (n)->data = ((n)->data & 0x00000FFF) | (((v) & 0xFFFFF) << 12) )

#define get_pool_unit_size(pool_index) ( sizeof(struct _trie)*(twoto(pool_index+1)-1) )/*pool_index:0~7*/


static size_t g_memsize = 0;

inline size_t wf_get_memsize() {
	return g_memsize;
}

void*
wf_malloc(size_t size) {
	g_memsize += size;
	return malloc(size);
}

void
wf_free(void* p, size_t size) {
	g_memsize -= size;
	free(p);
}

void*
wf_realloc(void* p, size_t newsize, size_t oldsize) {
	g_memsize += newsize;
	g_memsize -= oldsize;
	return realloc(p, newsize);
}

void
wf_free_str_list(strnodeptr strlist) {
	strnodeptr node = strlist;
	while (node) {
		if (node->str)
			wf_free(node->str, strlen(node->str) + 1);
		strnodeptr prenode = node;
		node = node->next;
		wf_free(prenode, sizeof(*prenode));
	}
}

static inline char
wf_tolower(char c) {
	return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static inline char*
copy_string(const char* str) {
	size_t str_len = strlen(str);
	char* newstr = wf_malloc((str_len + 1) * sizeof(char));
	if (!newstr) return NULL;
	strcpy(newstr, str);
	newstr[str_len] = '\0';
	return newstr;
}

static void
pool_init(struct _trie_pool pool[8]) {
	static uint32_t pool_init_size[8] = {1,1,1,1,1,1,0,0};
	int i;
	for (i=0; i<8; i++) {
		pool[i].freelist = NULL;
		pool[i].pool = NULL;
		pool[i].pool_size = pool_init_size[i];
		if (pool[i].pool_size > 0) {
			size_t size = pool->pool_size * get_pool_unit_size(i);
			pool[i].pool = (trieptr)wf_malloc(size);
			memset(pool[i].pool, 0, size);
		}
	}
}

static void
pool_deinit(struct _trie_pool pool[8]) {
	int i;
	for (i=0; i<8; i++) {
		struct _trie_pool_free_node* freenode = pool[i].freelist;
		while (freenode) {
			struct _trie_pool_free_node* next = freenode->next;
			wf_free(freenode, sizeof(*freenode));
			freenode = next;
		}
		if (pool[i].pool)
			wf_free(pool[i].pool, pool[i].pool_size * get_pool_unit_size(i));
	}
}

//return user index(>0)
static uint32_t
pool_alloc(struct _trie_pool pool[8], uint32_t pool_index) {
	static uint32_t pool_enlarge_size[8] = {8,4,2,1,1,1,1,1};
	struct _trie_pool* mypool = &pool[pool_index];

	assert(mypool->pool_tail <= MAX_INDEX);

	//先检测freelist是否有空闲空间，否则才用pool尾部的空闲空间，如果pool尾部也没有空间了，才扩充pool_size
	if (mypool->freelist) {
		uint32_t free_index = mypool->freelist->index;
		struct _trie_pool_free_node* next = mypool->freelist->next;
		wf_free(mypool->freelist, sizeof(*mypool->freelist));
		mypool->freelist = next;
		return free_index+1;
	}

	if (mypool->pool_tail >= mypool->pool_size) {
		uint32_t oldsize = mypool->pool_size;
		uint32_t unitsize = get_pool_unit_size(pool_index);
		mypool->pool_size = oldsize + pool_enlarge_size[pool_index];
		mypool->pool = (trieptr)wf_realloc(mypool->pool, mypool->pool_size * unitsize, oldsize * unitsize);
		
		memset(mypool->pool + oldsize * (twoto(pool_index+1)-1), 0, (mypool->pool_size - oldsize) * unitsize);
	}

	return ++mypool->pool_tail;
}

static void
pool_free(struct _trie_pool pool[8], uint32_t pool_index, uint32_t free_index) {
	struct _trie_pool* mypool = &pool[pool_index];
	if (free_index == 0) return;
	free_index--;
	if (free_index > 0 && free_index == mypool->pool_tail-1) {
		mypool->pool_tail--;
		return;
	}
	struct _trie_pool_free_node* freenode = (struct _trie_pool_free_node*)wf_malloc(sizeof(*freenode));
	freenode->index = free_index;
	freenode->next = mypool->freelist;
	mypool->freelist = freenode;
}

static inline trieptr
pool_get_trie(struct _trie_pool pool[8], uint32_t pool_index, uint32_t index) {
	struct _trie_pool* mypool = &pool[pool_index];
	if (mypool->pool == NULL || index == 0) return NULL;
	index--;
	return &mypool->pool[index * (twoto(pool_index+1)-1)];
}

static inline strnodeptr
insert_str(strnodeptr strnode, const char* str) {
	if (strnode) {
		strnodeptr newstrnode = (strnodeptr)wf_malloc(sizeof(*strnode));
		if (!newstrnode) return NULL;
		memset(newstrnode, 0, sizeof(*newstrnode));
		newstrnode->str = copy_string(str);
		newstrnode->next = strnode;
		return newstrnode;
	}
	strnode = (strnodeptr)wf_malloc(sizeof(*strnode));
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

static inline byte
calcinitsize(byte addsize) {
	for (int i = 1; i <= 8; i++) {
		int size = (1 << i) - 1;
		if (size >= addsize) return (byte)size;
	}
	return (byte)addsize;
}

static int
reserve(wordfilterctxptr ctx, trieptr* node, struct _trie_node_index node_index) {
	byte capacity = trie_get_capacity(*node);
	trieptr children = trie_get_children(ctx->pool, *node);

	if (capacity == MAX_TRIE_SIZE){
		return 1;
	}

//重新分配后，node指向的地址可能失效，需要进行重新修正

	if (!children) {
		byte newcapacity = 1;
		uint32_t index = pool_alloc(ctx->pool, 0);
		if (!index) return 0;
		if (node_index.pool_index == 0 && node_index.index)
			*node = pool_get_trie(ctx->pool, node_index.pool_index, node_index.index) + node_index.children_index;

		trie_set_capacity(*node, newcapacity);
		trie_set_children_index(*node, index);

		children = trie_get_children(ctx->pool, *node);
		children[0].data = 0;
		return 1;
	}
	
	byte data = trie_get_data(&children[capacity-1]);
	if (data != 0) { //如果最后一个数据不为0，说明已经用完
		byte oldcapacity = capacity;
		byte newcapacity = (capacity << 1) + 1;
		struct _trie_node_index oldclildren_index = { trie_get_capacity_pool(*node), trie_get_children_index(*node), 0 };
		uint32_t pool_index = ceil_log2(newcapacity)-1;
		uint32_t index = pool_alloc(ctx->pool, pool_index);
		if (!index) return 0;
		if (node_index.pool_index == pool_index && node_index.index)
			*node = pool_get_trie(ctx->pool, node_index.pool_index, node_index.index) + node_index.children_index;

		trie_set_capacity(*node, newcapacity);
		trie_set_children_index(*node, index);

		trieptr newchildren = trie_get_children(ctx->pool, *node);
		int i;
		for (i=0; i<oldcapacity; i++) {
			newchildren[i] = children[i];
		}
		for (i=oldcapacity; i<newcapacity; i++) {
			newchildren[i].data = 0;
		}

		pool_free(ctx->pool, oldclildren_index.pool_index, oldclildren_index.index);
	}
	return 1;
}

static trieptr
add_trie(wordfilterctxptr ctx, trieptr* node, byte index, byte c, byte isword, struct _trie_node_index node_index) {
	if (!reserve(ctx, node, node_index)) return NULL;

	trieptr children = trie_get_children(ctx->pool, *node);
	byte capacity = trie_get_capacity(*node);
	assert(index <= capacity);

	byte i = index;
	struct _trie last = children[i];
	while (trie_get_data(&last) != 0 && i+1 < capacity) {
		struct _trie temp = children[i+1];
		children[i+1]=last;
		last = temp; i++;
	}

	trieptr newnode = &children[index];
	memset(newnode, 0, sizeof(*newnode));
	trie_set_data(newnode, c);
	trie_set_isword(newnode, isword);
	//trie_set_children_index(newnode, 0);
	return newnode;
}

static byte
binary_search(wordfilterctxptr ctx, trieptr node, byte c, int* exist) {
	trieptr children = trie_get_children(ctx->pool, node);
	if (children == NULL) {
		if (exist) *exist = 0;
		return 0;
	}
	byte capacity = trie_get_capacity(node);
	int l = 0, r = capacity - 1;

	while (l <= r) {
		byte middle = (l + r) >> 1;
		byte data = trie_get_data(&children[middle]);
		if (data == 0 || data > c) {
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
skip_word(wordfilterctxptr ctx, trieptr word_root, const char** str, int ignorecase) {
	if (!str) return 0;
	char c;
	const char* wordptr = *str;
	trieptr node = word_root;
	int pos_index = 0;
	int find_pos = 0;
	while ((c = *wordptr)) {
		if (ignorecase) c = wf_tolower(c);
		int exist = 0;
		byte index = binary_search(ctx, node, c, &exist);
		if (!exist) break;

		pos_index++;
		trieptr children = trie_get_children(ctx->pool, node);
		node = &children[index];

		if (trie_get_isword(node)) {
			find_pos = pos_index;
		}
		wordptr++;
	}

	if (find_pos)
		*str += find_pos;

	return find_pos;
}

static int
do_insert_word(wordfilterctxptr ctx, trieptr root, const char* word) {
	if (strlen(word) > MAX_WORD_LENGTH) return 0;

	const char* wordptr = word;
	char c;
	trieptr node = root;
	struct _trie_node_index node_index = {0,0,0};
	while ((c = *wordptr)) {
		if (ctx->ignorecase) c = wf_tolower(c);
		int exist = 0;
		byte index = binary_search(ctx, node, c, &exist);
		byte isword = *(wordptr + 1) == '\0';
		if (exist) {
			trieptr children = trie_get_children(ctx->pool, node);
			node_index = (struct _trie_node_index){trie_get_capacity_pool(node), trie_get_children_index(node), index};
			node = &children[index];
			if (isword) {
				trie_set_isword(node, 1);
			}
		} else {
			trieptr newnode = add_trie(ctx, &node, index, c, isword, node_index);
			if (!newnode) return 0;
			node_index = (struct _trie_node_index){trie_get_capacity_pool(node), trie_get_children_index(node), index};
			node = newnode;
		}
		wordptr++;
	}
	return 1;
}

static int
do_search_word(wordfilterctxptr ctx, trieptr word_root, trieptr skip_word_root, const char* word, char* word_key) {
	char c;
	int ignorecase = ctx->ignorecase;
	int find = 0;
	int word_key_index = 0;
	trieptr node = word_root;
	const char* wordptr = word;
	int skip_num = 0;
	int exist = 0;
	byte index = 0;

	while ((c = *wordptr)) {
		if (ignorecase) c = wf_tolower(c);
		exist = 0;
		index = binary_search(ctx, node, c, &exist);
		if (!exist) {
			//word not existed, try skip word
			if (word_key_index > 0) {
				int skip = skip_word(ctx, skip_word_root, &wordptr, ignorecase);
				if (skip) {
					skip_num += skip;
					continue;
				}
			}
			
			break;
		}
		if (word_key) {
			word_key[word_key_index] = *wordptr;
		}
		word_key_index++;
		trieptr children = trie_get_children(ctx->pool, node);
		node = &children[index];

		if (trie_get_isword(node)) {
			find = word_key_index;
		}
		wordptr++;
	}
	return find ? (find + skip_num) : 0;
}

int
wf_insert_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, &ctx->word_root, word);
}

int
wf_insert_skip_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, &ctx->skip_word_root, word);
}

wordfilterctxptr
wf_create_ctx() {
	wordfilterctxptr ctx = (wordfilterctxptr)wf_malloc(sizeof(*ctx));
	if (!ctx) return NULL;

	memset(ctx, 0, sizeof(*ctx));
	pool_init(ctx->pool);

	ctx->mask_word = '*';
	return ctx;
}

void
wf_clean_ctx(wordfilterctxptr ctx) {
	if (!ctx) return;
	pool_deinit(ctx->pool);

	memset(ctx, 0, sizeof(*ctx));
	pool_init(ctx->pool);
	ctx->mask_word = '*';
}

void wf_free_ctx(wordfilterctxptr ctx) {
	if (!ctx) return;

	pool_deinit(ctx->pool);
	wf_free(ctx, sizeof(*ctx));
}

int
wf_search_word(wordfilterctxptr ctx, const char* word, char* word_key) {
	return do_search_word(ctx, &ctx->word_root, &ctx->skip_word_root, word, word_key);
}

int
wf_search_word_ex(wordfilterctxptr ctx, const char* word, strnodeptr* strlist) {
	const char* wordptr = word;
	int find = 0;
	strnodeptr strnode = NULL;
	while (*wordptr) {
		char string[MAX_WORD_LENGTH + 1] = {0};
		int ret = wf_search_word(ctx, wordptr, string);
		if (ret) {
			find = 1; 
			wordptr += ret;
			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
		}
		else {
			wordptr++;
		}
	}
	if (strlist) {
		*strlist = strnode;
	}
	return find;
}

int
wf_filter_word(wordfilterctxptr ctx, const char* word, strnodeptr* strlist, char *outstr) {
	if (!ctx || !word || !outstr) return 0;
	const char* wordptr = word;
	char mask_word = ctx->mask_word;
	int find = 0, strpos = 0;

	strnodeptr strnode = NULL;
	while (*wordptr) {
		char string[MAX_WORD_LENGTH + 1] = {0};
		int ret = wf_search_word(ctx, wordptr, string);
		if (ret) {
			find = 1;
			int stringindex = 0, i = 0;
			while (i < ret) {
				char c = *(wordptr + i);
				if (string[stringindex] != c) {
					outstr[strpos++] = c;
					i++;
					continue;
				}
				int j = 0;
				while (c & (0x80>>j) && j < 4) j++; //utf8 code
				j = j ? j : 1;
				i += j;
				stringindex += j;
				outstr[strpos++] = mask_word;
			}
			wordptr += ret;

			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
		}
		else {
			outstr[strpos++] = *wordptr;
			wordptr++;
		}
	}

	if (strlist) {
		*strlist = strnode;
	}
	outstr[strpos] = '\0';
	return find;
}

//the function must used at before insert word
void
wf_set_ignore_case(wordfilterctxptr ctx, int is_ignore) {
	ctx->ignorecase = is_ignore;
}

void
wf_set_mask_word(wordfilterctxptr ctx, char mask_word) {
	ctx->mask_word = mask_word;
}
