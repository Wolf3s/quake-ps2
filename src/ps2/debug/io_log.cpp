/* ================================================================================================
 * File: io_log.cpp
 * Brief: The I/O log. See io_log.h.
 *
 * This source code is released under the GNU GPL v2 license.
 * ================================================================================================ */

#include "ps2/common.h"
#include "ps2/debug/io_log.h"

#if PS2_QUAKE_IO_LOG

#include "ps2/system/iop_boot.h" // SyncGameDataDevice, FileXioBufferBytes

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <timer.h> // GetTimerSystemTime

namespace ps2::iolog {
namespace {

// The EE timer counts at BUSCLK/256, 576,000 ticks a second (sys.cpp's Milliseconds).
constexpr u32 kTicksPerSec = 576000;

// A load's lines: about 50 characters a file, and a level load opens 50 to 200 files. The last
// of it is kept for the summary, so a load that opens more drops lines, not its totals.
constexpr int kOutBytes       = 32 * 1024;
constexpr int kSummaryReserve = 512;

// The file the calls are for now, written out as a line once the next one is named.
struct FileEntry
{
    char name[MAX_QPATH];
    u32  ticks;
    int  bytes;
    bool loose;
};

static bool      s_recording      = false;
static int       s_loadNumber     = 0;
static u32       s_loadStartTicks = 0;
static char      s_loadName[MAX_QPATH];
static bool      s_haveFile       = false;
static FileEntry s_file           = {};

// The load's totals.
static int s_files     = 0;
static u32 s_callTicks = 0;
static u32 s_bytes     = 0;

static char s_out[kOutBytes];
static int  s_outUsed      = 0;
static int  s_linesDropped = 0;
static bool s_fileStarted  = false; // the first write of a boot empties io_log.txt
static bool s_fileFailed   = false; // and one that fails turns the file off for the boot

// Appends to the load's text. A file line leaves the summary's reserve alone, and one that won't
// fit is dropped whole.
void Append(int reserve, const char * format, ...) Q_PRINTF_FUNC(2, 3);
void Append(const int reserve, const char * format, ...)
{
    const int room = kOutBytes - s_outUsed - reserve;
    if (room > 1)
    {
        va_list args;
        va_start(args, format);
        const int n = std::vsnprintf(s_out + s_outUsed, static_cast<size_t>(room), format, args);
        va_end(args);

        if (n > 0 && n < room)
        {
            s_outUsed += n;
            return;
        }
    }
    s_out[s_outUsed] = '\0';
    ++s_linesDropped;
}

// Seconds or megabytes with two decimals, from a count of 'unit' parts. The remainder times 100
// stays in 32 bits for both units used (a MB, a second of ticks).
struct Hundredths
{
    u32 whole;
    u32 hundredths;
};

Hundredths InHundredths(const u32 value, const u32 unit)
{
    return { value / unit, ((value % unit) * 100u) / unit };
}

void WriteFileLine()
{
    if (!s_haveFile)
    {
        return;
    }
    s_haveFile = false;

    ++s_files;
    s_callTicks += s_file.ticks;
    s_bytes     += static_cast<u32>(s_file.bytes);

    Append(kSummaryReserve, "%8u  %10d  %-6s  %s\n", s_file.ticks / (kTicksPerSec / 1000u), s_file.bytes,
           s_file.loose ? "file" : "pak", s_file.name);
}

// The load's text to stdout, and appended to <gamedir>/io_log.txt in one write.
void WriteOut(const char * loadName)
{
    std::fputs(s_out, stdout);
    std::fflush(stdout);

    if (s_fileFailed)
    {
        return;
    }

    char path[sizeof(com_gamedir) + sizeof("/io_log.txt")];
    std::snprintf(path, sizeof(path), "%s/io_log.txt", com_gamedir);

    const int flags = s_fileStarted ? O_WRONLY : (O_WRONLY | O_CREAT | O_TRUNC);
    const int fd    = open(path, flags, 0666);
    bool written    = (fd >= 0);
    if (fd >= 0)
    {
        written = (lseek(fd, 0, SEEK_END) >= 0) && (write(fd, s_out, static_cast<size_t>(s_outUsed)) == s_outUsed);
        // A successful close on USB/HDD returns the iomanX slot, not 0 (see ps2-platform.md).
        written = (close(fd) >= 0) && written;
        written = (ps2::sys::SyncGameDataDevice() == 0) && written;
    }
    s_fileStarted = true;

    if (written)
    {
        Con_SafePrintf("I/O log: load %d (%s) written to %s\n", s_loadNumber, loadName, path);
    }
    else
    {
        s_fileFailed = true;
        Con_SafePrintf("I/O log: couldn't write %s; stdout only from here\n", path);
    }
}

} // namespace

u32 Ticks()
{
    // GetTimerSystemTime scales T2 back up to BUSCLK units, whose low 8 bits are always zero.
    return static_cast<u32>(GetTimerSystemTime() >> 8);
}

void LoadBegin()
{
    ++s_loadNumber;
    Con_SafePrintf("I/O log: recording load %d\n", s_loadNumber);

    s_recording    = true;
    s_loadName[0]  = '\0';
    s_haveFile     = false;
    s_files        = 0;
    s_callTicks    = 0;
    s_bytes        = 0;
    s_outUsed      = 0;
    s_out[0]       = '\0';
    s_linesDropped = 0;

    Append(kSummaryReserve, "#       ms       bytes  source  name\n");
    s_loadStartTicks = Ticks();
}

void LoadEnd()
{
    if (!s_recording)
    {
        return;
    }

    const u32 loadTicks = Ticks() - s_loadStartTicks;
    WriteFileLine();
    s_recording = false;

    const char * const name = (s_loadName[0] != '\0') ? s_loadName : "no map";
    const Hundredths mb     = InHundredths(s_bytes, 1024u * 1024u);
    const Hundredths calls  = InHundredths(s_callTicks, kTicksPerSec);
    const Hundredths load   = InHundredths(loadTicks, kTicksPerSec);
    Append(0, "# load %d, %s: %d files, %u.%02u MB read; %u.%02u s in file calls, of %u.%02u s\n", s_loadNumber,
           name, s_files, mb.whole, mb.hundredths, calls.whole, calls.hundredths, load.whole, load.hundredths);

    const int bufferBytes = ps2::sys::FileXioBufferBytes();
    if (bufferBytes > 0)
    {
        Append(0, "#   fileXio read buffer %d KB\n", bufferBytes / 1024);
    }
    else
    {
        Append(0, "#   the emulator's host: filesystem, through the ROM's FILEIO\n");
    }
    if (s_linesDropped > 0)
    {
        Append(0, "#   %d file lines didn't fit in the log's buffer, but are in the totals\n", s_linesDropped);
    }

    WriteOut(name);
}

void FileOpening(const char * name)
{
    if (!s_recording)
    {
        return;
    }

    WriteFileLine();
    s_haveFile = true;
    s_file     = {};
    std::snprintf(s_file.name, sizeof(s_file.name), "%s", name);

    // The load is named for its map: the first world model it opens.
    const size_t length = std::strlen(name);
    if (s_loadName[0] == '\0' && std::strncmp(name, "maps/", 5) == 0 && length > 4 &&
        std::strcmp(name + length - 4, ".bsp") == 0)
    {
        std::snprintf(s_loadName, sizeof(s_loadName), "%s", name);
    }
}

void LooseFile()
{
    if (s_recording && s_haveFile)
    {
        s_file.loose = true;
    }
}

void Call(const u32 ticks, const int bytes)
{
    if (s_recording && s_haveFile)
    {
        s_file.ticks += ticks;
        s_file.bytes += (bytes > 0) ? bytes : 0;
    }
}

} // namespace ps2::iolog

#endif // PS2_QUAKE_IO_LOG
