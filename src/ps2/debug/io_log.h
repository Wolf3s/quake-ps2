#pragma once
/* ================================================================================================
 * File: io_log.h
 * Brief: The I/O log: what each level load read, and how long its file calls took, to time a
 *        console's loads with none of the log's own writes inside them. Built for comparing a
 *        load over USB with one off the HDD, one from a pak with one from its compressed pkz
 *        (fs/pkz_pack.cpp), or one with another fileXio read buffer (PS2_QUAKE_FILEXIO_BUFFER_KB
 *        in iop_boot.cpp).
 *
 *        PS2_QUAKE_IO_LOG is 0 in the repository, and every hook then compiles to nothing. For a
 *        test build pass -DPS2_QUAKE_IO_LOG=1 in CONFIG_DEFS, after clearing the objects (make
 *        doesn't track flags):
 *
 *          rm -rf build/release/src && make release CONFIG_DEFS='-DPS2_QUAKE_DEBUG=0 -DPS2_QUAKE_ASSERTS=0 -DPS2_QUAKE_PROFILE=0 -DNDEBUG -DPS2_QUAKE_IO_LOG=1'
 *
 *        and the same rm -rf and a plain make release afterwards. A release build it is: a debug
 *        build also checks the CRC of every file it reads from a pkz.
 *
 *        A load is the loading plaque's lifetime (PS2_LoadingScreenBegin to End): a level change,
 *        a game loaded from the menu, the demo loop's next demo. A "map" disconnects first and
 *        raises no plaque, so it isn't logged. Every file the load opens gets a line, and the
 *        load a summary:
 *
 *          #     ms  inflate_ms       bytes  read_bytes  source  name
 *               139         134     1332228      604939  pkz     maps/e1m2.bsp
 *          # load 1, maps/e1m2.bsp: 49 files, 2.77 MB delivered, 1.30 MB read; 0.29 s in file
 *          #   calls (0.27 s inflating), of 1.50 s
 *          #   the emulator's host: filesystem, through the ROM's FILEIO
 *
 *        "ms" is the time in the file calls for that file: the seek into its pak or the open of
 *        a loose file, the read, the close. "inflate_ms" is the part of it a pkz spent inflating,
 *        "bytes" what the reads delivered, and "read_bytes" what they took off the drive: the
 *        compressed bytes for a pkz, the same as "bytes" otherwise. A demo is a stdio stream
 *        (COM_FOpenFile) read with fread, and only its open is seen.
 *
 *        The lines stay in RAM until the load ends, then go to stdout and are appended to
 *        <gamedir>/io_log.txt with one open/write/close, as frame_log.txt is. Each boot starts
 *        the file afresh. The console says when a load is being recorded and where it was
 *        written: on a console nothing reads stdout. The load trace can't time a load: with
 *        ps2_logfile 1 every line it prints is a write to the drive, ~23 ms over USB.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#ifndef PS2_QUAKE_IO_LOG
#define PS2_QUAKE_IO_LOG 0
#endif

#include <tamtypes.h>

namespace ps2::iolog {

#if PS2_QUAKE_IO_LOG

// The loading plaque went up, and came down (loading_screen.cpp).
void LoadBegin();
void LoadEnd();

// COM_FindFile is opening 'name' to read (PS2_FileOpening): the file the calls after it are for.
void FileOpening(const char * name);

// That file is a loose one (Sys_FileOpenRead), not one in a pak.
void LooseFile();

// That file is in a compressed pack (fs/pkz_pack.cpp), whose reads say what they took off the
// drive and how long they inflated.
void CompressedFile();

// The clock the calls are timed in: the EE timer's 576 kHz tick.
u32 Ticks();

// One of the system's file calls for that file starts (and returns Ticks()), and ends, having
// taken 'ticks' and delivered 'bytes'.
u32 CallBegin();
void Call(u32 ticks, int bytes);

// From the pkz reader: it read 'bytes' off the drive, and inflated for 'ticks'. Counted only
// inside a call, so a demo's fread on its stream is left out, as it is from a pak.
void DriveRead(int bytes);
void Inflate(u32 ticks);

#else // PS2_QUAKE_IO_LOG

inline void LoadBegin() {}
inline void LoadEnd() {}
inline void FileOpening(const char *) {}
inline void LooseFile() {}
inline void CompressedFile() {}
inline u32  Ticks() { return 0; }
inline u32  CallBegin() { return 0; }
inline void Call(u32, int) {}
inline void DriveRead(int) {}
inline void Inflate(u32) {}

#endif // PS2_QUAKE_IO_LOG

// Times one file call (sys.cpp's Sys_File*), for the file the last FileOpening named. Free when the
// log is compiled out.
struct ScopedCall
{
    u32 start = CallBegin();
    int bytes = 0;

    ~ScopedCall() { Call(Ticks() - start, bytes); }
};

} // namespace ps2::iolog
