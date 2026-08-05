#include "crash_handler.h"

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include <cstdint>

#include "Config/logger-config.h"
#include "Config/path.h"
#include "Logger/logger.h"
#include "Memory/common.h"
#include "Windows/constants.h"


// ---------------------------------------------------------------------------------------------------------
// Constraints this file works under
//
// Everything below the install function runs while the process is already broken, so it must not allocate,
// must not touch iostreams, and must not take a lock anything else might hold. That rules out std::format,
// the logger, the CRT heap and GetModuleFileName (loader lock). Hence the hand rolled text writer, the module
// table snapshotted at install time, and the fixed size buffers.
// ---------------------------------------------------------------------------------------------------------


namespace
{
    constexpr size_t k_header_size = 2048;
    constexpr size_t k_slot_size = 512;
    constexpr size_t k_slot_count = 48;
    constexpr size_t k_ring_size = k_slot_size * k_slot_count;
    constexpr size_t k_report_size = 192 * 1024;
    constexpr size_t k_mapping_size = k_header_size + k_ring_size + k_report_size;

    constexpr size_t k_max_modules = 640;
    constexpr int k_max_frames = 96;
    constexpr int k_max_scanned_stack_frames = 64;
    constexpr size_t k_stack_scan_words = 4096;

    // Rate limits for reports escalated from the vectored handler. See vectored_handler for why it escalates
    // at all. A full report costs a stack walk, a dump costs disk, so the dump is throttled much harder.
    constexpr uint64_t k_first_chance_report_interval_ms = 250;
    constexpr uint64_t k_first_chance_dump_interval_ms = 10 * 1000;

    struct ModuleEntry
    {
        uint64_t base;
        uint64_t size;
        char name[64];
    };

