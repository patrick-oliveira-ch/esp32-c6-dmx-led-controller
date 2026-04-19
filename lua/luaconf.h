#ifndef luaconf_h
#define luaconf_h

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>

/*
** Lua 5.4.7 — Configuration embarquée pour ESP32-C6
** Float 32-bit, Integer int 32-bit
*/

/* ── Désactiver computed goto (lvm.c) — ljumptab.h absent ── */
#define LUA_USE_JUMPTABLE   0

/* ── Attributs du type float (requis par lmathlib.c, lvm.c) ─ */
#define l_floatatt(x)           (LUA_NUMBER_##x)
#define LUA_NUMBER_MANT_DIG     FLT_MANT_DIG
#define LUA_NUMBER_MAX_10_EXP   FLT_MAX_10_EXP

/* ── Linkage / visibilité interne ────────────────────────── */
#define LUAI_FUNC       extern
#define LUAI_DDEF       /* empty */
#define LUAI_DDEC(dec)  extern dec

/* ── Extra space dans lua_State ──────────────────────────── */
#define LUA_EXTRASPACE  (sizeof(void *))

/* ── Détection entier 32-bit (llimits.h) ─────────────────── */
#define LUAI_IS32INT    1

/* ── Math ────────────────────────────────────────────────── */
#define l_floor(x)          floorf(x)
#define l_sprintf(s,sz,f,i)         snprintf(s,sz,f,i)
#define lua_pointer2str(buff,sz,p)  snprintf(buff,sz,"%p",p)

/* ── Modificateurs de format printf ──────────────────────── */
#define LUA_NUMBER_FRMLEN   ""      /* float : pas de modificateur */
#define LUA_INTEGER_FRMLEN  ""      /* int   : pas de modificateur */

/* ── Branch hints ────────────────────────────────────────── */
#if defined(__GNUC__)
#define l_likely(x)     (__builtin_expect(((x) != 0), 1))
#define l_unlikely(x)   (__builtin_expect(((x) != 0), 0))
#else
#define l_likely(x)     (x)
#define l_unlikely(x)   (x)
#endif
#define luai_likely(x)   l_likely(x)
#define luai_unlikely(x) l_unlikely(x)

/* ── Types numériques ──────────────────────────────────── */

/* float au lieu de double */
#define LUA_NUMBER          float
#define LUAI_UACNUMBER      float
#define LUA_NUMBER_FMT      "%.7g"
#define LUAI_NUMFORMAT      "%.7g"
#define l_mathop(x)         x##f

/* int au lieu de long long */
#define LUA_INTEGER         int
#define LUA_UNSIGNED        unsigned int
#define LUAI_UACINT         int
#define LUA_INTEGER_FMT     "%d"
#define LUA_MAXINTEGER      INT_MAX
#define LUA_MININTEGER      INT_MIN
#define LUA_MAXUNSIGNED     UINT_MAX

/* Type de contexte continuation */
#define LUA_KCONTEXT        ptrdiff_t

/* ── Visibilité API ────────────────────────────────────── */
#define LUA_API             extern
#define LUALIB_API          extern
#define LUAMOD_API          extern

/* ── Limites ───────────────────────────────────────────── */
#define LUAI_MAXSTACK       200
#define LUA_IDSIZE          60

/* ── Buffer lauxlib ────────────────────────────────────── */
#define LUAL_BUFFERSIZE     128

/* Alignement maximum pour union dans luaL_Buffer */
#define LUAI_MAXALIGN       volatile double u_d; void *u_p; lua_Integer u_i; long u_l

/* ── Chemins (inutilisés sur embedded) ─────────────────── */
#define LUA_PATH_DEFAULT    ""
#define LUA_CPATH_DEFAULT   ""
#define LUA_DIRSEP          "/"
#define LUA_PATH_SEP        ";"
#define LUA_PATH_MARK       "?"
#define LUA_EXEC_DIR        "!"
#define LUA_IGMARK          "-"

/* ── Locale ────────────────────────────────────────────── */
#define lua_getlocaledecpoint()     '.'

/* ── Conversions nombre <-> string ─────────────────────── */
#define lua_number2str(s,sz,n)  \
    snprintf((s), (sz), LUA_NUMBER_FMT, (LUAI_UACNUMBER)(n))

#define lua_str2number(s,p)     strtof((s), (p))

#define lua_integer2str(s,sz,n) \
    snprintf((s), (sz), LUA_INTEGER_FMT, (LUAI_UACINT)(n))

#define lua_numbertointeger(n,p)  \
    ((n) >= (LUA_NUMBER)(LUA_MININTEGER) &&  \
     (n) < -(LUA_NUMBER)(LUA_MININTEGER) &&  \
     (*(p) = (LUA_INTEGER)(n), 1))

/* ── Misc interne ──────────────────────────────────────── */
#define LUA_USE_C89

#endif /* luaconf_h */
