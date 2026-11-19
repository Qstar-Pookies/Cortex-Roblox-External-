#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

#include <hacks/system/settings/settings.h>
#include <game/offsets/offsets.h>
#include <game/offsets/rva.h>
#include <system/memory/memory.h>
#include <game/sdk.h>
#include <game/game/game.h>
#include <game/cache/core/cache.h>

namespace hacks::engine_chams {

struct LayerBackup {
    uint8_t fillmode;
    uint32_t matflags;
    uint32_t param;
    uint32_t flags2;
    uint32_t color;
};

struct EntityBackup {
    uint32_t render_queue_id;
};

static std::unordered_map<uintptr_t, LayerBackup> g_backups;
static std::unordered_map<uintptr_t, EntityBackup> g_entity_backups;
static std::unordered_map<uintptr_t, std::vector<uintptr_t>> g_entity_layers;
static std::unordered_set<uintptr_t> g_known_entities;
static std::unordered_set<uintptr_t> g_weapon_arm_entities;
static std::mutex g_mtx;

static std::uintptr_t g_last_base = 0;
static std::uint32_t g_last_pid = 0;
static std::uintptr_t g_cache_base = 0;
static std::uint32_t g_cache_pid = 0;
static constexpr std::size_t kMaxLayersPerEntity = 256;
static constexpr std::size_t kMaxKnownEntities = 8192;

inline bool is_valid_ptr(uintptr_t ptr) {
    return ptr >= 0x10000 && ptr < 0x7FFFFFFEFFFFull;
}

inline bool is_valid_technique_array(uintptr_t entity);

static void restore_entity_locked(uintptr_t entity)
{
    const bool live = is_valid_technique_array(entity);

    auto layers_it = g_entity_layers.find(entity);
    if (layers_it != g_entity_layers.end()) {
        if (live) {
            for (uintptr_t layer : layers_it->second) {
                if (!is_valid_ptr(layer)) continue;
                auto backup_it = g_backups.find(layer);
                if (backup_it == g_backups.end())
                    continue;

                const auto& b = backup_it->second;
                memory->write<uint8_t>(layer + Offsets::MaterialLayer::FillModeByte, b.fillmode);
                memory->write<uint32_t>(layer + Offsets::MaterialLayer::MatFlags, b.matflags);
                memory->write<uint32_t>(layer + Offsets::MaterialLayer::Param, b.param);
                memory->write<uint32_t>(layer + Offsets::MaterialLayer::Flags2, b.flags2);
                memory->write<uint32_t>(layer + Offsets::MaterialLayer::ColorData, b.color);
                g_backups.erase(backup_it);
            }
        } else {
            for (uintptr_t layer : layers_it->second)
                g_backups.erase(layer);
        }
        g_entity_layers.erase(layers_it);
    }

    auto entity_backup_it = g_entity_backups.find(entity);
    if (entity_backup_it != g_entity_backups.end()) {
        if (live && is_valid_ptr(entity)) {
            memory->write<uint32_t>(entity + Offsets::FastClusterEntity::RenderQueueId,
                entity_backup_it->second.render_queue_id);
        }
        g_entity_backups.erase(entity_backup_it);
    }

    g_known_entities.erase(entity);
}

inline bool is_valid_technique_array(uintptr_t entity) {
    if (!is_valid_ptr(entity)) return false;
    if (!memory || !memory->is_attached()) return false;

    uintptr_t mod_base = memory->m_base_address;
    if (!mod_base) return false;

    uintptr_t base_address = mod_base;
    if (!base_address || base_address < 0x10000 || base_address > 0x7FFFFFFFFFFFull)
        base_address = mod_base;

    uintptr_t vtable_va1 = base_address + Offsets::FastClusterEntity::VTableRva;
    uintptr_t vtable_va2 = base_address + RVA::FastClusterEntity::VTableRva;
    uintptr_t vtable_va3 = base_address + RVA::Chams::Confirmed::RenderEntityVTable;
    uintptr_t vtable_va4 = base_address + Offsets::FastCluster::MeshEntityVTableRva;
    uintptr_t vtable_va5 = base_address + RVA::Chams::Confirmed::FastClusterEntityVTable;

    uintptr_t vt = memory->read<uintptr_t>(entity);
    if (vt != vtable_va1 && vt != vtable_va2 && vt != vtable_va3 && vt != vtable_va4 && vt != vtable_va5) return false;

    uintptr_t arr = memory->read<uintptr_t>(entity + Offsets::FastClusterEntity::TechniqueArrayPtr);
    if (!is_valid_ptr(arr)) return false;

    uintptr_t begin = memory->read<uintptr_t>(arr + Offsets::TechniqueArray::BeginOffset);
    uintptr_t end = memory->read<uintptr_t>(arr + Offsets::TechniqueArray::EndOffset);
    if (!is_valid_ptr(begin) || !is_valid_ptr(end) || end <= begin) return false;

    size_t bytes = end - begin;
    if (bytes > 64 * 1024) return false;

    size_t count = bytes / Offsets::MaterialLayer::Stride;
    if (count == 0 || count > kMaxLayersPerEntity) return false;

    return true;
}

inline bool is_valid_cluster_vt(uintptr_t vt, uintptr_t base)
{
    return vt == base + Offsets::FastCluster::VTableRva
        || vt == base + Offsets::FastCluster::VTableRvaSub
        || vt == base + Offsets::FastCluster::VTableRvaAlt
        || vt == base + Offsets::FastCluster::VTableRvaSubAlt;
}

inline bool is_valid_cluster_binding_vt(uintptr_t vt, uintptr_t base)
{
    return vt == base + Offsets::FastClusterBinding::VTableRva;
}

inline uintptr_t resolve_cluster(uintptr_t node, uintptr_t base)
{
    if (!is_valid_ptr(node)) return 0;
    uintptr_t vt = memory->read<uintptr_t>(node);
    if (!is_valid_ptr(vt)) return 0;
    if (is_valid_cluster_vt(vt, base)) return node;
    if (is_valid_cluster_binding_vt(vt, base)) {
        uintptr_t owner = memory->read<uintptr_t>(node + Offsets::FastClusterBinding::Owner);
        if (is_valid_ptr(owner)) {
            uintptr_t owner_vt = memory->read<uintptr_t>(owner);
            if (is_valid_cluster_vt(owner_vt, base)) return owner;
        }
        for (int off = 0; off <= 0x200; off += 8) {
            uintptr_t p = memory->read<uintptr_t>(node + off);
            if (!is_valid_ptr(p)) continue;
            uintptr_t p_vt = memory->read<uintptr_t>(p);
            if (is_valid_cluster_vt(p_vt, base)) return p;
        }
    }
    return 0;
}

inline void collect_cluster_entities(uintptr_t cluster, uintptr_t base, std::unordered_set<uintptr_t>& out)
{
    if (!is_valid_ptr(cluster)) return;
    for (int off = 0; off <= 0x3F0; off += 8) {
        uintptr_t b = memory->read<uintptr_t>(cluster + off);
        uintptr_t e = memory->read<uintptr_t>(cluster + off + 8);
        if (!is_valid_ptr(b) || !is_valid_ptr(e) || e <= b || ((e - b) & 7)) continue;
        size_t count = (e - b) / 8;
        if (count < 1 || count > 4096) continue;

        bool found = false;
        size_t check_n = min(count, (size_t)8);
        for (size_t k = 0; k < check_n; ++k) {
            uintptr_t ent = memory->read<uintptr_t>(b + k * 8);
            if (!is_valid_ptr(ent)) continue;
            uintptr_t vt = memory->read<uintptr_t>(ent);
            if (vt == base + Offsets::FastClusterEntity::VTableRva ||
                vt == base + RVA::FastClusterEntity::VTableRva ||
                vt == base + Offsets::FastCluster::MeshEntityVTableRva ||
                vt == base + RVA::Chams::Confirmed::RenderEntityVTable ||
                vt == base + RVA::Chams::Confirmed::FastClusterEntityVTable) {
                found = true;
                break;
            }
        }
        if (found) {
            for (size_t i = 0; i < count; ++i) {
                uintptr_t ent = memory->read<uintptr_t>(b + i * 8);
                if (is_valid_technique_array(ent)) {
                    out.insert(ent);
                }
            }
            break;
        }
    }
}

inline void collect_weapon_arm_parts(std::vector<uintptr_t>& out_parts)
{
    if (!memory || !memory->is_attached() || !game::datamodel || !game::datamodel->address)
        return;

    uintptr_t local_char = cache::local_character.address;
    if (!is_valid_ptr(local_char)) {
        auto lsnap = cache::get_local_snap();
        if (lsnap) local_char = lsnap->instance.address;
    }

    auto is_part_name_arm = [](const std::string& name) {
        return name == "LeftUpperArm" || name == "LeftLowerArm" || name == "LeftHand" || name == "Left Arm"
            || name == "RightUpperArm" || name == "RightLowerArm" || name == "RightHand" || name == "Right Arm"
            || name == "LeftArm" || name == "RightArm" || name == "Arms" || name == "Arm";
    };

    auto scan_instance_parts = [&](uintptr_t inst_addr, auto& self, int depth) -> void {
        if (!is_valid_ptr(inst_addr) || depth > 4) return;
        rbx::c_instance inst(inst_addr);
        for (uint64_t child : inst.get_children()) {
            if (!is_valid_ptr(child)) continue;
            rbx::c_instance ch(child);
            std::string cls = ch.get_class_name();
            if (cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" || cls == "CornerWedgePart" || cls == "WedgePart") {
                out_parts.push_back(child);
            }
            if (cls == "Model" || cls == "Folder" || cls == "Tool" || cls == "Accessory") {
                self(child, self, depth + 1);
            }
        }
    };

    if (is_valid_ptr(local_char)) {
        rbx::c_instance char_inst(local_char);
        for (uint64_t child : char_inst.get_children()) {
            if (!is_valid_ptr(child)) continue;
            rbx::c_instance ch(child);
            std::string name = ch.get_name();
            std::string cls = ch.get_class_name();
            if (is_part_name_arm(name)) {
                out_parts.push_back(child);
            }
            if (cls == "Tool" || name.find("Weapon") != std::string::npos || name.find("Gun") != std::string::npos) {
                scan_instance_parts(child, scan_instance_parts, 1);
            }
        }
    }

    uint64_t ws = game::datamodel->find_first_child_by_class("Workspace");
    if (!ws) ws = game::datamodel->find_first_child("Workspace");
    if (is_valid_ptr(ws)) {
        rbx::c_instance ws_inst(ws);

        uint64_t vm_folder = ws_inst.find_first_child("ViewModels");
        if (!vm_folder) vm_folder = ws_inst.find_first_child("Viewmodels");
        if (is_valid_ptr(vm_folder)) {
            scan_instance_parts(vm_folder, scan_instance_parts, 1);
        }

        uint64_t camera = ws_inst.find_first_child_by_class("Camera");
        if (!camera) camera = ws_inst.find_first_child("Camera");
        if (is_valid_ptr(camera)) {
            rbx::c_instance cam_inst(camera);
            for (uint64_t child : cam_inst.get_children()) {
                if (!is_valid_ptr(child)) continue;
                rbx::c_instance ch(child);
                std::string name = ch.get_name();
                std::string cls = ch.get_class_name();
                if (cls == "Model" || cls == "Folder" || cls == "Tool" ||
                    name.find("View") != std::string::npos || name.find("Arm") != std::string::npos ||
                    name.find("Weapon") != std::string::npos || name.find("Gun") != std::string::npos) {
                    scan_instance_parts(child, scan_instance_parts, 1);
                }
            }
        }
    }
}

inline void update_weapon_arm_entities(uintptr_t base_address)
{
    std::vector<uintptr_t> parts;
    collect_weapon_arm_parts(parts);
    std::unordered_set<uintptr_t> new_entities;
    std::unordered_set<uintptr_t> seen_nodes;

    for (uintptr_t part : parts) {
        if (!is_valid_ptr(part)) continue;
        uintptr_t node = memory->read<uintptr_t>(part + Offsets::BasePart::ClusterNode);
        if (!is_valid_ptr(node) || !seen_nodes.insert(node).second) continue;
        uintptr_t cluster = resolve_cluster(node, base_address);
        if (!is_valid_ptr(cluster)) continue;
        collect_cluster_entities(cluster, base_address, new_entities);
    }

    std::lock_guard<std::mutex> lk(g_mtx);
    if (!settings::visuals::engine_chams_weapons) {
        for (uintptr_t e : g_weapon_arm_entities) {
            restore_entity_locked(e);
        }
        g_weapon_arm_entities.clear();
    } else {
        for (auto it = g_weapon_arm_entities.begin(); it != g_weapon_arm_entities.end();) {
            if (!new_entities.count(*it)) {
                restore_entity_locked(*it);
                it = g_weapon_arm_entities.erase(it);
            } else {
                ++it;
            }
        }
        for (uintptr_t e : new_entities) {
            g_weapon_arm_entities.insert(e);
            g_known_entities.insert(e);
        }
    }
}

inline void material_set(uintptr_t entity, bool is_weapon_or_arm = false)
{
    if (!settings::visuals::engine_chams || !is_valid_technique_array(entity))
        return;

    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (!g_entity_backups.count(entity)) {
            EntityBackup b{};
            b.render_queue_id = memory->read<uint32_t>(entity + Offsets::FastClusterEntity::RenderQueueId);
            g_entity_backups[entity] = b;
            if (!g_cache_base) {
                g_cache_base = memory->m_base_address;
                g_cache_pid = memory->m_process_id;
            }
        }
    }

