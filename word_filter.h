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

size_t word_filter_get_memsize();
void* word_filter_malloc(size_t size);
void word_filter_free(void* p, size_t size);
void* word_filter_realloc(void* p, size_t newsize, size_t oldsize);
void word_filter_free_str_list(strnodeptr strlist);

wordfilterctxptr word_filter_create_ctx();
void word_filter_clean_ctx(wordfilterctxptr ctx);
void word_filter_free_ctx(wordfilterctxptr ctx);

int word_filter_insert_word(wordfilterctxptr ctx, const char* word);
int word_filter_insert_skip_word(wordfilterctxptr ctx, const char* word);
int word_filter_search_word(wordfilterctxptr ctx, const char* word, 
	char** word_key);
int word_filter_search_word_ex(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist);
char* word_filter_filter_word(wordfilterctxptr ctx, const char* word, 
	strnodeptr* strlist, int* isfilter);
void word_filter_set_ignore_case(wordfilterctxptr ctx, int is_ignore);
void word_filter_set_mask_word(wordfilterctxptr ctx, char mask_word);

#endif //__WORD_FILTER_H