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
	newtrie->capacity = 2;
	newtrie->children = (trieptr)malloc(sizeof(*newtrie) * 2);
	memset(newtrie->children, 0, sizeof(*newtrie) * 2);
	return newtrie;
}

static void reserve(trieptr node) {
	if (node->capacity == MAX_TRIE_SIZE) return;
	if (!node->children) {
		node->capacity = 2;
		node->children = (trieptr)malloc(sizeof(struct _trie) * node->nchildren);
		memset(node->children, 0, sizeof(struct _trie) * node->nchildren);
	}
}

static trieptr add_trie(trieptr node, byte index, byte c) {
	
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
		byte index = binary_search(node);
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