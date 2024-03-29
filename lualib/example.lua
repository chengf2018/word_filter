local word_filter = require "wordfilter"

local word_filter_id = 1
local ignorecase = 1
word_filter.newctx(word_filter_id, ignorecase)

local mask_word = {
	"I",
	"am",
	"badword",
}

local skip_word = {
	",",
	" "
}

word_filter.updateword(word_filter_id, mask_word)

word_filter.updateskipword(word_filter_id, skip_word)

--check word
local is_find, badword = word_filter.check(word_filter_id, "i am a Bad word! ,a ,m, this is test.")
print("---check word:")
print(is_find)
for k,v in pairs(badword) do
	print(v)
end

--filter word to new string
local is_filter, newstr, filter_word = word_filter.filter(word_filter_id, "i am a Bad word! ,a ,m, this is test.")
print("---filter word:")
print(is_filter, newstr)
for k,v in pairs(filter_word) do
	print(v)
end

word_filter.freectx(word_filter_id)