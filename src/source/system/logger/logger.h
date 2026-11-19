#pragma once
#define NOMINMAX
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <mutex>
#include <string>
#include <format>
#include <memory>
#include <vector>

#include <game/offsets/offsets.h>
#include <system/memory/memory.h>
#include <game/game/game.h>
#include <game/cache/core/cache.h>

namespace Cheat::Console {

    enum class Color : unsigned short {
        Default   = 0x07,
        Dim       = 0x08,
        Gray      = 0x07,
        White     = 0x0F,
        Red       = 0x0C,
        Green     = 0x0A,
        Blue      = 0x09,
        Cyan      = 0x0B,
        Magenta   = 0x0D,
        Yellow    = 0x0E,
        Orange    = 0x06,
        Teal      = 0x03,
        Purple    = 0x05,
        Lime      = 0x02,
        Sky       = 0x0B,
        Pink      = 0x0D,
        BrightRed = 0x4C,
    };

    namespace detail {
        inline std::mutex g_mu;
        inline bool g_vt = false;
        inline bool g_have_last_crash = false;
        inline unsigned long g_last_crash_code = 0;

        inline void ensure_console()
        {
            static bool once = false;
            if (once)
                return;
            once = true;

            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            if (!h || h == INVALID_HANDLE_VALUE)
                return;

            DWORD mode = 0;
            if (GetConsoleMode(h, &mode))
            {
                mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                g_vt = SetConsoleMode(h, mode) != 0;
            }

            CONSOLE_FONT_INFOEX font{};
            font.cbSize = sizeof(font);
            font.dwFontSize.X = 5;
            font.dwFontSize.Y = 12;
            font.FontFamily = FF_DONTCARE;
            font.FontWeight = FW_NORMAL;
            wcscpy_s(font.FaceName, L"Terminal");
            SetCurrentConsoleFontEx(h, FALSE, &font);
        }

        inline const char* ansi_for(Color c)
        {
            switch (c)
            {
            case Color::Dim:                 return "\x1b[90m";
            case Color::Red:
            case Color::BrightRed:           return "\x1b[31m";
            case Color::Green:
            case Color::Lime:                return "\x1b[32m";
            case Color::Yellow:
            case Color::Orange:              return "\x1b[33m";
            case Color::Blue:                return "\x1b[34m";
            case Color::Magenta:
            case Color::Purple:              return "\x1b[35m";
            case Color::Cyan:
            case Color::Teal:                return "\x1b[36m";
            case Color::White:               return "\x1b[37m";
            case Color::Default:
            default:                         return "\x1b[0m";
            }
        }