    const int style = is_weapon_or_arm ? settings::visuals::engine_chams_weapons_style : settings::visuals::engine_chams_style;
    const int mode = is_weapon_or_arm ? settings::visuals::engine_chams_weapons_mode : settings::visuals::engine_chams_mode;
    const int color_idx = is_weapon_or_arm ? settings::visuals::engine_chams_weapons_color_index : settings::visuals::engine_chams_color_index;

    uint32_t queue_id = 10;
    if (mode == 0)
        queue_id = 10;
    else if (mode == 1)
        queue_id = 14;
    else if (mode == 2)
        queue_id = 13;
    else
        queue_id = static_cast<uint32_t>(settings::visuals::engine_chams_queue_id);

    if (!is_valid_technique_array(entity))
        return;

    memory->write<uint32_t>(entity + Offsets::FastClusterEntity::RenderQueueId, queue_id);

    uintptr_t arr = memory->read<uintptr_t>(entity + Offsets::FastClusterEntity::TechniqueArrayPtr);
    uintptr_t begin = memory->read<uintptr_t>(arr + Offsets::TechniqueArray::BeginOffset);
    uintptr_t end = memory->read<uintptr_t>(arr + Offsets::TechniqueArray::EndOffset);
    if (!is_valid_ptr(arr) || !is_valid_ptr(begin) || !is_valid_ptr(end) || end <= begin)
        return;
    size_t bytes = end - begin;
    if (bytes > 64 * 1024)
        return;
    size_t count = bytes / Offsets::MaterialLayer::Stride;
    if (count == 0 || count > kMaxLayersPerEntity)
        return;

