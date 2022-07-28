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
	wordfilterctxptr ctx = wf_create_ctx();
	wf_set_ignore_case(ctx, 1);
	for (int i = 0; i < sizeof(badword)/sizeof(*badword); i++) {
		wf_insert_word(ctx, badword[i]);
	}

	wf_insert_skip_word(ctx, "*");
	wf_insert_skip_word(ctx, " ");

	printf("------------test \"wf_search_word_ex\":\n");
	for (int i = 0; i < sizeof(usecase)/sizeof(*usecase); i++) {
		printf("usecase[%d]:%s\n", i, usecase[i]);
		strnodeptr strlist;
		wf_search_word_ex(ctx, usecase[i], &strlist);
		strnodeptr p = strlist;
		while (p) { 
			printf("bad word:%s\n", p->str);
			p = p->next;
		}
		wf_free_str_list(strlist);
	}

	printf("------------test \"wf_filter_word\":\n");
	for (int i = 0; i < sizeof(usecase)/sizeof(*usecase); i++) {
		printf("usecase[%d]:%s\n", i, usecase[i]);
		strnodeptr strlist;
		
		size_t slen = strlen(usecase[i]) + 1;
		char newstr[slen];
		memset(newstr, 0, slen);

		wf_filter_word(ctx, usecase[i], &strlist, newstr);
		printf("newstr:%s\n", newstr);
		strnodeptr p = strlist;
		while (p) {
			printf("bad word:%s\n", p->str);
			p = p->next;
		}
		wf_free_str_list(strlist);
		//wf_free(newstr, strlen(newstr) + 1);
	}

	wf_clean_ctx(ctx);
	wf_free_ctx(ctx);

	printf("memroy alloc size:%lu\n", wf_get_memsize());
	return 0;
}