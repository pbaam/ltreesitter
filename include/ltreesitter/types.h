#ifndef LTREESITTER_TYPES_H
#define LTREESITTER_TYPES_H

#include "dynamiclib.h"
#include <tree_sitter/api.h>

typedef struct ltreesitter_Parser ltreesitter_Parser;
typedef struct ltreesitter_Tree ltreesitter_Tree;
typedef struct ltreesitter_TreeCursor ltreesitter_TreeCursor;
typedef struct ltreesitter_Node ltreesitter_Node;
typedef struct ltreesitter_Query ltreesitter_Query;
typedef struct ltreesitter_QueryCursor ltreesitter_QueryCursor;

struct ltreesitter_Parser {
	ltreesitter_Dynlib *dl;
	TSParser *parser;
};
#define LTREESITTER_PARSER_METATABLE_NAME "ltreesitter.Parser"

// reference counted source text for trees to hold on to
typedef struct {
	// TODO: rather than refcounting, we should use the lua registry to garbage
	// collect this
	size_t refs;
	size_t length;
	const char *text;
} ltreesitter_SourceText;

struct ltreesitter_Tree {
	TSTree *tree;

	// The *TSTree structure is opaque so we don't have access to how it
	// internally gets the source of the string that it parsed,
	// so we just keep a copy for ourselves here.
	// Not the most memory efficient, but I'd argue having Node:source()
	// is worth it
	ltreesitter_SourceText *source;
};
#define LTREESITTER_TREE_METATABLE_NAME "ltreesitter.Tree"

struct ltreesitter_TreeCursor {
	TSTreeCursor cursor;
};
#define LTREESITTER_TREE_CURSOR_METATABLE_NAME "ltreesitter.TreeCursor"

struct ltreesitter_Node {
	TSNode node;
};
#define LTREESITTER_NODE_METATABLE_NAME "ltreesitter.Node"

struct ltreesitter_Query {
	TSQuery *query;

	const TSLanguage *lang;
	const char *src;
	size_t src_len;
};
#define LTREESITTER_QUERY_METATABLE_NAME "ltreesitter.Query"

struct ltreesitter_QueryCursor {
	ltreesitter_Query *query;
	TSQueryCursor *query_cursor;
};
#define LTREESITTER_QUERY_CURSOR_METATABLE_NAME "ltreesitter.QueryCursor"

#endif
