#ifndef __WORD_FILTER_H
#define __WORD_FILTER_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef unsigned char byte;

typedef struct _trie {
	byte data;
	byte nchildren;
	byte capacity;
	byte isword;
	struct _trie* children;
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

size_t wf_get_memsize();
void* wf_malloc(size_t size);
void wf_free(void* p, size_t size);
void* wf_realloc(void* p, size_t newsize, size_t oldsize);
void wf_free_str_list(strnodeptr strlist);

wordfilterctxptr wf_create_ctx();
void wf_clean_ctx(wordfilterctxptr ctx);
void wf_free_ctx(wordfilterctxptr ctx);

int wf_insert_word(wordfilterctxptr ctx, const char* word);
int wf_insert_skip_word(wordfilterctxptr ctx, const char* word);
int wf_search_word(wordfilterctxptr ctx, const char* word, 
	char** word_key);
int wf_search_word_ex(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist);
int wf_filter_word(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist, char *outstr);
void wf_set_ignore_case(wordfilterctxptr ctx, int is_ignore);
void wf_set_mask_word(wordfilterctxptr ctx, char mask_word);

#endif //__WORD_FILTER_H