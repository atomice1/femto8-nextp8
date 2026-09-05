/*
** $Id: lmem.c,v 1.84 2012/05/23 15:41:53 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


//#define DEBUG_ALLOC 1
#ifdef DEBUG_ALLOC
#include <malloc.h>
#endif
#include <stddef.h>

#define lmem_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** (`osize' is the old size, `nsize' is the new size)
**
** * frealloc(ud, NULL, x, s) creates a new block of size `s' (no
** matter 'x').
**
** * frealloc(ud, p, x, 0) frees the block `p'
** (in this specific case, frealloc must return NULL);
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ANSI C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



#define MINSIZEARRAY	4


void *luaM_growaux_ (lua_State *L, void *block, int *size, size_t size_elems,
                     int limit, const char *what) {
  void *newblock;
  int newsize;
  if (*size >= limit/2) {  /* cannot double it? */
    if (*size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    newsize = limit;  /* still have at least one free place */
  }
  else {
    newsize = (*size)*2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_reallocv(L, block, *size, newsize, size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}



/*
** generic allocation routine.
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  size_t realosize = (block) ? osize : 0;
  lua_assert((realosize == 0) == (block == NULL));
#if defined(HARDMEMTESTS)
  if (nsize > realosize && g->gcrunning)
    luaC_fullgc(L, 1);  /* force a GC whenever possible */
#endif
#ifdef DEBUG_ALLOC
  printf("luaM_realloc_(%p, %zu, %zu) = ", block, osize, nsize);
#endif
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  if (newblock == NULL && nsize > 0) {
    api_check(L, nsize > realosize,
                 "realloc cannot fail when shrinking a block");
    if (g->gcrunning) {
      luaC_fullgc(L, 1);  /* try to free some memory... */
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
    }
    if (newblock == NULL) {
      puts("LUA_ERRMEM\n");
      luaD_throw(L, LUA_ERRMEM);
    }
  }
#ifdef DEBUG_ALLOC
#ifdef __linux__
  struct mallinfo2 info = mallinfo2();
  void *sbrk(intptr_t increment);
  printf("%p totalbytes=%ld GCdebt=%ld gettotalbytes=%ld uordblks=%zd sbrk=%zx\n", newblock, g->totalbytes, g->GCdebt, g->totalbytes - g->GCdebt, info.uordblks, (size_t)sbrk(0));
#else
  struct mallinfo info = mallinfo();
  extern char *__heap;
  extern char __end[] __attribute__ ((aligned (4)));
  printf("%p totalbytes=%ld GCdebt=%ld gettotalbytes=%ld uordblks=%zd sbrk=%zd\n", newblock, g->totalbytes, g->GCdebt, g->totalbytes - g->GCdebt, info.uordblks, __heap - __end);
#endif
#endif
  lua_assert((nsize == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + nsize) - realosize;
  return newblock;
}