    uint8_t selected_fill_mode = 0;
    uint32_t custom_param = static_cast<uint32_t>(color_idx + 1);
    uint32_t custom_flags2 = 0;

    switch (style) {
    case 0:
        selected_fill_mode = 0;
        custom_flags2 = 0;
        custom_param = static_cast<uint32_t>(color_idx + 1);
        break;
    case 1:
        selected_fill_mode = 1;
        custom_flags2 = 0;
        custom_param = static_cast<uint32_t>(color_idx + 1);
        break;
    case 2:
        selected_fill_mode = 1;
        custom_flags2 = 0;
        custom_param = static_cast<uint32_t>(color_idx + 1);
        break;
    case 3:
        selected_fill_mode = 0;
        custom_flags2 = 15;
        custom_param = static_cast<uint32_t>(color_idx + 1);
        break;
    default:
        break;
    }

    for (size_t i = 0; i < count; ++i) {
        uintptr_t layer = begin + i * Offsets::MaterialLayer::Stride;
        if (!is_valid_ptr(layer)) continue;

        {
            std::lock_guard<std::mutex> lk(g_mtx);
            if (!g_backups.count(layer)) {
                LayerBackup b{};
                b.fillmode = memory->read<uint8_t>(layer + Offsets::MaterialLayer::FillModeByte);
                b.matflags = memory->read<uint32_t>(layer + Offsets::MaterialLayer::MatFlags);
                b.param = memory->read<uint32_t>(layer + Offsets::MaterialLayer::Param);
                b.flags2 = memory->read<uint32_t>(layer + Offsets::MaterialLayer::Flags2);
                b.color = memory->read<uint32_t>(layer + Offsets::MaterialLayer::ColorData);
                g_backups[layer] = b;
                g_entity_layers[entity].push_back(layer);
            }
        }

        memory->write<uint8_t>(layer + Offsets::MaterialLayer::FillModeByte, selected_fill_mode);
        memory->write<uint32_t>(layer + Offsets::MaterialLayer::MatFlags, 0);
        memory->write<uint32_t>(layer + Offsets::MaterialLayer::Param, custom_param);
        memory->write<uint32_t>(layer + Offsets::MaterialLayer::Flags2, custom_flags2);
        memory->write<uint32_t>(layer + Offsets::MaterialLayer::ColorData, 0xFFFFFFFFu);
    }
}

