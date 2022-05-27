#include "word_filter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *usecase[] = {
	"helloworld",
	"HelloWorld",
	"Hello World!",
	"hello WORLD",
	"wordfilter",
	"word filter",
	"WORD FILTER",
	"Word Filter",
	"this is a test",
	"screenword",
	"this is a bad word.",
	"first make it runs, then make it simple",
	"say hello hello hello",
	"hi hi hi",
};

char *badword[] = {
	"bad",
	"word",
	"is",
	"simple",
	"filter",
	"hi",
	"hello",
	"aa",
	"bb",
	"cc"
};

int main(int argc, char **argv) {
	wordfilterctxptr ctx = word_filter_create_ctx();
	word_filter_set_ignore_case(ctx, 1);
	for (int i = 0; i < sizeof(badword)/sizeof(*badword); i++) {
		word_filter_insert_word(ctx, badword[i]);
	}

	word_filter_insert_skip_word(ctx, "*");
	word_filter_insert_skip_word(ctx, " ");

	printf("------------test \"search_word_ex\":\n");
	for (int i = 0; i < sizeof(usecase)/sizeof(*usecase); i++) {
		printf("usecase[%d]:%s\n", i, usecase[i]);
		strnodeptr strlist;
		word_filter_search_word_ex(ctx, usecase[i], &strlist);
		strnodeptr p = strlist;
		while (p) {
			printf("bad word:%s\n", p->str);
			p = p->next;
		}
		word_filter_free_str_list(strlist);
	}

	printf("------------test \"search_word_ex\":\n");
	for (int i = 0; i < sizeof(usecase)/sizeof(*usecase); i++) {
		printf("usecase[%d]:%s\n", i, usecase[i]);
		strnodeptr strlist;
		int isFilter = 0;
		char *newstr = word_filter_filter_word(ctx, usecase[i], &strlist, &isFilter);
		printf("newstr:%s\n", newstr);
		strnodeptr p = strlist;
		while (p) {
			printf("bad word:%s\n", p->str);
			p = p->next;
		}
		word_filter_free_str_list(strlist);
		word_filter_free(newstr, strlen(newstr) + 1);
	}

	word_filter_clean_ctx(ctx);
	word_filter_free_ctx(ctx);

	printf("memroy alloc size:%lu\n", word_filter_get_memsize());
	return 0;
}