    struct DbgHelp
    {
        using MiniDumpWriteDump_t = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                  PMINIDUMP_EXCEPTION_INFORMATION,
                                                  PMINIDUMP_USER_STREAM_INFORMATION,
                                                  PMINIDUMP_CALLBACK_INFORMATION);
        using SymSetOptions_t = DWORD(WINAPI*)(DWORD);
        using SymInitializeW_t = BOOL(WINAPI*)(HANDLE, PCWSTR, BOOL);
        using SymRefreshModuleList_t = BOOL(WINAPI*)(HANDLE);
        using SymFromAddrW_t = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFOW);
        using SymGetLineFromAddrW64_t = BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINEW64);
        using StackWalk64_t = BOOL(WINAPI*)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
                                            PREAD_PROCESS_MEMORY_ROUTINE64,
                                            PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                            PGET_MODULE_BASE_ROUTINE64,
                                            PTRANSLATE_ADDRESS_ROUTINE64);
        using SymFunctionTableAccess64_t = PVOID(WINAPI*)(HANDLE, DWORD64);
        using SymGetModuleBase64_t = DWORD64(WINAPI*)(HANDLE, DWORD64);

        MiniDumpWriteDump_t MiniDumpWriteDump;
        SymSetOptions_t SymSetOptions;
        SymInitializeW_t SymInitializeW;
        SymRefreshModuleList_t SymRefreshModuleList;
        SymFromAddrW_t SymFromAddrW;
        SymGetLineFromAddrW64_t SymGetLineFromAddrW64;
        StackWalk64_t StackWalk64;
        SymFunctionTableAccess64_t SymFunctionTableAccess64;
        SymGetModuleBase64_t SymGetModuleBase64;
    };

    // All process lifetime state. Deliberately plain globals: no constructors to run, nothing to initialise
    // lazily, so the handlers can read it at any point including before or after CRT teardown.
    char* g_mapping = nullptr;
    HANDLE g_mapping_file = INVALID_HANDLE_VALUE;
    HANDLE g_mapping_handle = nullptr;

    ModuleEntry g_modules[k_max_modules]{};
    volatile LONG g_module_count = 0;

    DbgHelp g_dbghelp{};
    bool g_dbghelp_ready = false;
    bool g_symbols_ready = false;

    volatile LONG g_exception_seq = 0;
    volatile LONG g_uef_reported = 0;
    volatile LONG64 g_last_first_chance_report_tick = 0;
    volatile LONG64 g_last_first_chance_dump_tick = 0;

    // Guards against recursion: writing a report calls into dbghelp, which raises and handles its own
    // exceptions internally, and every one of those comes back through our vectored handler.
    thread_local bool t_in_handler = false;

    wchar_t g_dump_path[WIN32_MAX_PATH]{};
    wchar_t g_report_path[WIN32_MAX_PATH]{};


    // -----------------------------------------------------------------------------------------------------
    // Allocation free text writer over a fixed region.
    // -----------------------------------------------------------------------------------------------------

    class TextWriter
    {
    public:
        TextWriter(char* buffer, size_t capacity)
            : m_buffer(buffer), m_capacity(capacity), m_length(0)
        {
        }

        void ch(char c)
        {
            if (m_length < m_capacity)
                m_buffer[m_length++] = c;
        }

        void str(const char* s)
        {
            if (!s)
                return;
            while (*s && m_length < m_capacity)
                m_buffer[m_length++] = *s++;
        }

        // Symbol names and module paths come back from the Windows APIs as UTF-16. Anything outside ASCII is
        // not worth handling here, so it becomes '?'.
        void wstr(const wchar_t* s)
        {
            if (!s)
                return;
            while (*s && m_length < m_capacity)
            {
                const wchar_t wc = *s++;
                m_buffer[m_length++] = (wc >= 0x20 && wc < 0x7F) ? static_cast<char>(wc) : '?';
            }
        }

        void hex(uint64_t value, int min_digits = 1)
        {
            char digits[16];
            int count = 0;
            do
            {
                digits[count++] = "0123456789abcdef"[value & 0xF];
                value >>= 4;
            }
            while (value != 0 && count < 16);

            while (count < min_digits && count < 16)
                digits[count++] = '0';

            while (count > 0)
                ch(digits[--count]);
        }

        void dec(uint64_t value)
        {
            char digits[24];
            int count = 0;
            do
            {
                digits[count++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            }
            while (value != 0 && count < 24);

            while (count > 0)
                ch(digits[--count]);
        }

        void addr(uint64_t value)
        {
            str("0x");
            hex(value, 16);
        }

        void pad_to(size_t column)
        {
            while (m_length < column)
                ch(' ');
        }

        void line()
        {
            ch('\r');
            ch('\n');
        }

        size_t length() const { return m_length; }
        size_t capacity() const { return m_capacity; }
        bool full() const { return m_length >= m_capacity; }

    private:
        char* m_buffer;
        size_t m_capacity;
        size_t m_length;
    };


    // -----------------------------------------------------------------------------------------------------
    // Module table. Snapshotted at install so the vectored handler can attribute an address without taking
    // the loader lock, which the faulting thread may well be holding.
    // -----------------------------------------------------------------------------------------------------

    void refresh_module_table()
    {
        HMODULE handles[k_max_modules];
        DWORD needed = 0;

        if (!EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &needed))
            return;

        const size_t count = min(static_cast<size_t>(needed) / sizeof(HMODULE), k_max_modules);
        LONG written = 0;

        for (size_t i = 0; i < count; ++i)
        {
            MODULEINFO info{};
            if (!GetModuleInformation(GetCurrentProcess(), handles[i], &info, sizeof(info)))
                continue;

            ModuleEntry& entry = g_modules[written];
            entry.base = reinterpret_cast<uint64_t>(info.lpBaseOfDll);
            entry.size = info.SizeOfImage;
            entry.name[0] = '\0';

            char name[64]{};
            if (GetModuleBaseNameA(GetCurrentProcess(), handles[i], name, sizeof(name) - 1) > 0)
            {
                size_t n = 0;
                while (name[n] != '\0' && n < sizeof(entry.name) - 1)
                {
                    entry.name[n] = name[n];
                    ++n;
                }
                entry.name[n] = '\0';
            }

            ++written;
        }

        // Publish the count last so a concurrent reader never sees a half filled entry.
        InterlockedExchange(&g_module_count, written);
    }


    const ModuleEntry* find_module(uint64_t address)
    {
        const LONG count = InterlockedCompareExchange(&g_module_count, 0, 0);

        for (LONG i = 0; i < count; ++i)
        {
            const ModuleEntry& entry = g_modules[i];
            if (address >= entry.base && address < entry.base + entry.size)
                return &entry;
        }

        return nullptr;
    }


    // Writes "module.dll+0x1234" when the address is inside a known image, otherwise the bare address.
    void write_module_offset(TextWriter& out, uint64_t address)
    {
        const ModuleEntry* entry = find_module(address);

        if (!entry)
        {
            out.str("<unknown> ");
            out.addr(address);
            return;
        }

        out.str(entry->name[0] != '\0' ? entry->name : "<unnamed>");
        out.str("+0x");
        out.hex(address - entry->base);
    }


    struct ImageIdentity
    {
        uint32_t timestamp;
        uint32_t size;
    };


    // TimeDateStamp plus SizeOfImage is exactly the key a symbol server uses to identify a binary, so
    // reporting it lets a crash be matched against the right build's pdb offline. Reading the headers
    // directly avoids depending on the version APIs, which resolve through the proxy version.dll we ship.
    bool get_image_identity(HMODULE module, ImageIdentity& identity)
    {
        if (module == nullptr)
            return false;

        const auto base = reinterpret_cast<const unsigned char*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;

        identity.timestamp = nt->FileHeader.TimeDateStamp;
        identity.size = nt->OptionalHeader.SizeOfImage;
        return true;
    }


    void write_image_identity(TextWriter& out, HMODULE module)
    {
        ImageIdentity identity{};
        if (!get_image_identity(module, identity))
        {
            out.str(module == nullptr ? "not loaded" : "bad PE header");
            return;
        }

        out.str("timestamp=0x");
        out.hex(identity.timestamp, 8);
        out.str(" size=0x");
        out.hex(identity.size, 8);
    }


    // Codes that mean memory or execution actually went wrong, as opposed to a debug notification or a
    // language level throw. Note that a hard fault is not automatically fatal: Mono raises access violations
    // to implement null reference checks, and obfuscated code sometimes uses them for control flow.
    bool is_hard_fault(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        case EXCEPTION_INVALID_DISPOSITION:
        case 0xC0000409: // fast fail
            return true;
        default:
            return false;
        }
    }


    // Not worth a ring slot. The debug print codes are what OutputDebugString raises, and the game calls it
    // constantly: in the first report we got back from a player, 25 of the 26 slots were OutputDebugString
    // and the one entry that mattered nearly got evicted.
    bool is_ring_noise(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_SINGLE_STEP:
        case 0x406D1388: // SetThreadName
        case 0x40010006: // DBG_PRINTEXCEPTION_C,      OutputDebugStringA
        case 0x4001000A: // DBG_PRINTEXCEPTION_WIDE_C, OutputDebugStringW
            return true;
        default:
            return false;
        }
    }


    const char* exception_code_name(DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:             return "INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION:         return "PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
        case EXCEPTION_GUARD_PAGE:               return "GUARD_PAGE";
        case EXCEPTION_INVALID_HANDLE:           return "INVALID_HANDLE";
        case 0xE06D7363:                         return "MSVC_CPP_EXCEPTION";
        case 0x406D1388:                         return "SET_THREAD_NAME";
        case 0xE0434352:                         return "CLR_EXCEPTION";
        case 0xC0000409:                         return "FAST_FAIL";
        default:                                 return "UNKNOWN";
        }
    }


    void write_exception_summary(TextWriter& out, const EXCEPTION_RECORD* record)
    {
        out.str("code=0x");
        out.hex(record->ExceptionCode, 8);
        out.ch(' ');
        out.str(exception_code_name(record->ExceptionCode));
        out.str(" at ");
        write_module_offset(out, reinterpret_cast<uint64_t>(record->ExceptionAddress));

        // For an access violation the two extra parameters say what was attempted and where, which is often
        // enough on its own: a read of 0x0 reads very differently from a write to a valid looking pointer.
        if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
             record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
            record->NumberParameters >= 2)
        {
            switch (record->ExceptionInformation[0])
            {
            case 0:  out.str(" reading "); break;
            case 1:  out.str(" writing "); break;
            case 8:  out.str(" executing "); break;
            default: out.str(" accessing "); break;
            }
            out.addr(record->ExceptionInformation[1]);
        }

        if (record->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
            out.str(" [noncontinuable]");
    }


    // -----------------------------------------------------------------------------------------------------
    // Mapped file regions
    // -----------------------------------------------------------------------------------------------------

    char* ring_slot(LONG sequence)
    {
        const size_t index = static_cast<size_t>(static_cast<uint32_t>(sequence) % k_slot_count);
        return g_mapping + k_header_size + index * k_slot_size;
    }


    char* report_region()
    {
        return g_mapping + k_header_size + k_ring_size;
    }


    // -----------------------------------------------------------------------------------------------------
    // Stack walking
    // -----------------------------------------------------------------------------------------------------

    void write_symbol(TextWriter& out, uint64_t address)
    {
        if (!g_symbols_ready)
            return;

        alignas(SYMBOL_INFOW) unsigned char symbol_storage[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(symbol_storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (g_dbghelp.SymFromAddrW && g_dbghelp.SymFromAddrW(GetCurrentProcess(), address, &displacement, symbol))
        {
            out.str("  ");
            out.wstr(symbol->Name);
            if (displacement != 0)
            {
                out.str("+0x");
                out.hex(displacement);
            }
        }

        IMAGEHLP_LINEW64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        if (g_dbghelp.SymGetLineFromAddrW64 &&
            g_dbghelp.SymGetLineFromAddrW64(GetCurrentProcess(), address, &line_displacement, &line))
        {
            out.str("  (");
            out.wstr(line.FileName);
            out.ch(':');
            out.dec(line.LineNumber);
            out.ch(')');
        }
    }


    void write_frame(TextWriter& out, int index, uint64_t address)
    {
        out.str("  #");
        if (index < 10)
            out.ch('0');
        out.dec(static_cast<uint64_t>(index));
        out.ch(' ');
        out.addr(address);
        out.str("  ");
        write_module_offset(out, address);
        write_symbol(out, address);
        out.line();
    }


    void write_stack_walk(TextWriter& out, const CONTEXT* context)
    {
        out.str("--- stack (StackWalk64) ---");
        out.line();

        if (!g_dbghelp_ready || !g_dbghelp.StackWalk64)
        {
            out.str("  dbghelp unavailable");
            out.line();
            return;
        }

        // StackWalk64 mutates the context it is given, so hand it a copy.
        CONTEXT walk_context = *context;

        STACKFRAME64 frame{};
        frame.AddrPC.Offset = walk_context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = walk_context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = walk_context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        for (int i = 0; i < k_max_frames && !out.full(); ++i)
        {
            SetLastError(ERROR_SUCCESS);

            if (!g_dbghelp.StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(),
                                       &frame, &walk_context, nullptr,
                                       g_dbghelp.SymFunctionTableAccess64,
                                       g_dbghelp.SymGetModuleBase64, nullptr))
            {
                // A clean stop at the bottom of the stack also returns FALSE, just without an error set.
                const DWORD error = GetLastError();
                if (error != ERROR_SUCCESS)
                {
                    out.str("  <unwind failed, error ");
                    out.dec(error);
                    out.ch('>');
                    out.line();
                }
                break;
            }

            if (frame.AddrPC.Offset == 0)
                break;

            write_frame(out, i, frame.AddrPC.Offset);
        }
    }


    // The asm trampolines have no Win64 unwind info, so StackWalk64 gives up as soon as one of them is on the
    // stack. Scanning the raw stack for values that land inside a known image recovers the callers anyway. It
    // reports false positives, hence "candidates", but it is usually the more informative of the two lists.
    void write_stack_scan(TextWriter& out, const CONTEXT* context)
    {
        out.str("--- stack (raw scan, includes false positives) ---");
        out.line();

        auto* sp = reinterpret_cast<const uint64_t*>(context->Rsp);
        if (!sp)
            return;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(sp, &mbi, sizeof(mbi)) == 0)
            return;

        const auto region_end = reinterpret_cast<uint64_t>(mbi.BaseAddress) + mbi.RegionSize;
        int found = 0;

        for (size_t i = 0; i < k_stack_scan_words && found < k_max_scanned_stack_frames && !out.full(); ++i)
        {
            const auto slot = reinterpret_cast<uint64_t>(sp + i);
            if (slot + sizeof(uint64_t) > region_end)
                break;

            const uint64_t value = sp[i];
            const ModuleEntry* entry = find_module(value);
            if (!entry)
                continue;

            out.str("  [rsp+0x");
            out.hex(i * sizeof(uint64_t));
            out.str("] ");
            out.addr(value);
            out.str("  ");
            write_module_offset(out, value);
            write_symbol(out, value);
            out.line();
            ++found;
        }

        if (found == 0)
        {
            out.str("  no candidates found");
            out.line();
        }
    }


    void write_registers(TextWriter& out, const CONTEXT* c)
    {
        struct { const char* name; uint64_t value; } regs[] = {
            {"rip", c->Rip}, {"rsp", c->Rsp}, {"rbp", c->Rbp},
            {"rax", c->Rax}, {"rbx", c->Rbx}, {"rcx", c->Rcx}, {"rdx", c->Rdx},
            {"rsi", c->Rsi}, {"rdi", c->Rdi},
            {"r8 ", c->R8}, {"r9 ", c->R9}, {"r10", c->R10}, {"r11", c->R11},
            {"r12", c->R12}, {"r13", c->R13}, {"r14", c->R14}, {"r15", c->R15},
        };

        out.str("--- registers ---");
        out.line();

        for (size_t i = 0; i < ARRAYSIZE(regs); ++i)
        {
            if (i % 4 == 0)
                out.str("  ");
            out.str(regs[i].name);
            out.ch('=');
            out.hex(regs[i].value, 16);
            if (i % 4 == 3 || i == ARRAYSIZE(regs) - 1)
                out.line();
            else
                out.ch(' ');
        }
    }


    void write_module_list(TextWriter& out)
    {
        out.str("--- modules ---");
        out.line();

        const LONG count = InterlockedCompareExchange(&g_module_count, 0, 0);
        for (LONG i = 0; i < count && !out.full(); ++i)
        {
            const ModuleEntry& entry = g_modules[i];
            out.str("  ");
            out.addr(entry.base);
            out.str(" - ");
            out.addr(entry.base + entry.size);
            out.str("  ");
            out.str(entry.name[0] != '\0' ? entry.name : "<unnamed>");
            out.line();
        }
    }


    // -----------------------------------------------------------------------------------------------------
    // Minidump
    // -----------------------------------------------------------------------------------------------------

    bool write_minidump(EXCEPTION_POINTERS* pointers)
    {
        if (!g_dbghelp_ready || !g_dbghelp.MiniDumpWriteDump || g_dump_path[0] == L'\0')
            return false;

        const HANDLE file = CreateFileW(g_dump_path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = pointers;
        info.ClientPointers = FALSE;

        // Sized so a player can actually upload it. Thread stacks plus the module lists are what we read, and
        // MiniDumpNormal already covers those. Deliberately excluded: MiniDumpWithFullMemory and
        // MiniDumpWithIndirectlyReferencedMemory, both of which chase the game's multi-gigabyte heaps. Add
        // MiniDumpWithIndirectlyReferencedMemory here if a report ever needs the objects a stack points at.
        const auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpNormal |
            MiniDumpWithThreadInfo |
            MiniDumpWithUnloadedModules |
            MiniDumpWithProcessThreadData
        );

        const BOOL ok = g_dbghelp.MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                                    type, &info, nullptr, nullptr);
        CloseHandle(file);
        return ok != FALSE;
    }


    // -----------------------------------------------------------------------------------------------------
    // Fatal report
    // -----------------------------------------------------------------------------------------------------

    struct ReportRequest
    {
        EXCEPTION_POINTERS* pointers;
        DWORD thread_id;
        const char* origin;
        LONG sequence;
        bool want_dump;
        bool dump_written;
    };


    void write_report(ReportRequest* request)
    {
        // Module set may have changed since install, and by now a stale entry costs more than the loader lock.
        refresh_module_table();

        if (g_dbghelp_ready && g_symbols_ready && g_dbghelp.SymRefreshModuleList)
            g_dbghelp.SymRefreshModuleList(GetCurrentProcess());

        if (request->want_dump)
            request->dump_written = write_minidump(request->pointers);

        if (!g_mapping)
            return;

        TextWriter out(report_region(), k_report_size - 1);

        out.str("=========================== REPORT ===========================");
        out.line();

        out.str("origin       ");
        out.str(request->origin);
        out.line();

        out.str("ring entry   #");
        out.dec(static_cast<uint64_t>(request->sequence));
        out.line();

        SYSTEMTIME time{};
        GetLocalTime(&time);
        out.str("time         ");
        out.dec(time.wYear); out.ch('-'); out.dec(time.wMonth); out.ch('-'); out.dec(time.wDay);
        out.ch(' ');
        out.dec(time.wHour); out.ch(':'); out.dec(time.wMinute); out.ch(':'); out.dec(time.wSecond);
        out.line();

        out.str("thread       ");
        out.dec(request->thread_id);
        out.line();

        out.str("exception    ");
        write_exception_summary(out, request->pointers->ExceptionRecord);
        out.line();

        out.str("minidump     ");
        out.str(request->want_dump ? (request->dump_written ? "written" : "FAILED") : "skipped (rate limited)");
        out.line();
        out.line();

        write_registers(out, request->pointers->ContextRecord);
        out.line();

        write_stack_walk(out, request->pointers->ContextRecord);
        out.line();

        write_stack_scan(out, request->pointers->ContextRecord);
        out.line();

        write_module_list(out);

        out.str("=========================== END ===========================");
        out.line();

        FlushViewOfFile(g_mapping, k_mapping_size);
    }


    // Reporting runs on its own thread for two reasons: a stack overflow leaves the faulting thread with no
    // room to work in, and MiniDumpWriteDump is documented as preferring a thread that is not the victim.
    DWORD WINAPI report_thread(LPVOID parameter)
    {
        auto* request = static_cast<ReportRequest*>(parameter);

        t_in_handler = true;

        __try
        {
            write_report(request);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // Nothing useful left to do. The breadcrumb ring is already on disk.
        }

        t_in_handler = false;
        return 0;
    }


    void run_report(ReportRequest* request, bool needs_fresh_stack)
    {
        if (!needs_fresh_stack)
        {
            report_thread(request);
            return;
        }

        const HANDLE thread = CreateThread(nullptr, 0, report_thread, request, 0, nullptr);
        if (thread != nullptr)
        {
            WaitForSingleObject(thread, 60 * 1000);
            CloseHandle(thread);
            return;
        }

        // No thread available, so do it here and hope there is enough stack left.
        report_thread(request);
    }


    // -----------------------------------------------------------------------------------------------------
    // Handlers
    // -----------------------------------------------------------------------------------------------------

    // Records every first chance exception into the ring, escalates hard faults to a full report, then always
    // continues the search so normal handling is untouched.
    //
    // Escalating here rather than only from the unhandled filter is not belt and braces, it is the only thing
    // that works in this game. The first report a player sent back recorded a fatal access violation in the
    // ring but had no report and no dump, because the unhandled filter never ran: Unreal wraps its main loop
    // in __try/__except and the game's protection layer installs handlers of its own, so something claims the
    // exception long before it reaches the top level filter.
    //
    // The cost of escalating is that a hard fault is not necessarily fatal. Mono raises access violations to
    // implement null reference checks, so a report written from here is labelled FIRST CHANCE and may describe
    // an exception the process survived. Whether it was fatal is decided by looking at whether the log
    // continues past it.
    LONG CALLBACK vectored_handler(EXCEPTION_POINTERS* pointers)
    {
        if (!g_mapping || !pointers || !pointers->ExceptionRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        // dbghelp raises and handles its own exceptions while walking a stack, and they come back through
        // here. Without this the first escalation would recurse until the stack ran out.
        if (t_in_handler)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD code = pointers->ExceptionRecord->ExceptionCode;

        if (is_ring_noise(code))
            return EXCEPTION_CONTINUE_SEARCH;

        const LONG sequence = InterlockedIncrement(&g_exception_seq);

        __try
        {
            TextWriter out(ring_slot(sequence), k_slot_size - 2);

            out.ch('#');
            out.dec(static_cast<uint64_t>(sequence));
            out.str(" tid=");
            out.dec(GetCurrentThreadId());
            out.str(" tick=");
            out.dec(GetTickCount64());
            out.ch(' ');
            write_exception_summary(out, pointers->ExceptionRecord);
            out.str(" rip=");
            write_module_offset(out, pointers->ContextRecord ? pointers->ContextRecord->Rip : 0);

            out.pad_to(k_slot_size - 2);

            char* slot = ring_slot(sequence);
            slot[k_slot_size - 2] = '\r';
            slot[k_slot_size - 1] = '\n';
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            // Never let the recorder turn a survivable exception into a fatal one.
        }

        if (!is_hard_fault(code) || !pointers->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        // Throttle, because a hard fault can in principle repeat at a high rate.
        const auto now = static_cast<LONG64>(GetTickCount64());

        const LONG64 last_report = InterlockedCompareExchange64(&g_last_first_chance_report_tick, 0, 0);
        if (last_report != 0 && now - last_report < static_cast<LONG64>(k_first_chance_report_interval_ms))
            return EXCEPTION_CONTINUE_SEARCH;

        InterlockedExchange64(&g_last_first_chance_report_tick, now);

        const LONG64 last_dump = InterlockedCompareExchange64(&g_last_first_chance_dump_tick, 0, 0);
        const bool want_dump = last_dump == 0 ||
                               now - last_dump >= static_cast<LONG64>(k_first_chance_dump_interval_ms);
        if (want_dump)
            InterlockedExchange64(&g_last_first_chance_dump_tick, now);

        ReportRequest request{};
        request.pointers = pointers;
        request.thread_id = GetCurrentThreadId();
        request.origin = "FIRST CHANCE (may have been handled, check whether CSharpLog.txt continues past this)";
        request.sequence = sequence;
        request.want_dump = want_dump;

        run_report(&request, code == EXCEPTION_STACK_OVERFLOW);

        return EXCEPTION_CONTINUE_SEARCH;
    }


    LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* pointers)
    {
        if (!pointers || !pointers->ExceptionRecord || !pointers->ContextRecord)
            return EXCEPTION_CONTINUE_SEARCH;

        // One report per process from here, and a second fault while reporting must not recurse. This
        // overwrites whatever the vectored handler wrote, which is what we want: reaching the top level filter
        // proves nothing handled the exception, so this account is the authoritative one.
        if (InterlockedCompareExchange(&g_uef_reported, 1, 0) != 0)
            return EXCEPTION_CONTINUE_SEARCH;

        ReportRequest request{};
        request.pointers = pointers;
        request.thread_id = GetCurrentThreadId();
        request.origin = "UNHANDLED (nothing claimed this exception, the process is terminating)";
        request.sequence = InterlockedCompareExchange(&g_exception_seq, 0, 0);
        request.want_dump = true;

        run_report(&request, true);

        // Best effort pointer for whoever reads CSharpLog.txt first. The logger allocates and takes locks, so
        // it is only safe to reach for once the report itself is already on disk.
        __try
        {
            log_crit(L"FATAL native exception, wrote {} and {}", g_report_path, g_dump_path);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        // Leave Unreal's crash reporter and WER behaviour exactly as it was.
        return EXCEPTION_CONTINUE_SEARCH;
    }


    // -----------------------------------------------------------------------------------------------------
    // Install
    // -----------------------------------------------------------------------------------------------------

    bool init_mapping()
    {
        const auto report_path = get_base_dir() / L"CSharpCrash.txt";
        const auto dump_path = get_base_dir() / L"CSharpCrash.dmp";

        wcsncpy_s(g_report_path, report_path.c_str(), WIN32_MAX_PATH - 1);
        wcsncpy_s(g_dump_path, dump_path.c_str(), WIN32_MAX_PATH - 1);

        g_mapping_file = CreateFileW(g_report_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_mapping_file == INVALID_HANDLE_VALUE)
        {
            log_error(L"Crash handler: could not create {}", g_report_path);
            return false;
        }

        g_mapping_handle = CreateFileMappingW(g_mapping_file, nullptr, PAGE_READWRITE, 0,
                                              static_cast<DWORD>(k_mapping_size), nullptr);
        if (g_mapping_handle == nullptr)
        {
            log_error("Crash handler: CreateFileMapping failed with {}", GetLastError());
            CloseHandle(g_mapping_file);
            g_mapping_file = INVALID_HANDLE_VALUE;
            return false;
        }

        g_mapping = static_cast<char*>(MapViewOfFile(g_mapping_handle, FILE_MAP_WRITE, 0, 0, k_mapping_size));
        if (g_mapping == nullptr)
        {
            log_error("Crash handler: MapViewOfFile failed with {}", GetLastError());
            CloseHandle(g_mapping_handle);
            CloseHandle(g_mapping_file);
            g_mapping_handle = nullptr;
            g_mapping_file = INVALID_HANDLE_VALUE;
            return false;
        }

        // Spaces everywhere so the file reads as fixed width text rather than a wall of NULs, then a CRLF at
        // the end of every ring slot so each recorded exception is its own line.
        FillMemory(g_mapping, k_mapping_size, ' ');
        for (size_t i = 0; i < k_slot_count; ++i)
        {
            char* slot = g_mapping + k_header_size + i * k_slot_size;
            slot[k_slot_size - 2] = '\r';
            slot[k_slot_size - 1] = '\n';
        }

        return true;
    }


    void write_header()
    {
        TextWriter out(g_mapping, k_header_size - 2);

        out.str("ReadyM WukongMp native crash report");
        out.line();
        out.str("pid          ");
        out.dec(GetCurrentProcessId());
        out.line();

        const auto exe_base = reinterpret_cast<uint64_t>(GetModuleHandleA(g_main_module_name));
        out.str("exe base     ");
        out.addr(exe_base);
        out.str(exe_base == g_exe_base_address ? "  (expected)" : "  *** RELOCATED, patch RVAs are wrong ***");
        out.line();

        out.str("expected     ");
        out.addr(g_exe_base_address);
        out.line();

        const HMODULE self_module = GetModuleHandleW(L"dxgi.dll");
        out.str("loader base  ");
        out.addr(reinterpret_cast<uint64_t>(self_module));
        out.line();

        out.str("loader image ");
        write_image_identity(out, self_module);
        out.str("   (match this against the build's dxgi.pdb to symbolise loader frames)");
        out.line();

        out.str("exe image    ");
        write_image_identity(out, GetModuleHandleA(g_main_module_name));
        out.line();

        out.str("dbghelp      ");
        out.str(g_dbghelp_ready ? "loaded" : "MISSING");
        out.str("   symbols ");
        out.str(g_symbols_ready ? "initialised" : "unavailable");
        out.line();

        out.str("minidump     ");
        out.wstr(g_dump_path);
        out.line();
        out.line();
        out.str("Sections below: a ring of the last ");
        out.dec(k_slot_count);
        out.str(" exceptions seen anywhere in the process, then a full report");
        out.line();
        out.str("for the most recent hard fault. The highest #number in the ring is the most recent.");
        out.line();
        out.line();
        out.str("Read the report's origin line first. UNHANDLED means nothing claimed the exception and the");
        out.line();
        out.str("process was terminating, so it is the cause of death. FIRST CHANCE means we reported it before");
        out.line();
        out.str("handling ran, and it may have been handled: decide by whether CSharpLog.txt continues past the");
        out.line();
        out.str("report's timestamp. Mono raises access violations by design to implement null checks.");
        out.line();
    }


    bool init_dbghelp()
    {
        // Full path so the game directory, which contains our own proxy DLLs, cannot be searched first.
        wchar_t system_dir[WIN32_MAX_PATH]{};
        if (GetSystemDirectoryW(system_dir, WIN32_MAX_PATH) == 0)
            return false;

        wchar_t dbghelp_path[WIN32_MAX_PATH]{};
        if (swprintf_s(dbghelp_path, WIN32_MAX_PATH, L"%s\\dbghelp.dll", system_dir) == -1)
            return false;

        const HMODULE dbghelp = LoadLibraryW(dbghelp_path);
        if (dbghelp == nullptr)
            return false;

        g_dbghelp.MiniDumpWriteDump = reinterpret_cast<DbgHelp::MiniDumpWriteDump_t>(
            GetProcAddress(dbghelp, "MiniDumpWriteDump"));
        g_dbghelp.SymSetOptions = reinterpret_cast<DbgHelp::SymSetOptions_t>(
            GetProcAddress(dbghelp, "SymSetOptions"));
        g_dbghelp.SymInitializeW = reinterpret_cast<DbgHelp::SymInitializeW_t>(
            GetProcAddress(dbghelp, "SymInitializeW"));
        g_dbghelp.SymRefreshModuleList = reinterpret_cast<DbgHelp::SymRefreshModuleList_t>(
            GetProcAddress(dbghelp, "SymRefreshModuleList"));
        g_dbghelp.SymFromAddrW = reinterpret_cast<DbgHelp::SymFromAddrW_t>(
            GetProcAddress(dbghelp, "SymFromAddrW"));
        g_dbghelp.SymGetLineFromAddrW64 = reinterpret_cast<DbgHelp::SymGetLineFromAddrW64_t>(
            GetProcAddress(dbghelp, "SymGetLineFromAddrW64"));
        g_dbghelp.StackWalk64 = reinterpret_cast<DbgHelp::StackWalk64_t>(
            GetProcAddress(dbghelp, "StackWalk64"));
        g_dbghelp.SymFunctionTableAccess64 = reinterpret_cast<DbgHelp::SymFunctionTableAccess64_t>(
            GetProcAddress(dbghelp, "SymFunctionTableAccess64"));
        g_dbghelp.SymGetModuleBase64 = reinterpret_cast<DbgHelp::SymGetModuleBase64_t>(
            GetProcAddress(dbghelp, "SymGetModuleBase64"));

        // The dump is the part we cannot reconstruct afterwards, so that one alone decides "ready".
        return g_dbghelp.MiniDumpWriteDump != nullptr;
    }


    void init_symbols()
    {
        if (!g_dbghelp.SymInitializeW || !g_dbghelp.SymSetOptions)
            return;

        g_dbghelp.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES |
                                SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);

        // Search next to dxgi.dll (its own pdb) and in the loader dir. DEFERRED_LOADS keeps this cheap: no
        // pdb is touched until a symbol is actually requested, which only happens once we are already dying.
        wchar_t search_path[WIN32_MAX_PATH * 2]{};
        wchar_t self_path[WIN32_MAX_PATH]{};
        GetModuleFileNameW(GetModuleHandleW(L"dxgi.dll"), self_path, WIN32_MAX_PATH);

        const std::filesystem::path self_dir = std::filesystem::path(self_path).parent_path();
        const auto loader_dir = get_loader_dir();

        if (swprintf_s(search_path, WIN32_MAX_PATH * 2, L"%s;%s",
                       self_dir.c_str(), loader_dir.c_str()) == -1)
            return;

        g_symbols_ready = g_dbghelp.SymInitializeW(GetCurrentProcess(), search_path, TRUE) != FALSE;
    }
}