        inline void log_locked(Color color, const char* body)
        {
            ensure_console();

            SYSTEMTIME st{};
            GetLocalTime(&st);

            if (g_vt)
            {
                std::printf("\x1b[90m[%02u:%02u:%02u]\x1b[0m %s%s\x1b[0m\n",
                    (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
                    ansi_for(color), body ? body : "");
            }
            else
            {
                std::printf("[%02u:%02u:%02u] %s\n",
                    (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
                    body ? body : "");
            }
            std::fflush(stdout);
        }

        enum class CrashSide {
            User,
            Cheat,
            Both,
            Info
        };

        struct CrashInfo {
            const char* name;
            CrashSide side;
            const char* fix;
            const char* dev;
        };

        inline CrashInfo classify_crash(unsigned long code)
        {
            if (code == 0)
            {
                return {
                    "Clean Exit",
                    CrashSide::Info,
                    "clean exit.",
                    nullptr
                };
            }
            else if (code == 0xC000013A)
            {
                return {
                    "Process Terminated",
                    CrashSide::User,
                    "killed from taskmgr / another app. reopen.",
                    nullptr
                };
            }
            else if (code == 0xC0000135)
            {
                return {
                    "DLL Not Found",
                    CrashSide::User,
                    "reinstall roblox + vcredist.",
                    nullptr
                };
            }
            else if (code == 0xC0000141)
            {
                return {
                    "Invalid Address",
                    CrashSide::Cheat,
                    "cfg/bad call. try off raycast silent / lua.",
                    "raycast stub = in-module cave or xrw+cfg. luavm: call-rax;ret in-module."
                };
            }
            else if (code == 0xC0000142)
            {
                return {
                    "DLL Init Failed",
                    CrashSide::User,
                    "dll init fail. update gpu, vcredist, reinstall roblox.",
                    nullptr
                };
            }
            else if (code == 0xC000007B)
            {
                return {
                    "Bad Image",
                    CrashSide::User,
                    "bad image. delete %localappdata%\\Roblox + reinstall.",
                    nullptr
                };
            }
            else if (code == 0xC0000017 || code == 0xC000009A)
            {
                return {
                    "Out Of Memory",
                    CrashSide::User,
                    "oom. close apps, lower gfx, reboot.",
                    nullptr
                };
            }
            else if (code == 0xC0000006)
            {
                return {
                    "In-Page Error",
                    CrashSide::User,
                    "disk/ram read fail. check disk + memdiag.",
                    nullptr
                };
            }
            else if (code == 0xC0000185)
            {
                return {
                    "IO Device Error",
                    CrashSide::User,
                    "io device error. check disk, reinstall roblox.",
                    nullptr
                };
            }
            else if (code == 0xE06D7363)
            {
                return {
                    "C++ Exception",
                    CrashSide::User,
                    "c++ exception. reinstall roblox usually fixes.",
                    "if only with features on - note which."
                };
            }
            else if (code == 0xC0000005)
            {
                return {
                    "Access Violation",
                    CrashSide::Both,
                    "av. update gpu, disable overlays, reinstall if keeps happening.",
                    "often cheat: bad r/w or silent. off silent/fly/speed."
                };
            }
            else if (code == 0xC0000374)
            {
                return {
                    "Heap Corruption",
                    CrashSide::Cheat,
                    "heap corrupt. features off + still? reinstall roblox.",
                    "bad write (fly/speed/silent). bisect features."
                };
            }
            else if (code == 0xC0000409)
            {
                return {
                    "Stack Buffer Overrun",
                    CrashSide::Cheat,
                    "stack smash. features off? reinstall roblox.",
                    "usually silent inject. off silent, check stub."
                };
            }
            else if (code == 0xC00000FD)
            {
                return {
                    "Stack Overflow",
                    CrashSide::Cheat,
                    "stack overflow. reboot; clean still crash -> reinstall.",
                    "maybe hook recursion. check silent path."
                };
            }
            else if (code == 0xC000001D)
            {
                return {
                    "Illegal Instruction",
                    CrashSide::Both,
                    "illegal insn. reinstall roblox; update windows.",
                    "bad patched bytes in silent stub?"
                };
            }
            else if (code == 0xC0000096)
            {
                return {
                    "Privileged Instruction",
                    CrashSide::Cheat,
                    "priv insn. av inject? add exclusions.",
                    "bad patch / wrong rva."
                };
            }
            else if (code == 0xC000041D)
            {
                return {
                    "Fatal Callback Exception",
                    CrashSide::Both,
                    "fatal callback. reboot, gpu drivers, overlays off, reinstall.",
                    "hooks during window/input callbacks."
                };
            }

            if ((code & 0xF0000000u) == 0xC0000000u)
            {
                return {
                    "NTSTATUS Crash",
                    CrashSide::Both,
                    "unknown ntstatus. reinstall roblox + disable overlays.",
                    "log code + features on."
                };
            }

            return {
                "Unknown Exit",
                CrashSide::Both,
                "weird exit. reopen; reinstall if repeats.",
                "save exit code + features."
            };
        }

        inline const char* side_text(CrashSide side)
        {
            if (side == CrashSide::User)
                return "user (fixable)";
            else if (side == CrashSide::Cheat)
                return "cheat (dev)";
            else if (side == CrashSide::Both)
                return "user + cheat";
            else if (side == CrashSide::Info)
                return "info";
            return "unknown";
        }

        inline void dump_crash_locked(unsigned long exit_code)
        {
            CrashInfo info = classify_crash(exit_code);
            long long signed_code = (long long)(std::int32_t)exit_code;

            log_locked(Color::White, "Cortex crash");
            {
                char body[160]{};
                std::snprintf(body, sizeof(body), "Exit code      0x%08lX (%lld)",
                    exit_code, signed_code);
                log_locked(Color::Yellow, body);
            }
            {
                char body[160]{};
                std::snprintf(body, sizeof(body), "Name           %s", info.name);
                log_locked(Color::Orange, body);
            }
            {
                char body[160]{};
                std::snprintf(body, sizeof(body), "Side           %s", side_text(info.side));

                Color c = Color::Magenta;
                if (info.side == CrashSide::Cheat)
                    c = Color::Red;
                else if (info.side == CrashSide::User)
                    c = Color::Green;
                else if (info.side == CrashSide::Info)
                    c = Color::Gray;

                log_locked(c, body);
            }
            if (info.fix && info.fix[0])
            {
                char body[640]{};
                std::snprintf(body, sizeof(body), "Fix            %s", info.fix);
                log_locked(Color::Lime, body);
            }
            if (info.dev && info.dev[0])
            {
                char body[640]{};
                std::snprintf(body, sizeof(body), "Dev            %s", info.dev);
                log_locked(Color::Cyan, body);
            }
        }
    }

    inline void Clear()
    {
        std::lock_guard<std::mutex> lock(detail::g_mu);
        detail::ensure_console();
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE)
            return;

        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(h, &csbi))
            return;

        const DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
        DWORD written = 0;
        COORD home{ 0, 0 };
        FillConsoleOutputCharacterA(h, ' ', cells, home, &written);
        FillConsoleOutputAttribute(h, csbi.wAttributes, cells, home, &written);
        SetConsoleCursorPosition(h, home);
    }

