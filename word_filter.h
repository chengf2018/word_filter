#ifndef __WORD_FILTER_H
#define __WORD_FILTER_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef unsigned char byte;

typedef struct _trie {
	uint32_t data;
}*trieptr;

struct _trie_pool_free_node {
	struct _trie_pool_free_node *next;
	uint32_t index;
};

struct _trie_node_index {
	uint32_t pool_index;
	uint32_t index;
	byte children_index;
};

struct _trie_pool {
	struct _trie_pool_free_node* freelist;
	trieptr pool;
	uint32_t pool_size;
	uint32_t pool_tail;
};

typedef struct _str_node {
	char* str;
	struct _str_node* next;
}*strnodeptr;

typedef struct _wordfilter_ctx {
	struct _trie word_root;
	struct _trie skip_word_root;
	int ignorecase;
	char mask_word;
	struct _trie_pool pool[8];
}*wordfilterctxptr;

size_t wf_get_memsize();
void* wf_malloc(size_t size);
void wf_free(void* p, size_t size);
void* wf_realloc(void* p, size_t newsize, size_t oldsize);
void wf_free_str_list(strnodeptr strlist);

wordfilterctxptr wf_create_ctx();
void wf_clean_ctx(wordfilterctxptr ctx);
void wf_free_ctx(wordfilterctxptr ctx);

int wf_word_isempty(wordfilterctxptr ctx);
int wf_skipword_isempty(wordfilterctxptr ctx);
int wf_insert_word(wordfilterctxptr ctx, const char* word);
int wf_insert_skip_word(wordfilterctxptr ctx, const char* word);
int wf_search_word(wordfilterctxptr ctx, const char* word, 
	char* word_key);
int wf_search_word_ex(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist);
int wf_filter_word(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist, char *outstr);
void wf_set_ignore_case(wordfilterctxptr ctx, int is_ignore);
void wf_set_mask_word(wordfilterctxptr ctx, char mask_word);

#endif //__WORD_FILTER_H