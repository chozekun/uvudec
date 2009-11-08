/*
Universal Decompiler (uvudec)
Copyright 2008 John McMaster
JohnDMcMaster@gmail.com
Licensed under the terms of the BSD license.  See LICENSE for details.
*/


#include "uvd_error.h"

#ifndef UV_DEBUG_H
#define UV_DEBUG_H

#define DEBUG_BREAK()			do { printf("DEBUG BREAK\n"); exit(1); } while ( 0 )

#ifdef __cplusplus
extern "C"
{
#endif /* ifdef __cplusplus */

extern int g_verbose_init;
extern int g_verbose_analysis;
extern int g_verbose_processing;
extern int g_verbose_printing;
extern int g_verbose;
extern FILE *g_pDebugFile;
extern FILE *g_pOutputFile;


//Don't print anything
#define UVD_DEBUG_NONE			0
//Only print high level status messages
#define UVD_DEBUG_PASSES		1
//Main sections being executed
#define UVD_DEBUG_SUMMARY		2
//Debug messages from each section
#define UVD_DEBUG_VERBOSE		3
extern int g_verbose_level;

const char *get_last_func();

#define printf_warn printf_debug
#define printf_error printf

/*
uv_assert is only enabled during debugging.
	It is for paranoid error checking
uv_assert_all is always enabled
	It is for checking things like malloc.  Fails on value of false
uv_assert_err is always enabled
	It is like above, except checks for UV_SUCCEDED rather than being false
*/
#define uv_assert_all(x) if( !(x) ) { UV_DEBUG(rc); goto error; }
#define uv_assert_all_ret(x) if( !(x) ) { return UV_DEBUG(rc); }
#define uv_assert_err(x) if( UV_FAILED(x) ) { UV_DEBUG(rc); goto error; }
#define uv_assert_err_ret(x) if( UV_FAILED(x) ) { return UV_DEBUG(UV_ERR_GENERAL); }
#define uv_assert_err_ret_rc(x) if( UV_FAILED(x) ) { return UV_DEBUG(rc); }

//#define NDEBUG
#ifdef NDEBUG 

#define printf_debug(format, ...)
#define printf_debug_level(level, format, ...)
#define uv_assert(x) do{ } while(0)
#define uv_assert_ret(x) do{ } while(0)

/* Disable such messages if not debugging, probably signifigant binary size reduction, speed improvement */
#define UV_DEBUG(x) x
#define UV_ENTER()

#else /* ifdef NDEBUG */

#define UV_ERR		UV_DEBUG


/* I don't check rc on printf anyway, fix later if needed */
//#define printf_debug(format, ...) if( g_verbose ) { printf(format, ## __VA_ARGS__); }
#define printf_debug(format, ...) printf_debug_level(UVD_DEBUG_VERBOSE, format, ## __VA_ARGS__)
void printf_debug_level(int level, const char *format, ...);

#define uv_assert(x) if( !(x) ) { UV_ERR(rc); goto error; }
#define uv_assert_ret(x) if( !(x) ) { return UV_ERR(UV_ERR_GENERAL); }

uv_err_t uv_err_ret_handler(uv_err_t rc, const char *file, int line, const char *func);
void uv_enter(const char *file, int line, const char *func);

#define UV_DEBUG(x) uv_err_ret_handler(x, __FILE__, __LINE__, __FUNCTION__)
#define UV_ENTER() uv_enter(__FILE__, __LINE__, __FUNCTION__)
//#define UV_ENTER() do{ } while(0)

#endif /* ifndef NDEBUG */

#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif /* ifndef UV_DEBUG_H */
