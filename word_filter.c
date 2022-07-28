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
//update date:2022-07-27
//License:MIT

#include "word_filter.h"
#define _CRT_SECURE_NO_WARNINGS

#define MAX_TRIE_SIZE 0xFF
#define MAX_WORD_LENGTH 0xFF //word length limit
#define MAX_FILTER_NUM 10

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

static inline trieptr
create_trie() {
	trieptr newtrie = (trieptr)wf_malloc(sizeof(*newtrie));
	memset(newtrie, 0, sizeof(*newtrie));
	newtrie->capacity = 1;
	newtrie->children = (trieptr)wf_malloc(sizeof(*newtrie));
	memset(newtrie->children, 0, sizeof(*newtrie));
	return newtrie;
}

static inline void
free_node(trieptr node) {
	if (node && node->children) {
		for (int i = 0; i < node->capacity; i++) {
			trieptr childnode = &node->children[i];
			if (childnode->children)
				free_node(childnode);
		}
		wf_free(node->children, node->capacity * sizeof(*node->children));
		node->children = NULL;
		node->capacity = 0;
		node->nchildren = 0;
	}
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
reserve(trieptr node, byte addsize) {
	if (node->capacity == MAX_TRIE_SIZE) return 1;
	addsize = addsize ? addsize : 1;
	if (!node->children) {
		byte newcapacity = calcinitsize(addsize);
		trieptr newchildren = (trieptr)wf_malloc(sizeof(*node->children) * newcapacity);
		if (!newchildren) return 0;
		node->capacity = newcapacity;
		node->children = newchildren;
		memset(node->children, 0, sizeof(*node->children) * node->capacity);
		return 1;
	}
	if (node->nchildren + 1 > node->capacity) {
		byte oldsize = node->capacity;
		byte newcapacity = (node->capacity << 1) + 1;
		trieptr newchildren = (trieptr)wf_realloc(node->children,
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
		memmove(&node->children[index + 1], &node->children[index], \
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
binary_search(trieptr node, byte c, int* exist) {
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
			if (ignorecase) c = wf_tolower(c);
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
		if (ctx->ignorecase) c = wf_tolower(c);
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
	int last_skip = 0;
	byte index = 0;

	while ((c = *wordptr)) {
		if (ignorecase) c = wf_tolower(c);
		exist = 0;
		index = binary_search(node, c, &exist);
		if (!exist) break;
		skip_num += last_skip;
		trie_stack[trie_stack_index++] = c;
		node = &node->children[index];

		if (node->isword) {
			find = trie_stack_index;
		}
		wordptr++;
		last_skip = skip_word(skip_word_root, &wordptr, ignorecase);
	}

	if (find && word_key) {
		trie_stack[find] = '\0';
		*word_key = copy_string(trie_stack);
	}

	return find ? (find + skip_num) : 0;
}

int
wf_insert_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->word_root, word);
}

int
wf_insert_skip_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->skip_word_root, word);
}

wordfilterctxptr
wf_create_ctx() {
	wordfilterctxptr ctx = (wordfilterctxptr)wf_malloc(sizeof(*ctx));
	if (!ctx) return NULL;

	memset(ctx, 0, sizeof(*ctx));
	ctx->ignorecase = 0;
	ctx->word_root = create_trie();
	ctx->skip_word_root = create_trie();
	ctx->mask_word = '*';
	return ctx;
}

void
wf_clean_ctx(wordfilterctxptr ctx) {
	if (!ctx) return;
	if (ctx->word_root) {
		free_node(ctx->word_root);
	}
	if (ctx->skip_word_root) {
		free_node(ctx->skip_word_root);
	}
}

void wf_free_ctx(wordfilterctxptr ctx) {
	wf_clean_ctx(ctx);
	if (ctx->word_root) {
		wf_free(ctx->word_root, sizeof(*ctx->word_root));
	}
	if (ctx->skip_word_root) {
		wf_free(ctx->skip_word_root, sizeof(*ctx->skip_word_root));
	}
	wf_free(ctx, sizeof(*ctx));
}

int
wf_search_word(wordfilterctxptr ctx, const char* word, char** word_key) {
	return do_search_word(ctx, ctx->word_root, ctx->skip_word_root, word, word_key);
}

int
wf_search_word_ex(wordfilterctxptr ctx, const char* word, strnodeptr* strlist) {
	const char* wordptr = word;
	int find = 0;
	strnodeptr strnode = NULL;
	while (*wordptr) {
		char* string = NULL;
		int ret = strlist ? wf_search_word(ctx, wordptr, &string)
			: wf_search_word(ctx, wordptr, NULL);
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
			wf_free(string, strlen(string) + 1);
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
		char* string = NULL;
		int ret = strlist ? wf_search_word(ctx, wordptr, &string)
			: wf_search_word(ctx, wordptr, NULL);
		if (ret) {
			find = 1;
			for (int i = 0; i < ret; i++) {
				char c = *(wordptr + i);
				if (c & 0x80) {
					//utf8 code
					int skip = 1;
					for (int j = 0; j < 3; j++) {
						if (c & (0x40 >> j)) {
							skip++;
						}
						else break;
					}
					i += skip;
				}
				outstr[strpos++] = mask_word;
			}
			wordptr += ret;
		}
		else {
			outstr[strpos++] = *wordptr;
			wordptr++;
		}
		if (string) {
			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
			wf_free(string, strlen(string) + 1);
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