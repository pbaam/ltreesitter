
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <tree_sitter/api.h>

#include <stdlib.h>
#include <string.h>

#include "luautils.h"
#include "object.h"
#include <ltreesitter/node.h>
#include <ltreesitter/types.h>

#ifdef LOG_GC
#include <stdio.h>
#endif

static void err_if(lua_State *L, const char *msg) {
	if (msg) {
		luaL_error(L, "%s", msg);
	}
}

ltreesitter_Tree *ltreesitter_check_tree(lua_State *L, int idx, const char *msg) {
	if (lua_type(L, idx) != LUA_TUSERDATA) {
		err_if(L, msg);
		return NULL;
	}
	if (lua_getmetatable(L, idx) == 0) {
		err_if(L, msg);
		return NULL;
	}
	lua_getfield(L, -1, "__name");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		err_if(L, msg);
		return NULL;
	}
	const char *name = lua_tostring(L, -1);
	const int cmp = strcmp(name, LTREESITTER_TREE_METATABLE_NAME);
	lua_pop(L, 2);
	if (cmp != 0) {
		err_if(L, msg);
		return NULL;
	}
	return lua_touserdata(L, idx);
}

ltreesitter_Tree *ltreesitter_check_tree_arg(lua_State *L, int idx) {
	return luaL_checkudata(L, idx, LTREESITTER_TREE_METATABLE_NAME);
}

void ltreesitter_push_tree(
    lua_State *L,
    TSTree *t,
    bool own_str,
    const char *src,
    size_t src_len) {
	ltreesitter_Tree *tree = lua_newuserdata(L, sizeof(struct ltreesitter_Tree));
	tree->tree = t;
	tree->own_str = own_str;
	tree->src = src;
	tree->src_len = src_len;
	setmetatable(L, LTREESITTER_TREE_METATABLE_NAME);
}

/* @teal-export Tree.root: function(Tree): Node [[
   Returns the root node of the given parse tree
]] */
static int tree_push_root(lua_State *L) {
	ltreesitter_Tree *const t = ltreesitter_check_tree_arg(L, 1);
	lua_newuserdata(L, sizeof(ltreesitter_Node));
	ltreesitter_push_node(L, 1, ts_tree_root_node(t->tree));
	return 1;
}

static int tree_to_string(lua_State *L) {
	TSTree *t = ltreesitter_check_tree_arg(L, 1)->tree;
	const TSNode root = ts_tree_root_node(t);
	char *s = ts_node_string(root);
	lua_pushlstring(L, (const char *)s, strlen(s));
	free(s);
	return 1;
}

/* @teal-export Tree.copy: function(Tree): Tree [[
   Creates a copy of the tree. Tree-sitter recommends to create copies if you are going to use multithreading since tree accesses are not thread-safe, but copying them is cheap and quick
]] */
static int tree_copy(lua_State *L) {
	ltreesitter_Tree *t = ltreesitter_check_tree_arg(L, 1);

	const char *src_copy;
	if (t->own_str) {
		src_copy = malloc(sizeof(char) * t->src_len);
		if (!src_copy)
			return ALLOC_FAIL(L);
		memcpy((char *)src_copy, t->src, t->src_len);
	} else {
		src_copy = t->src;
	}

	ltreesitter_Tree *const t_copy = lua_newuserdata(L, sizeof(struct ltreesitter_Tree));
	t_copy->tree = ts_tree_copy(t->tree);
	t_copy->src = src_copy;
	t_copy->src_len = t->src_len;
	t_copy->own_str = t->own_str;

	setmetatable(L, LTREESITTER_TREE_METATABLE_NAME);
	return 1;
}

static inline bool is_non_negative(lua_State *L, int i) {
	return lua_tonumber(L, i) >= 0;
}

