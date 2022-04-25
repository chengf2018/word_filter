#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_TRIE_SIZE 0xFF
typedef unsigned char byte;

typedef struct _trie {
	byte data;
	byte nchildren;
	byte capacity;
	byte isword;
	struct _trie *children;
}*trieptr;


inline init() {

}

static inline trieptr create_trie() {
	trieptr newtrie = (trieptr)malloc(sizeof(*newtrie));
	memset(newtrie, 0, sizeof(*newtrie));
	newtrie->capacity = 1;
	newtrie->children = (trieptr)malloc(sizeof(*newtrie));
	memset(newtrie->children, 0, sizeof(*newtrie));
	return newtrie;
}
static inline byte calcinitsize(byte addsize) {
	for (int i = 1; i <= 8; i++) {
		int size = (1 << i) - 1;
		if (size >= addsize) return (byte)size;
	}
	return (byte)addsize;
}

static void reserve(trieptr node, byte addsize) {
	if (node->capacity == MAX_TRIE_SIZE) return;
	addsize = addsize ? addsize : 1;
	if (!node->children) {
		node->capacity = calcinitsize(addsize);
		node->children = (trieptr)malloc(sizeof(*node->children) * node->nchildren);
		memset(node->children, 0, sizeof(*node->children) * node->nchildren);
		return;
	}
	if (node->nchildren + 1 > node->capacity) {
		byte oldsize = node->capacity;
		node->capacity = (node->capacity << 1) + 1;
		node->children = (trieptr)realloc(node->children, \
			node->capacity * sizeof(*node->children));
		memset(node->children + (oldsize * sizeof(*node->children)),  \
			0, (node->capacity - oldsize) * sizeof(*node->children));
	}
}

static trieptr add_trie(trieptr node, byte index, byte c) {
	reserve(node, 1);
}

static byte binary_search(trieptr node, byte c) {
	byte l = 0, r = node->nchildren - 1;
	if (r < 0) return 0;

	while (l >= r) {
		byte middle = (l + r) / 2;
		if (node->children[middle].data < c) {
			l = middle + 1;
		}
		else {
			r = middle - 1;
		}
	}
	return l;
}

int insert_word(trieptr root, const char* word) {
	const char* wordptr = word;
	char c;
	trieptr node = root;
	while (c = *wordptr) {
		byte index = binary_search(node, c);
		add_trie(node, index, c);
		wordptr++;
	}
}

int clean_all(trieptr root) {

}

int delete_word(trieptr root, const char* word) {

}

int search_word(trieptr root, const char* word) {

}

int main(int argc, char **argv) {
	printf("%d", sizeof(struct _trie));
	return 0;
}