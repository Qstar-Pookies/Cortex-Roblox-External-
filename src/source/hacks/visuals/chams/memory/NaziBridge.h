#pragma once

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

#include <system/memory/memory.h>
#include <game/offsets/offsets.h>
#include <game/sdk.h>
#include <game/math/math.h>
#include <game/game/game.h>
#include <game/cache/core/cache.h>

#ifdef GetClassName
#undef GetClassName
#endif

struct NaziMem {
    template<typename T>
    T Read(std::uintptr_t address) const {
        if (!address) return T{};
        return memory->read<T>(address);
    }
    template<typename T>
    bool Write(std::uintptr_t address, const T& value) const {
        if (!address) return false;
        return memory->write_raw(address, &value, sizeof(T));
    }

    SIZE_T ReadRaw(std::uintptr_t address, void* buf, SIZE_T bytes) const {
        if (!address || !buf || !bytes) return 0;
        return memory->read_raw(address, buf, (size_t)bytes) ? bytes : 0;
    }
    SIZE_T WriteRaw(std::uintptr_t address, const void* buf, SIZE_T bytes) const {
        if (!address || !buf || !bytes) return 0;
        return memory->write_raw(address, buf, (size_t)bytes) ? bytes : 0;
    }
    bool Protect(std::uintptr_t address, std::size_t n, DWORD prot, DWORD* old) const {
        if (!address || !n) return false;
        return memory->protect_raw(address, n, prot, old);
    }
    std::string ReadString(std::uint64_t address) const {
        if (!address) return {};
        return memory->read_string(address);
    }
    bool IsValid(std::uintptr_t address) const {
        return address >= 0x10000 && address < 0x7FFFFFFFFFFFull;
    }
    bool IsAttached() const {
        return memory && memory->is_attached();
    }
    bool IsAlive() const {
        return memory && memory->is_alive();
    }
    HANDLE GetHandle() const {
        return memory ? memory->m_process_handle : nullptr;
    }
    std::uint32_t GetProcessId() const {
        return memory ? memory->m_process_id : 0;
    }
    std::uintptr_t GetModuleBase() const {
        return memory ? memory->m_base_address : 0;
    }
    std::uintptr_t GetModuleBase(const wchar_t* mod) const {
        if (!mod || !memory || !memory->m_process_handle)
            return 0;
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, memory->m_process_id);
        if (snap == INVALID_HANDLE_VALUE)
            return 0;
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        std::uintptr_t base = 0;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, mod) == 0) {
                    base = (std::uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return base;
    }
    std::uintptr_t Alloc(std::size_t n, DWORD prot) const {
        if (!memory || !memory->m_process_handle || !n)
            return 0;
        return (std::uintptr_t)VirtualAllocEx(memory->m_process_handle, nullptr, n,
            MEM_COMMIT | MEM_RESERVE, prot);
    }
    void Free(std::uintptr_t address) const {
        if (!address || !memory || !memory->m_process_handle)
            return;
        VirtualFreeEx(memory->m_process_handle, (void*)address, 0, MEM_RELEASE);
    }
};

inline NaziMem g_Memory{};

inline std::atomic<std::uint64_t> g_meshpart_resolved_off{ 0 };

inline math::vector2 NaziViewportOrigin() {
    static math::vector2 s_origin{ 0.f, 0.f };
    static std::uint64_t s_last = 0;
    const std::uint64_t now = GetTickCount64();
    if (now - s_last >= 100) {
        s_last = now;
        HWND w = game::get_roblox_window();
        POINT pt{ 0, 0 };
        if (w && ClientToScreen(w, &pt))
            s_origin = math::vector2{ (float)pt.x, (float)pt.y };
    }
    return s_origin;
}

namespace Cheat {

class Instance {
public:
    std::uint64_t address{ 0 };
    Instance() = default;
    explicit Instance(std::uint64_t addr) : address(addr) {}

    std::string GetName() const {
        if (!address) return "";
        return rbx::c_nameable(address).get_name();
    }
    std::string GetClassName() const {
        if (!address) return "";
        return rbx::c_nameable(address).get_class_name();
    }
    std::vector<Instance> GetChildren() const {
        std::vector<Instance> out;
        if (!address) return out;
        for (std::uint64_t c : rbx::c_instance(address).get_children())
            out.emplace_back(c);
        return out;
    }
};

class MeshPart : public Instance {
public:
    using Instance::Instance;
    std::string GetMeshId() const {
        if (!address) return "Unknown";
        const std::uint64_t resolved = g_meshpart_resolved_off.load(std::memory_order_acquire);
        std::uint64_t cands[3] = {
            resolved ? resolved : Offsets::MeshPart::MeshId,
            Offsets::MeshPart::MeshId,
            0x300,
        };
        for (std::uint64_t off : cands) {
            if (!off)
                continue;
            std::string s = memory->read_string(address + off);
            if (!s.empty() && s != "Unknown" && s != "str_error") return s;
            std::uint64_t p = memory->read<std::uint64_t>(address + off);
            if (p >= 0x10000) {
                s = memory->read_string(p);
                if (!s.empty() && s != "Unknown" && s != "str_error") return s;
            }
        }
        return "Unknown";
    }
};

class SpecialMesh : public Instance {
public:
    using Instance::Instance;
    std::string GetMeshId() const {
        if (!address) return "Unknown";
        std::string s = memory->read_string(address + Offsets::SpecialMesh::MeshId);
        if (!s.empty() && s != "Unknown") return s;
        std::uint64_t p = memory->read<std::uint64_t>(address + Offsets::SpecialMesh::MeshId);
        if (!p) return "Unknown";
        return memory->read_string(p);
    }
};

struct PlayerCache {
    std::uint64_t character{ 0 };
};

class PlayerHandler {
public:
    template<typename F>
    static void ForEachPlayer(F&& fn) {
        std::vector<std::uint64_t> chars;
        {
            std::lock_guard<std::mutex> lk(cache::mtx);
            chars.reserve(cache::players.size());
            for (const auto& e : cache::players) {
                if (e.instance.address)
                    chars.push_back(e.instance.address);
            }
        }
        for (std::uint64_t ch : chars) {
            PlayerCache c{};
            c.character = ch;
            fn(c);
        }
    }
};

namespace Globals {
    struct DataModelRef {
        std::uint64_t address{ 0 };
    };
    inline DataModelRef InstanceDataModelForMesh() {
        DataModelRef r{};
        if (game::datamodel)
            r.address = game::datamodel->address;
        return r;
    }
}

namespace Features {
namespace RaycastEngine {

template<typename F>
inline void VisitOccluders(const Vector3&, float, std::size_t, F&&) {}
}
}

}
