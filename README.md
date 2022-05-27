# word_filter
This is a simple word filter,you can skip some word then filter the bad word.

# Build
Linux:

    make all

# Use Case
    wordfilterctxptr ctx = word_filter_create_ctx();
    word_filter_set_ignore_case(ctx, 1);
    word_filter_insert_word(ctx, "bad word");
	word_filter_insert_word(ctx, "is");

	word_filter_insert_skip_word(ctx, "*");
    strnodeptr strlist;
    word_filter_search_word_ex(ctx, "this is bad*** word", &strlist);
    strnodeptr p = strlist;
    while (p) {
    	printf("bad word:%s\n", p->str);
    	p = p->next;
    }
	word_filter_free_str_list(strlist);

More see test.c
# License
> **MIT License**