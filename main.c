#include <stdlib.h>
#include <stdio.h>

#define lua_assert(x) ((x) ? (void)0 : luaU_assert(#x, __FILE__, __LINE__))

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <lstate.h>

void luaU_assert(const char *msg, const char *file, const int line)
{
  printf("lua_assert: %s at %s:%d\n", msg, file, line);
  char *b = NULL; *(++b) = 0; // segfault to get backtrace in gdb
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
  printf("%-20s | L=%p, nCcalls=%d, nci=%d\n", "before lua_gc", L, L->nCcalls, L->nci);
  lua_gc(L, LUA_GCCOLLECT, 0);
  printf("%-20s | L=%p, nCcalls=%d, nci=%d\n", "after lua_gc", L, L->nCcalls, L->nci);
  lua_assert(L->nCcalls < 65000);
}

static lua_State *luabug_newthread(lua_State *L)
{
  lua_assert(L);
  lua_State *TL = lua_newthread(L);
  lua_assert(TL);
  printf("%-20s | L=%p from %p\n", "create thread", TL, L);
  return TL;
}

static int luabug_thread_resume(lua_State *L, int status, lua_KContext ctx)
{
  puts(__func__);
  lua_assert(L);

  Ldata *data = (Ldata *)ctx;
  lua_assert(data && L == data->L);
  lua_assert(status == LUA_OK || status == LUA_YIELD);

  if (lua_rawgeti(data->L, LUA_REGISTRYINDEX, data->Fref) != LUA_TFUNCTION)
    return luaL_error(L, "not a function");

  int res, nres;
  switch ((res = lua_resume(L, NULL, 0, &nres))) {
    case LUA_OK:
      printf("%-20s | LUA_OK\n", __func__);
      if (data->callback)
        data->callback(L, res, (lua_KContext)data);
      break;

    case LUA_YIELD:
      printf("%-20s | LUA_YIELD\n", __func__);
      break;

    default:
      return luaL_error(data->LG, "coroutine error: %s", lua_tostring(L, -1));
  }

  return 0;
}

static int luabug_thread_stop(lua_State *L, int status, lua_KContext ctx)
{
  puts(__func__);
  lua_assert(L);

  Ldata *data = (Ldata *)ctx;
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
  puts(__func__);
  lua_assert(L);

  Ldata *data = (Ldata *)ctx;
  lua_assert(data && data->LG && !data->L);

  data->L = luabug_newthread(data->LG);
  data->Lref = luaL_ref(data->LG, LUA_REGISTRYINDEX);
  data->callback = luabug_thread_stop;
  data->next = Llist;
  Llist = data;

  return luabug_thread_resume(data->L, lua_status(data->L), (lua_KContext)data);
}

static int luabug_thread(lua_State *L)
{
  puts(__func__);
  lua_assert(L);

  luaL_argcheck(L, lua_isfunction(L, 1), 1, "not a function");
  lua_settop(L, 1);

  Ldata *data = calloc(1, sizeof(Ldata));
  data->LG = L;
  data->Fref = luaL_ref(data->LG, LUA_REGISTRYINDEX);

  return lua_yieldk(L, 0, (lua_KContext)data, luabug_thread_k);
}

static inline int luabug_resume(void)
{
  puts(__func__);

  for (Ldata *data = Llist; data; data = data->next) {
    lua_assert(data->L);
    printf("%-20s | L=%p\n", "resume thread", data->L);
    luabug_thread_resume(data->L, lua_status(data->L), (lua_KContext)data);
    return 1;
  }

  return 0;
}

#define luabug_resume_all() while (luabug_resume() > 0) {}

int main(void)
{
  lua_State *LGlobal = luaL_newstate();
  luaL_openlibs(LGlobal);

  lua_State *L = luabug_newthread(LGlobal);
  int Lref = luaL_ref(LGlobal, LUA_REGISTRYINDEX);
  lua_register(L, "create_thread", luabug_thread);

  if (luaL_dofile(L, "main.lua"))
    return luaL_error(LGlobal, "luaL_dofile error: %s", lua_tostring(L, -1));

  if (!lua_isfunction(L, -1))
    return luaL_error(LGlobal, "script result is not a function");

  for (int res = -1; res != LUA_OK; ) {
    int nres;
    switch ((res = lua_resume(L, NULL, 0, &nres))) {
      case LUA_OK:
        printf("%-20s | LUA_OK\n", __func__);
        break;

      case LUA_YIELD:
        printf("%-20s | LUA_YIELD\n", __func__);
        break;

      default:
        return luaL_error(LGlobal, "coroutine error: %s", lua_tostring(L, -1));
    }

    luabug_resume_all();
    luabug_gc(LGlobal);
  }

  luabug_resume_all();
  luaL_unref(LGlobal, LUA_REGISTRYINDEX, Lref);
  luabug_gc(LGlobal);
  lua_close(LGlobal);
  return 0;
}