inline void refresh_known_entities(bool want_player_engine)
{
    if (!memory || !memory->is_attached())
        return;

    std::vector<uintptr_t> entities;
    std::unordered_set<uintptr_t> weapon_entities;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        entities.reserve(g_known_entities.size());
        for (uintptr_t e : g_known_entities)
            entities.push_back(e);
        weapon_entities = g_weapon_arm_entities;
    }

    for (uintptr_t e : entities) {
        if (!is_valid_technique_array(e)) {
            std::lock_guard<std::mutex> lk(g_mtx);
            restore_entity_locked(e);
            continue;
        }

        const bool is_vm = weapon_entities.count(e) > 0;
        if (is_vm) {
            if (settings::visuals::engine_chams_weapons)
                material_set(e, true);
            else {
                std::lock_guard<std::mutex> lk(g_mtx);
                restore_entity_locked(e);
            }
        } else {
            if (want_player_engine)
                material_set(e, false);
            else {
                std::lock_guard<std::mutex> lk(g_mtx);
                restore_entity_locked(e);
            }
        }
    }
}

inline void restore_all()
{
    std::lock_guard<std::mutex> lk(g_mtx);

    const bool attached = memory && memory->is_attached();
    const bool same_session = attached && g_cache_base != 0 &&
        g_cache_base == memory->m_base_address &&
        g_cache_pid != 0 && g_cache_pid == memory->m_process_id;
    const bool can_write = same_session;

    for (auto& [addr, b] : g_backups) {
        if (can_write && is_valid_ptr(addr)) {
            memory->write<uint8_t>(addr + Offsets::MaterialLayer::FillModeByte, b.fillmode);
            memory->write<uint32_t>(addr + Offsets::MaterialLayer::MatFlags, b.matflags);
            memory->write<uint32_t>(addr + Offsets::MaterialLayer::Param, b.param);
            memory->write<uint32_t>(addr + Offsets::MaterialLayer::Flags2, b.flags2);
            memory->write<uint32_t>(addr + Offsets::MaterialLayer::ColorData, b.color);
        }
    }
    g_backups.clear();

    for (auto& [addr, b] : g_entity_backups) {
        if (can_write && is_valid_ptr(addr) && is_valid_technique_array(addr)) {
            memory->write<uint32_t>(addr + Offsets::FastClusterEntity::RenderQueueId, b.render_queue_id);
        }
    }
    g_entity_backups.clear();
    g_entity_layers.clear();
    g_known_entities.clear();
    g_weapon_arm_entities.clear();
    g_cache_base = 0;
    g_cache_pid = 0;
}

