/*
** lprefix.h — Lua 5.4.7, version embarquée ESP32
** LUA_USE_C89 est défini dans luaconf.h — les extensions POSIX sont désactivées
*/
#ifndef lprefix_h
#define lprefix_h

/* Sur ESP32 (Windows cross-compile), éviter les warnings ISO C */
#if defined(_WIN32)
#  if !defined(_CRT_SECURE_NO_WARNINGS)
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#endif

#endif /* lprefix_h */
