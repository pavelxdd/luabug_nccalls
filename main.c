#include <stdlib.h>
#include <stdio.h>

#define lua_assert(x) ((x) ? (void)0 : luaU_assert(#x, __FILE__, __LINE__))

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <llimits.h>
#include <lstate.h>

void luaU_assert(const char *msg, const char *file, const int line)
{
  printf("lua_assert: %s at %s:%d\n", msg, file, line);
  // segfault to get backtrace in gdb
  char *b = NULL; *(++b) = 0;
  exit(1);
}

typedef struct Ldata {
  struct Ldata *next;
  lua_State *L, *LG;
  int Lref, Fref;
  lua_KFunction callback;
} Ldata;

static Ldata *Llist = NULL;

static inline void luabug_gc(lua_State *L)
{
  lua_assert(L);

  puts(" --- ");
  printf("%-20s | L=%p lua_gc nCcalls=%d (max=%d), nci=%d\n",
         __func__, L, L->nCcalls, LUAI_MAXCCALLS, L->nci);
  lua_gc(L, LUA_GCCOLLECT, 0);
  printf("%-20s | L=%p lua_gc nCcalls=%d (max=%d), nci=%d\n",
         __func__, L, L->nCcalls, LUAI_MAXCCALLS, L->nci);
  puts(" --- ");

  // it should fail at some point
  lua_assert(L->nCcalls < LUAI_MAXCCALLS);
}

static lua_State *luabug_newthread(lua_State *L)
{
  lua_assert(L);
  lua_State *TL = lua_newthread(L);
  lua_assert(TL);
  printf("%-20s | L=%p lua_newthread L=%p\n", __func__, L, TL);
  return TL;
}

static int luabug_thread_resume(lua_State *L, int status, lua_KContext ctx)
{
  Ldata *data = (Ldata *)ctx;
  printf("%-20s | L=%p data=%p, status=%d\n", __func__, L, data, status);
  lua_assert(L);
  lua_assert(data && L == data->L);
  lua_assert(status == LUA_OK || status == LUA_YIELD);

  int nres;
  const int res = lua_resume(L, data->LG, 0, &nres);
  printf("%-20s | L=%p lua_resume res=%d, nres=%d\n", __func__, L, res, nres);

  if (res != LUA_OK && res != LUA_YIELD)
    return luaL_error(data->LG, "coroutine error: %s", lua_tostring(L, -1));

  lua_pop(L, nres);

  if (res == LUA_OK && data->callback)
    data->callback(L, res, (lua_KContext)data);

  return 0;
}

static int luabug_thread_stop(lua_State *L, int status, lua_KContext ctx)
{
  Ldata *data = (Ldata *)ctx;
  printf("%-20s | L=%p data=%p, status=%d\n", __func__, L, data, status);
  lua_assert(L);
  lua_assert(data && L == data->L);
  lua_assert(status == LUA_OK);

  if (Llist == data) {
    Llist = Llist->next;
  } else {
    for (Ldata *d = Llist; d; d = d->next) {
      if (d->next == data) {
        d->next = data->next;
        break;
      }
    }
  }

  luaL_unref(data->LG, LUA_REGISTRYINDEX, data->Lref);
  luaL_unref(data->LG, LUA_REGISTRYINDEX, data->Fref);
  luabug_gc(data->LG);
  free(data);
  return 0;
}

static int luabug_thread_k(lua_State *L, int status, lua_KContext ctx)
{
  const int Fref = (int)ctx;
  printf("%-20s | L=%p ref=%d, status=%d\n", __func__, L, Fref, status);
  lua_assert(L);

  Ldata *data = malloc(sizeof(Ldata));
  data->LG = L;
  data->L = luabug_newthread(data->LG);
  data->Lref = luaL_ref(data->LG, LUA_REGISTRYINDEX);
  data->Fref = Fref;
  data->callback = luabug_thread_stop;
  data->next = Llist;
  Llist = data;

  if (lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->Fref) != LUA_TFUNCTION)
    return luaL_error(L, "not a function");

  return luabug_thread_resume(data->L, lua_status(data->L), (lua_KContext)data);
}

static int luabug_thread(lua_State *L)
{
  printf("%-20s | L=%p\n", __func__, L);
  lua_assert(L);
  luaL_argcheck(L, lua_isfunction(L, 1), 1, "not a function");
  lua_settop(L, 1);
  return lua_yieldk(L, 0, (lua_KContext)luaL_ref(L, LUA_REGISTRYINDEX), luabug_thread_k);
}

static inline int luabug_resume(void)
{
  Ldata *data = Llist;
  lua_State *L = data ? data->L : NULL;
  printf("%-20s | L=%p data=%p\n", __func__, L, data);

  if (data)
    luabug_thread_resume(L, lua_status(L), (lua_KContext)data);

  return !!data;
}

#define luabug_resume_all() while (luabug_resume()) {}

int main(void)
{
  lua_State *LG = luaL_newstate();
  printf("%-20s | L=%p\n", __func__, LG);
  luaL_openlibs(LG);

  lua_State *L = luabug_newthread(LG);
  int Lref = luaL_ref(LG, LUA_REGISTRYINDEX);
  lua_register(L, "create_thread", luabug_thread);

  if (luaL_dofile(L, "main.lua"))
    return luaL_error(LG, "luaL_dofile error: %s", lua_tostring(L, -1));

  if (!lua_isfunction(L, -1))
    return luaL_error(LG, "script result is not a function");

  while (1) {
    int nres;
    // passing NULL instead of LG makes it easier to reproduce a bug
    const int res = lua_resume(L, NULL, 0, &nres);
    printf("%-20s | L=%p lua_resume res=%d, nres=%d\n", __func__, L, res, nres);

    if (res != LUA_OK && res != LUA_YIELD)
      return luaL_error(LG, "coroutine error: %s", lua_tostring(L, -1));

    lua_pop(L, nres);

    if (res == LUA_OK)
      break;

    luabug_resume_all();
    luabug_gc(LG);
  }

  luabug_resume_all();
  luaL_unref(LG, LUA_REGISTRYINDEX, Lref);
  luabug_gc(LG);
  lua_close(LG);
  return 0;
}