    inline void Log(Color color, const char* fmt, ...)
    {
        if (!fmt)
            return;

        char body[640]{};
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);

        std::lock_guard<std::mutex> lock(detail::g_mu);
        detail::log_locked(color, body);
    }

    inline void Log(const char* fmt, ...)
    {
        if (!fmt)
            return;

        char body[640]{};
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);

        std::lock_guard<std::mutex> lock(detail::g_mu);
        detail::log_locked(Color::White, body);
    }

    inline void Ptr(Color color, const char* name, std::uint64_t addr)
    {
        Log(color, "%-14s 0x%llX", name ? name : "?", (unsigned long long)addr);
    }

    inline void Ptr(const char* name, std::uint64_t addr)
    {
        Ptr(Color::Cyan, name, addr);
    }

    inline void DumpCrash(unsigned long exit_code)
    {
        std::lock_guard<std::mutex> lock(detail::g_mu);
        detail::g_have_last_crash = true;
        detail::g_last_crash_code = exit_code;
        detail::dump_crash_locked(exit_code);
    }

    inline void DumpLastCrash()
    {
        std::lock_guard<std::mutex> lock(detail::g_mu);
        if (!detail::g_have_last_crash)
            return;
        detail::dump_crash_locked(detail::g_last_crash_code);
    }

    inline void DumpWorld()
    {
        (void)0;
    }

    inline void DumpSilent(bool ok, std::uint64_t handler, std::uint64_t stub,
                    std::uint64_t state, std::uint64_t slot, const char* detail_msg)
    {
        if (ok)
        {
            Log(Color::Green, "Silent inject  ok");
            Ptr(Color::Magenta, "Handler", handler);
            Ptr(Color::Yellow, "Stub", stub);
            Ptr(Color::Cyan, "State", state);
            Ptr(Color::Blue, "Slot", slot);
            Ptr(Color::Teal, "Desc RVA", Offsets::WorldRoot::RaycastBoundDesc);
            Log(Color::Gray, "Fn off         0x%llX",
                (unsigned long long)Offsets::WorldRoot::RaycastBoundFn);
        }
        else
        {
            Log(Color::Red, "Silent inject  fail%s%s",
                detail_msg && detail_msg[0] ? " " : "",
                detail_msg ? detail_msg : "");
            if (handler)
                Ptr(Color::Magenta, "Handler", handler);
            if (stub)
                Ptr(Color::Yellow, "Stub", stub);
            if (slot)
                Ptr(Color::Blue, "Slot", slot);
        }
    }

    inline void DumpGate(bool ok, const char* method, std::uint64_t slot,
                  std::uint64_t handler, std::uint64_t stub,
                  std::uint64_t state, bool cave, int fail)
    {
        if (!ok)
        {
            Log(Color::Red, "Gate install   fail (%d)", fail);
            return;
        }

        Log(Color::Green, "Gate install   ok");
        Log(Color::White, "Method         %s", method ? method : "?");
        Ptr(Color::Blue, "Slot", slot);
        Ptr(Color::Magenta, "Handler", handler);
        Ptr(Color::Yellow, "Stub", stub);
        Ptr(Color::Cyan, "State", state);
        Log(Color::Gray, "Stub place     %s", cave ? "cave" : "rwx");
    }

    inline void GateTimeout(const char* method, std::uint64_t calls)
    {
        Log(Color::Red, "Gate timeout   %s, hits %llu",
            method ? method : "?", (unsigned long long)calls);
    }

}

enum e_level
{
    INFO,
    WARN,
    ERR,
    DEBUG,
};

class c_logger final
{
public:
    c_logger() = default;

    template <e_level level, class... types>
    inline void log(const std::format_string<types...> format, types&&... args)
    {
        Cheat::Console::Color color = Cheat::Console::Color::White;
        if constexpr (level == e_level::INFO) {
            color = Cheat::Console::Color::Green;
        }
        else if constexpr (level == e_level::WARN) {
            color = Cheat::Console::Color::Orange;
        }
        else if constexpr (level == e_level::ERR) {
            color = Cheat::Console::Color::Red;
        }
        else if constexpr (level == e_level::DEBUG) {
            color = Cheat::Console::Color::Cyan;
        }

        std::string formatted_msg = std::format(format, std::forward<types>(args)...);
        Cheat::Console::Log(color, "%s", formatted_msg.c_str());
    }

    template <e_level level, class... types>
    inline void log_inline(const std::format_string<types...> format, types&&... args)
    {
        if constexpr (level == e_level::INFO || level == e_level::DEBUG) {
            return;
        }
        log<level>(format, std::forward<types>(args)...);
    }
};

inline std::unique_ptr<c_logger> logger = std::make_unique<c_logger>();
