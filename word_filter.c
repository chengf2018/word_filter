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

static inline char tolower(char c) {
	return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static inline char* copy_string(const char* str) {
	size_t str_len = strlen(str);
	char * newstr = word_filter_malloc((str_len + 1) * sizeof(char));
	if (!newstr) return NULL;
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

static inline wordfilterctxptr create_word_filter_context() {
	wordfilterctxptr ctx = (wordfilterctxptr)word_filter_malloc(sizeof(*ctx));
	if (!ctx) return NULL;

	memset(ctx, 0, sizeof(*ctx));
	ctx->ignorecase = 0;
	ctx->word_root = create_trie();
	ctx->skip_word_root = create_trie();
	ctx->mask_word = '*';
	return ctx;
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

static inline strnodeptr insert_str(strnodeptr strnode, const char* str) {
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

static inline const char* search_strnode(strnodeptr head, const char* str) {
	if (!str) return NULL;
	while (head) {
		if (strcmp(head->str, str) == 0)
			return head->str;
		head = head->next;
	}
	return NULL;
}

static inline void free_str_list(strnodeptr strlist) {
	strnodeptr node = strlist;
	while (node) {
		if (node->str)
			word_filter_free(node->str, strlen(node->str) + 1);
		strnodeptr prenode = node;
		node = node->next;
		word_filter_free(prenode, sizeof(*prenode));
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

static byte binary_search(trieptr node, byte c, int *exist) {
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

static inline int skip_word(trieptr word_root, const char** str, int ignorecase) {
	if (!str) return 0;
	char c;
	int find = 0;
	byte pos_index = 0;
	trieptr node = word_root;
	const char* wordptr = *str;

	while (c = *wordptr) {
		if (ignorecase) c = tolower(c);
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
	if (find)
		*str += find;
	return find;
}

static int do_insert_word(wordfilterctxptr ctx, trieptr root, const char* word) {
	if (strlen(word) > MAX_WORD_LENGTH) return 0;

	const char* wordptr = word;
	char c;
	trieptr node = root;
	while (c = *wordptr) {
		if (ctx->ignorecase)c = tolower(c);
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

static int do_search_word(wordfilterctxptr ctx, trieptr word_root, trieptr skip_word_root, const char* word, char** word_key) {
	char c;
	int ignorecase = ctx->ignorecase;
	int find = 0;
	char trie_stack[MAX_WORD_LENGTH + 1] = { 0 };
	byte trie_stack_index = 0;
	trieptr node = word_root;
	const char* wordptr = word;
	int skip_num = skip_word(skip_word_root, &wordptr, ignorecase);

	while (c = *wordptr) {
		if (ignorecase) c = tolower(c);
		int exist = 0;
		byte index = binary_search(node, c, &exist);
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

int insert_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->word_root, word);
}

int insert_skip_word(wordfilterctxptr ctx, const char* word) {
	return do_insert_word(ctx, ctx->skip_word_root, word);
}

void clean_ctx(wordfilterctxptr ctx) {
	if (!ctx) return;
	if (ctx->word_root) {
		free_node(ctx->word_root);
		word_filter_free(ctx->word_root, sizeof(*ctx->word_root));
	}
	if (ctx->skip_word_root) {
		free_node(ctx->skip_word_root);
		word_filter_free(ctx->skip_word_root, sizeof(*ctx->skip_word_root));
	}

	word_filter_free(ctx, sizeof(*ctx));
}

int search_word(wordfilterctxptr ctx, const char* word, char **word_key) {
	return do_search_word(ctx, ctx->word_root, ctx->skip_word_root, word, word_key);
}

int search_word_ex(wordfilterctxptr ctx, const char* word, strnodeptr* strlist) {
	const char* wordptr = word;
	int find = 0;
	strnodeptr strnode = NULL;
	while (*wordptr) {
		char* string = NULL;
		int ret = strlist ? search_word(ctx, wordptr, &string) : search_word(ctx, wordptr, NULL);
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

char* filter_word(wordfilterctxptr ctx, const char* word, strnodeptr *strlist) {
	if (!ctx || !word) return NULL;
	char* wordptr, *wordstartptr;
	char mask_word = ctx->mask_word;
	wordptr = wordstartptr = copy_string(word);
	int find = 0;
	strnodeptr strnode = NULL;
	while (*wordptr) {
		char* string = NULL;
		int ret = strlist ? search_word(ctx, wordptr, &string) : search_word(ctx, wordptr, NULL);
		if (ret) {
			find = 1;
			for (int i = 0; i < ret; i++) {
				*(wordptr + i) = mask_word;
			}
			wordptr += ret;
		}
		else {
			wordptr++;
		}
		if (string) {
			printf("check:%s\n", string);
			if (strlist && !search_strnode(strnode, string))
				strnode = insert_str(strnode, string);
			word_filter_free(string, strlen(string) + 1);
		}
	}

	if (strlist) {
		*strlist = strnode;
	}

	return wordstartptr;
}

//需在插入单词前调用
void set_ignore_case(wordfilterctxptr ctx, int is_ignore) {
	ctx->ignorecase = is_ignore;
}

void set_mask_word(wordfilterctxptr ctx, char mask_word) {
	ctx->mask_word = mask_word;
}

int main(int argc, char **argv) {
	wordfilterctxptr ctx = create_word_filter_context();
	set_ignore_case(ctx, 1);
	insert_word(ctx, "helloworld");
	insert_word(ctx, "hello");
	insert_word(ctx, "hi");
	insert_word(ctx, "h");
	insert_word(ctx, "ll");
	insert_word(ctx, "hel");
	insert_word(ctx, "he");
	insert_word(ctx, "hhh");
	insert_word(ctx, "O");
	insert_word(ctx, "0");
	/*for (int i = 1; i < 256; i++) {
		char s[10] = { 0 };
		s[0] = ((char)i);
		insert_word(ctx, s);
	}*/

	insert_skip_word(ctx, "*");
	insert_skip_word(ctx, " ");

	printf("test1:------------\n");
	strnodeptr strlist1;
	int find = search_word_ex(ctx, "HEL*lo world0_)000????hhhhhhh", &strlist1);
	strnodeptr p = strlist1;
	while (p) {
		printf("%s\n", p->str);
		p = p->next;
	}
	//int find = search_word_ex(ctx, "HEL*lo world0_)000????hhhhhhh");
	printf("test2:------------\n");
	strnodeptr strlist2;
	char *newstr = filter_word(ctx, "HEL*lo world0_)000????hhhhhhh", &strlist2);
	printf("newstr:%s\n", newstr);
	printf("mask words:------------\n");
	p = strlist2;
	while (p) {
		printf("%s\n", p->str);
		p = p->next;
	}


	word_filter_free(newstr, strlen(newstr) + 1);
	free_str_list(strlist1);
	free_str_list(strlist2);
	clean_ctx(ctx);

	printf("memroy alloc size:%d\n", g_memsize);
	return 0;
}