// Maybe make this Tree.Edit?
/* @teal-inline [[
   record TreeEdit
      start_byte: integer
      old_end_byte: integer
      new_end_byte: integer

      start_point: Point
      old_end_point: Point
      new_end_point: Point
   end
]]*/
/* @teal-export Tree.edit_s: function(Tree, TreeEdit) [[
   Create an edit to the given tree
]] */
static int tree_edit_s(lua_State *L) {
	lua_settop(L, 2);
	lua_checkstack(L, 15);
	ltreesitter_Tree *t = ltreesitter_check_tree_arg(L, 1);

	// get the edit struct from table
	luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "expected table");

	expect_field(L, 2, "start_byte", LUA_TNUMBER);
	expect_field(L, 2, "old_end_byte", LUA_TNUMBER);
	expect_field(L, 2, "new_end_byte", LUA_TNUMBER);

	expect_field(L, 2, "start_point", LUA_TTABLE);
	expect_field(L, -1, "row", LUA_TNUMBER);
	expect_field(L, -2, "column", LUA_TNUMBER);

	expect_field(L, 2, "old_end_point", LUA_TTABLE);
	expect_nested_field(L, -1, "old_end_point", "row", LUA_TNUMBER);
	expect_nested_field(L, -2, "old_end_point", "column", LUA_TNUMBER);

	expect_field(L, 2, "new_end_point", LUA_TTABLE);
	expect_nested_field(L, -1, "new_end_point", "row", LUA_TNUMBER);
	expect_nested_field(L, -2, "new_end_point", "column", LUA_TNUMBER);

	// type checked stack
	// 1.   tree
	// 2.   table argument
	// 3.   start_byte (u32)
	// 4.   old_end_byte (u32)
	// 5.   new_end_byte (u32)
	// 6.   start_point { .row (u32), .column (u32) }
	// 7.   start_point.row (u32)
	// 8.   start_point.col (u32)
	// 9.   old_end_point { .row (u32), .column (u32) }
	// 10.  old_end_point.row (u32)
	// 11.  old_end_point.col (u32)
	// 12.  new_end_point { .row (u32), .column (u32) }
	// 13.  new_end_point.row (u32)
	// 14.  new_end_point.col (u32)

	ts_tree_edit(t->tree, &(const TSInputEdit){
	                          .start_byte = lua_tonumber(L, 3),
	                          .old_end_byte = lua_tonumber(L, 4),
	                          .new_end_byte = lua_tonumber(L, 5),

	                          .start_point = {.row = lua_tonumber(L, 7), .column = lua_tonumber(L, 8)},
	                          .old_end_point = {.row = lua_tonumber(L, 10), .column = lua_tonumber(L, 11)},
	                          .new_end_point = {.row = lua_tonumber(L, 13), .column = lua_tonumber(L, 14)},
	                      });
	return 0;
}

/* @teal-export Tree.edit: function(
         Tree,
         start_byte: integer,
         old_end_byte: integer,
         new_end_byte: integer,
         start_point_row: integer,
         start_point_col: integer,
         old_end_point_row: integer,
         old_end_point_col: integer,
         new_end_point_row: integer,
         new_end_point_col: integer
      ) [[
   Create an edit to the given tree
]] */
static int tree_edit(lua_State *L) {
	ltreesitter_Tree *t = ltreesitter_check_tree_arg(L, 1);
	TSInputEdit edit = (TSInputEdit){
		.start_byte = luaL_checkinteger(L, 2),
		.old_end_byte = luaL_checkinteger(L, 3),
		.new_end_byte = luaL_checkinteger(L, 4),
		.start_point = {.row = luaL_checkinteger(L, 5), .column = luaL_checkinteger(L, 6)},
		.old_end_point = {.row = luaL_checkinteger(L, 7), .column = luaL_checkinteger(L, 8)},
		.new_end_point = {.row = luaL_checkinteger(L, 9), .column = luaL_checkinteger(L, 10)},
	};

	ts_tree_edit(t->tree, &edit);
	return 0;
}

static int tree_gc(lua_State *L) {
	ltreesitter_Tree *t = ltreesitter_check_tree_arg(L, 1);
#ifdef LOG_GC
	printf("Tree %p is being garbage collected\n", t);
#endif
	if (t->own_str) {
#ifdef LOG_GC
		printf("Tree %p owns its string %p, collecting that too...\n", t, t->src);
#endif
		free((char *)t->src);
	}
	ts_tree_delete(t->tree);
	return 0;
}

static const luaL_Reg tree_methods[] = {
    {"root", tree_push_root},
    {"copy", tree_copy},
    {"edit", tree_edit},
    {"edit_s", tree_edit_s},
    {NULL, NULL}};
static const luaL_Reg tree_metamethods[] = {
    {"__gc", tree_gc},
    {"__tostring", tree_to_string},
    {NULL, NULL}};

void ltreesitter_create_tree_metatable(lua_State *L) {
	create_metatable(L, LTREESITTER_TREE_METATABLE_NAME, tree_metamethods, tree_methods);
}
