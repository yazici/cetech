#ifndef CETECH_LUA_API_H
#define CETECH_LUA_API_H

//==============================================================================
// Includes
//==============================================================================

#include <stdio.h>
#include <celib/handler/handlerid.h>

#include "include/luajit/lua.h"
#include "include/luajit/lauxlib.h"
#include "include/luajit/lualib.h"

#include "celib/stringid/types.h"

//==============================================================================
// Interface
//==============================================================================

int luasys_init();

void luasys_shutdown();

int luasys_get_top(lua_State *);

void luasys_remove(lua_State *, int);

void luasys_pop(lua_State *, int);

int luasys_is_nil(lua_State *, int);

int luasys_is_number(lua_State *, int);

int luasys_value_type(lua_State *, int);

void luasys_push_nil(lua_State *);

void luasys_push_u64(lua_State *,
                     u64 value);

void luasys_push_handler(lua_State *,
                         handler_t value);

void luasys_push_int(lua_State *,
                     int value);

void luasys_push_bool(lua_State *,
                      int value);

void luasys_push_float(lua_State *,
                       float value);

void luasys_push_string(lua_State *,
                        const char *value);

int luasys_to_bool(lua_State *,
                   int i);

int luasys_to_int(lua_State *,
                  int i);

float luasys_to_float(lua_State *,
                      int i);

handler_t luasys_to_handler(lua_State *l,
                            int i);

const char *luasys_to_string(lua_State *,
                             int i);

const char *luasys_to_string_l(lua_State *, int, size_t *);

int luasys_execute_string(const char *str);

void luasys_add_module_function(const char *module, const char *name, const lua_CFunction func);

void luasys_execute_resource(stringid64_t name);

const struct game_callbacks *luasys_get_game_callbacks();

void luasys_execute_boot_script(stringid64_t name);

void luasys_call_global(const char *func, const char *args, ...);


#endif //CETECH_LUA_API_H
