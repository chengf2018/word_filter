#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_TRIE_SIZE 0xFF
#define MAX_WORD_LENGTH 0xFF //屏蔽词的大小限定

typedef unsigned char byte;

typedef struct _trie {
	byte data;
	byte nchildren;
	byte capacity;
	byte isword;
	struct _trie *children;
}*trieptr;

static size_t g_memsize = 0;

static inline void* word_filter_malloc(size_t size) {
	g_memsize += size;
	return malloc(size);
}
static inline void word_filter_free(void* p, size_t size) {
	g_memsize -= size;
	free(p);
}
static inline void* word_filter_realloc(void* p, size_t newsize, size_t oldsize) {
	g_memsize += newsize;
	g_memsize -= oldsize;
	return realloc(p, newsize);
}

static inline char* copy_string(char* str) {
	size_t str_len = strlen(str);
	char * newstr = word_filter_malloc((str_len + 1) * sizeof(char));
	strcpy(newstr, str);
	newstr[str_len] = '\0';
	return newstr;
}

static inline trieptr create_trie() {
	trieptr newtrie = (trieptr)word_filter_malloc(sizeof(*newtrie));
	memset(newtrie, 0, sizeof(*newtrie));
	newtrie->capacity = 1;
	newtrie->children = (trieptr)word_filter_malloc(sizeof(*newtrie));
	memset(newtrie->children, 0, sizeof(*newtrie));
	return newtrie;
}

static inline void free_node(trieptr node) {
	if (node && node->children) {
		for (int i = 0; i < node->capacity; i++) {
			trieptr childnode = &node->children[i];
			if (childnode->children) 
				free_node(childnode);
		}
		word_filter_free(node->children, node->capacity * sizeof(*node->children));
		node->children = NULL;
	}
}

static inline byte calcinitsize(byte addsize) {
	for (int i = 1; i <= 8; i++) {
		int size = (1 << i) - 1;
		if (size >= addsize) return (byte)size;
	}
	return (byte)addsize;
}

static int reserve(trieptr node, byte addsize) {
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
		trieptr newchildren = (trieptr)word_filter_realloc(node->children, \
			sizeof(*node->children) * newcapacity, \
			sizeof(*node->children) * oldsize);
		if (!newchildren) return 0;
		node->capacity = newcapacity;
		node->children = newchildren;
		memset(&node->children[oldsize],  \
			0, sizeof(*node->children) * (newcapacity - oldsize));
	}
	return 1;
}

static trieptr add_trie(trieptr node, byte index, byte c, byte isword) {
	if (!reserve(node, 1)) return NULL;
	struct _trie newnode = {0};
	newnode.data = c;
	newnode.isword = isword;

	int movesize = node->nchildren - index;
	if (movesize > 0) {
		memmove(&node->children[index+1], &node->children[index],\
		 movesize * sizeof(struct _trie));
	}
	node->children[index] = newnode;
	node->nchildren++;

	return &node->children[index];
}

static byte binary_search(trieptr node, byte c, int *exist) {
	if (node->nchildren == 0) {
		if (exist) *exist = 0;
		return 0;
	}
	int l = 0, r = node->nchildren - 1;

	while (l <= r) {
		byte middle = (l + r) >> 1;
		if (node->children[middle].data > c) {
			r = middle - 1;
		}
		else if (node->children[middle].data == c) {
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

int insert_word(trieptr root, const char* word) {
	if (strlen(word) > MAX_WORD_LENGTH) return 0;

	const char* wordptr = word;
	char c;
	trieptr node = root;
	while (c = *wordptr) {
		int exist = 0;
		byte index = binary_search(node, c, &exist);
		byte isword = *(wordptr+1) == '\0';
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

int clean_all(trieptr root) {
	free_node(root);
	word_filter_free(root, sizeof(*root));
}

int search_word(trieptr root, const char* word, char **word_key) {
	const char* wordptr = word;
	char c;
	char trie_stack[MAX_WORD_LENGTH + 1] = { 0 };
	byte trie_stack_index = 0;
	trieptr node = root;
	int find = 0;
	char* str = NULL;

	while (c = *wordptr)
	{
		int exist = 0;
		byte index = binary_search(node, c, &exist);
		if (!exist) break;
		trie_stack[trie_stack_index++] = c;
		node = &node->children[index];

		if (node->isword) {
			find = trie_stack_index;
		}
		wordptr++;
	}

	if (find) {
		trie_stack[find] = '\0';
		if (word_key) *word_key = copy_string(trie_stack);
	}

	return find;
}

int search_word_ex(trieptr root, const char* word) {
	char* wordptr = word;
	int find = 0;
	while (*wordptr) {
		char* string = NULL;
		int ret = search_word(root, wordptr, &string);
		if (ret && string) {
			find = 1; 
			printf("check:%s\n", string);
			word_filter_free(string, strlen(string) + 1);
			wordptr += ret;
		}
		else {
			wordptr++;
		}
	}
	return find;
}

int main(int argc, char **argv) {
	
	trieptr root = create_trie();
	insert_word(root, "hello world");
	insert_word(root, "hello");
	insert_word(root, "hi");
	insert_word(root, "h");
	insert_word(root, "ll");
	insert_word(root, "hel");
	insert_word(root, "he");
	insert_word(root, "hhh");
	insert_word(root, "o");

	char* string[64] = { 0 };
	int find = search_word_ex(root, "hello wohhhhhhh", string, 64);

	clean_all(root);

	printf("memroy alloc size:%d\n", g_memsize);
	return 0;
}