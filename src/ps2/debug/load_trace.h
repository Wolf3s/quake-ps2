/* ================================================================================================
 * File: load_trace.h
 * Brief: Tracing of the level loading path: from the loading plaque going up, through the
 *        server's spawn, the client's precache and R_NewMap, to the first 3D frame after it.
 *        Every file read along the way is traced too, with its size and how long it took.
 *        NOTE: Shared header between C and C++.
 *
 *        A trace line goes through Con_SafePrintf, so it also lands in the log file
 *        (debug/log_file.h), stamped with the time:
 *
 *          [  41.802] [load +   93 ms] read maps/e1m3.bsp: 1210 KB in 3480 ms
 *
 *        "+ms" is the time since the previous trace line, which is how long the step it closes
 *        took. A read is named by the file COM_FindFile opened last, so the paks' headers and
 *        directories, read at boot without it, show as "read ?". Nothing that runs every frame is
 *        traced, apart from a heartbeat a second while a level comes up (Sys_SendKeyEvents).
 *
 *        PS2_QUAKE_LOAD_TRACE is 0 in the repository: set it to 1 to trace, and back to 0
 *        before committing. When it is 0 every trace compiles to nothing. It also decides
 *        whether the log file is on by default, since that is where the trace is read back.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */
#ifndef PS2_DEBUG_LOAD_TRACE_H
#define PS2_DEBUG_LOAD_TRACE_H

#ifndef PS2_QUAKE_LOAD_TRACE
#define PS2_QUAKE_LOAD_TRACE 0
#endif

#if PS2_QUAKE_LOAD_TRACE

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Prints "[load +<ms since the previous trace> ms] <message>" with Con_SafePrintf, which, unlike
   Con_Printf, never redraws the screen: some traces run inside a frame. */
void PS2_LoadTrace(const char * fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#define PS2_LOAD_TRACE(...) PS2_LoadTrace(__VA_ARGS__)

#else /* PS2_QUAKE_LOAD_TRACE */

#define PS2_LOAD_TRACE(...) ((void)0)

#endif /* PS2_QUAKE_LOAD_TRACE */
#endif /* PS2_DEBUG_LOAD_TRACE_H */