bool install_crash_handler()
{
    static bool s_installed = false;
    if (s_installed)
        return true;

    refresh_module_table();

    g_dbghelp_ready = init_dbghelp();
    if (g_dbghelp_ready)
        init_symbols();

    if (!init_mapping())
        return false;

    write_header();

    if (AddVectoredExceptionHandler(0, vectored_handler) == nullptr)
        log_error("Crash handler: AddVectoredExceptionHandler failed.");

    SetUnhandledExceptionFilter(unhandled_filter);

    s_installed = true;

    log_info(L"Crash handler installed. Report: {} Dump: {}", g_report_path, g_dump_path);
    log_debug("Crash handler: dbghelp={} symbols={} modules={}",
              g_dbghelp_ready, g_symbols_ready, InterlockedCompareExchange(&g_module_count, 0, 0));

    // NOTE: the version banner elsewhere in the log reads ReadyM.Loader.Wukong.Managed.dll's file version, so
    // it says nothing about this DLL. A package that updated the managed files but shipped a stale dxgi.dll
    // looked completely up to date in the log and cost a full round trip with a player. These two lines are
    // the native loader's own identity: compare them against the dxgi.dll you actually built.
    ImageIdentity self{};
    if (get_image_identity(GetModuleHandleW(L"dxgi.dll"), self))
    {
        log_info("Native loader (dxgi.dll) image: timestamp=0x{:08x} size=0x{:08x}", self.timestamp, self.size);
    }
    else
    {
        log_error("Native loader (dxgi.dll) image identity could not be read.");
    }

    ImageIdentity exe{};
    if (get_image_identity(GetModuleHandleA(g_main_module_name), exe))
    {
        log_info("Game ({}) image: timestamp=0x{:08x} size=0x{:08x}", g_main_module_name, exe.timestamp, exe.size);
    }

    const auto exe_base = reinterpret_cast<uint64_t>(GetModuleHandleA(g_main_module_name));
    if (exe_base != g_exe_base_address)
    {
        log_error("Main module base is 0x{:x} but the loader assumes 0x{:x}. Every patch RVA is off by 0x{:x}.",
                  exe_base, static_cast<uint64_t>(g_exe_base_address), exe_base - g_exe_base_address);
    }
    else
    {
        log_debug("Main module base 0x{:x} matches the assumed base.", exe_base);
    }

    return true;
}