inline void thread()
{
    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    uintptr_t max_va = (uintptr_t)si.lpMaximumApplicationAddress;
    MEMORY_BASIC_INFORMATION mbi{};
    static std::vector<uint8_t> buf;
    bool was_enabled = false;
    auto last_full_scan = std::chrono::steady_clock::now();

    while (true) {
      try {
        if (!memory) { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); continue; }
        HANDLE proc = memory->m_process_handle;
        uintptr_t mod_base = memory->m_base_address;
        uintptr_t base_address = mod_base;
        if (!base_address || base_address < 0x10000 || base_address > 0x7FFFFFFFFFFFull)
            base_address = mod_base;

        if (!proc || !base_address || !memory->is_attached()) {
            if (was_enabled) {
                restore_all();
                std::lock_guard<std::mutex> lk(g_mtx);
                g_known_entities.clear();
                g_weapon_arm_entities.clear();
                was_enabled = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        {
            const std::uint32_t cur_pid = memory->m_process_id;
            if (was_enabled && (mod_base != g_last_base || cur_pid != g_last_pid)) {
                std::lock_guard<std::mutex> lk(g_mtx);
                g_backups.clear();
                g_entity_backups.clear();
                g_entity_layers.clear();
                g_known_entities.clear();
                g_weapon_arm_entities.clear();
                g_cache_base = 0;
                g_cache_pid = 0;
                was_enabled = false;
            }
            g_last_base = mod_base;
            g_last_pid = cur_pid;
        }

        const bool class_engine = (settings::visuals::hostile.chams && settings::visuals::hostile.chams_type == 2) ||
            (settings::visuals::friendly.chams && settings::visuals::friendly.chams_type == 2) ||
            (settings::visuals::neutral.chams && settings::visuals::neutral.chams_type == 2);
        const bool want_player_engine = settings::visuals::engine_chams || class_engine;
        const bool want_engine = want_player_engine || settings::visuals::engine_chams_weapons;

        if (want_engine) {
            if (!was_enabled) {
                std::lock_guard<std::mutex> lk(g_mtx);
                g_backups.clear();
                g_entity_backups.clear();
                g_known_entities.clear();
                g_entity_layers.clear();
                g_weapon_arm_entities.clear();
                was_enabled = true;
                last_full_scan = std::chrono::steady_clock::now() - std::chrono::seconds(10);
            }

            if (settings::visuals::engine_chams_weapons) {
                update_weapon_arm_entities(base_address);
            }

            auto now = std::chrono::steady_clock::now();
            bool do_full_scan = want_player_engine && (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_full_scan).count() >= 1000);

            if (do_full_scan) {
                last_full_scan = now;
                uintptr_t vtable_va1 = base_address + Offsets::FastClusterEntity::VTableRva;
                uintptr_t vtable_va2 = base_address + RVA::FastClusterEntity::VTableRva;
                uintptr_t vtable_va3 = base_address + RVA::Chams::Confirmed::RenderEntityVTable;
                uintptr_t vtable_va4 = base_address + Offsets::FastCluster::MeshEntityVTableRva;
                uintptr_t vtable_va5 = base_address + RVA::Chams::Confirmed::FastClusterEntityVTable;

                uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
                std::vector<uintptr_t> found_entities;

                while (addr < max_va) {
                    if (VirtualQueryEx(proc, (LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
                        break;

                    const bool readable_prot =
                        mbi.Protect == PAGE_READWRITE ||
                        mbi.Protect == PAGE_EXECUTE_READWRITE ||
                        mbi.Protect == PAGE_WRITECOPY ||
                        mbi.Protect == PAGE_EXECUTE_WRITECOPY;

                    const bool scannable =
                        mbi.State == MEM_COMMIT &&
                        mbi.Type != MEM_IMAGE &&
                        readable_prot;

                    if (scannable && mbi.RegionSize <= 64 * 1024 * 1024) {
                        uintptr_t region_start = (uintptr_t)mbi.BaseAddress;
                        size_t region_size = mbi.RegionSize;
                        if (buf.size() < region_size)
                            buf.resize(region_size);

                        SIZE_T got = 0;
                        if (ReadProcessMemory(proc, (LPCVOID)region_start, buf.data(), region_size, &got) && got >= 16) {
                            for (size_t i = 0; i + 16 <= got; i += 8) {
                                uintptr_t vt = *(const uintptr_t*)(buf.data() + i);
                                if (vt != vtable_va1 && vt != vtable_va2 && vt != vtable_va3 && vt != vtable_va4 && vt != vtable_va5)
                                    continue;

                                uintptr_t entity = region_start + i;
                                found_entities.push_back(entity);
                            }
                        }
                    }

                    uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                    if (next <= addr)
                        break;
                    addr = next;
                }

                if (!found_entities.empty()) {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    for (uintptr_t entity : found_entities) {
                        if (g_known_entities.size() >= kMaxKnownEntities)
                            break;
                        if (is_valid_technique_array(entity)) {
                            g_known_entities.insert(entity);
                        }
                    }
                }
            }

            refresh_known_entities(want_player_engine);
        } else {
            if (was_enabled) {
                restore_all();
                std::lock_guard<std::mutex> lk(g_mtx);
                g_known_entities.clear();
                g_weapon_arm_entities.clear();
                was_enabled = false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
      } catch (...) {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_backups.clear();
        g_entity_backups.clear();
        g_entity_layers.clear();
        g_known_entities.clear();
        g_weapon_arm_entities.clear();
        g_cache_base = 0;
        g_cache_pid = 0;
        was_enabled = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }
}

}
