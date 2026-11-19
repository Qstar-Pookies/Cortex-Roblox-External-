#include "chams_main.h"
#include "chams.h"
#include "shader.h"
#include "cache.h"
#include "NaziBridge.h"

#include <hacks/system/settings/settings.h>
#include <game/offsets/offsets.h>
#include <game/sdk.h>
#include <system/memory/memory.h>

#include <cmath>
#include <cstring>

namespace nazichams {

namespace {

using BoxList = std::vector<Cheat::Visuals::MeshChams::FallbackBox>;

Matrix4x4 make_box_world(const Vector3& pos, const Matrix4x4& rot, const Vector3& sz) {
    Matrix4x4 w{};
    w.m[0][0] = rot.m[0][0] * sz.x;
    w.m[0][1] = rot.m[0][1] * sz.y;
    w.m[0][2] = rot.m[0][2] * sz.z;
    w.m[0][3] = pos.x;
    w.m[1][0] = rot.m[1][0] * sz.x;
    w.m[1][1] = rot.m[1][1] * sz.y;
    w.m[1][2] = rot.m[1][2] * sz.z;
    w.m[1][3] = pos.y;
    w.m[2][0] = rot.m[2][0] * sz.x;
    w.m[2][1] = rot.m[2][1] * sz.y;
    w.m[2][2] = rot.m[2][2] * sz.z;
    w.m[2][3] = pos.z;
    w.m[3][0] = 0.f;
    w.m[3][1] = 0.f;
    w.m[3][2] = 0.f;
    w.m[3][3] = 1.f;
    return w;
}

void collect_scattered_boxes(const cache::entity_t* e, const Vector3& camera, BoxList& out) {
    if (!e || e->parts.empty())
        return;
    for (const auto& kv : e->parts) {
        if (out.size() >= 40)
            break;
        const std::uint64_t part = kv.second.address;
        if (part < 0x10000)
            continue;
        const std::uint64_t prim =
            memory->read<std::uint64_t>(part + Offsets::BasePart::Primitive);
        if (prim < 0x10000)
            continue;
        float rp[12];
        if (!memory->read_raw(prim + Offsets::Primitive::Rotation, rp, sizeof(rp)))
            continue;
        const Vector3 sz = memory->read<Vector3>(prim + Offsets::Primitive::Size);
        if (!(sz.x > 0.05f && sz.x < 200.f && sz.y > 0.05f && sz.y < 200.f && sz.z > 0.05f && sz.z < 200.f))
            continue;
        if (!std::isfinite(rp[9]) || !std::isfinite(rp[10]) || !std::isfinite(rp[11]))
            continue;
        Cheat::Visuals::MeshChams::FallbackBox b;
        b.pos = Vector3{ rp[9], rp[10], rp[11] };
        const float dx = b.pos.x - camera.x;
        const float dy = b.pos.y - camera.y;
        const float dz = b.pos.z - camera.z;
        if (dx * dx + dy * dy + dz * dz > 5000.f * 5000.f)
            continue;
        b.rot = Matrix4x4{ {
            { rp[0], rp[1], rp[2], 0.f },
            { rp[3], rp[4], rp[5], 0.f },
            { rp[6], rp[7], rp[8], 0.f },
            { 0.f, 0.f, 0.f, 1.f } } };
        b.sz = sz;
        out.push_back(b);
    }
}

static bool project_pt(const Matrix4x4& view, const Vector2& vp, const Vector3& p, ImVec2& out) {
    float w = p.x * view.m[3][0] + p.y * view.m[3][1] + p.z * view.m[3][2] + view.m[3][3];
    if (w < 0.01f)
        return false;
    float x = p.x * view.m[0][0] + p.y * view.m[0][1] + p.z * view.m[0][2] + view.m[0][3];
    float y = p.x * view.m[1][0] + p.y * view.m[1][1] + p.z * view.m[1][2] + view.m[1][3];
    float inv = 1.f / w;
    const math::vector2 org = NaziViewportOrigin();
    out.x = (vp.x * 0.5f + (x * inv) * vp.x * 0.5f) + org.x;
    out.y = (vp.y * 0.5f - (y * inv) * vp.y * 0.5f) + org.y;
    return true;
}

void draw_box_edges(ImDrawList* dl, const BoxList& boxes, const Matrix4x4& view, const Vector2& vp, ImU32 col) {
    if (!dl || boxes.empty())
        return;
    static const int kEdges[12][2] = {
        { 0, 1 }, { 0, 2 }, { 0, 4 }, { 1, 3 }, { 1, 5 }, { 2, 3 },
        { 2, 6 }, { 3, 7 }, { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 }
    };
    for (const auto& b : boxes) {
        const float hx = b.sz.x * 0.5f;
        const float hy = b.sz.y * 0.5f;
        const float hz = b.sz.z * 0.5f;
        ImVec2 c[8];
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            const float lx = (i & 1) ? hx : -hx;
            const float ly = (i & 2) ? hy : -hy;
            const float lz = (i & 4) ? hz : -hz;
            const Vector3 wc{
                b.pos.x + b.rot.m[0][0] * lx + b.rot.m[0][1] * ly + b.rot.m[0][2] * lz,
                b.pos.y + b.rot.m[1][0] * lx + b.rot.m[1][1] * ly + b.rot.m[1][2] * lz,
                b.pos.z + b.rot.m[2][0] * lx + b.rot.m[2][1] * ly + b.rot.m[2][2] * lz,
            };
            if (!project_pt(view, vp, wc, c[i])) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;
        for (const auto& e : kEdges)
            dl->AddLine(c[e[0]], c[e[1]], col, 1.0f);
    }
}

}

void ensure_init(ID3D11Device* device, ID3D11DeviceContext* ctx) {
    if (!device || !ctx)
        return;
    static ID3D11Device* s_dev = nullptr;
    if (s_dev == device)
        return;

    Cheat::Visuals::MeshDxShader::Shutdown();
    if (Cheat::Visuals::MeshDxShader::Init(device, ctx))
        s_dev = device;
    else
        s_dev = nullptr;
}

void on_resize(unsigned width, unsigned height) {
    Cheat::Visuals::MeshDxShader::Resize(width, height);
}

void shutdown() {
    Cheat::Visuals::MeshDxShader::Shutdown();
}

static bool is_model_container(std::uint64_t addr) {
    if (!addr)
        return false;
    const std::string c = rbx::c_nameable(addr).get_class_name();
    return c == "Model" || c == "Folder";
}

std::uint64_t character_address(const cache::entity_t& e) {
    const std::uint64_t inst = e.instance.address;
    if (!inst)
        return 0;
    if (is_model_container(inst))
        return inst;

    const std::uint64_t refs[] = {
        e.humanoid_root_part.address,
        e.head_part.address,
        e.humanoid.address,
    };
    std::uint64_t start = 0;
    for (std::uint64_t r : refs) {
        if (r) { start = r; break; }
    }
    if (!start && !e.parts.empty()) {
        for (const auto& kv : e.parts) {
            if (kv.second.address) { start = kv.second.address; break; }
        }
    }
    if (!start)
        return 0;

    std::uint64_t cur = memory->read<std::uint64_t>(start + Offsets::Instance::Parent);
    std::uint64_t nearest = 0;
    std::uint64_t named = 0;
    for (int i = 0; i < 5 && cur >= 0x10000; ++i) {
        if (!is_model_container(cur))
            break;
        if (!nearest)
            nearest = cur;
        if (!named) {
            const std::string n = rbx::c_nameable(cur).get_name();
            if ((!e.name.empty() && n == e.name) ||
                (!e.display_name.empty() && n == e.display_name))
                named = cur;
        }
        cur = memory->read<std::uint64_t>(cur + Offsets::Instance::Parent);
    }
    return named ? named : nearest;
}

void render_buckets(
    ImDrawList* dl,
    ID3D11Device* device,
    ID3D11DeviceContext* ctx,
    ID3D11RenderTargetView* rtv,
    const Matrix4x4& view,
    const Vector3& camera,
    const Vector2& viewport,
    const EntityBucket* buckets,
    std::size_t bucket_count) {
    if (!buckets || !bucket_count || !device || !ctx || !rtv)
        return;
    if (viewport.x <= 0.0f || viewport.y <= 0.0f)
        return;

    struct CharSrc {
        std::uint64_t character{ 0 };
        const cache::entity_t* ent{ nullptr };
    };
    struct Resolved {
        std::vector<CharSrc> items;
        float fill[4];
        float line[4];
    };
    Resolved groups[8];
    std::size_t group_count = 0;
    std::size_t total_chars = 0;

    for (std::size_t b = 0; b < bucket_count; ++b) {
        if (!buckets[b].entities || buckets[b].entities->empty() || !buckets[b].fill)
            continue;
        float fill[4];
        std::memcpy(fill, buckets[b].fill, sizeof(fill));
        if (fill[3] == 0.25f)
            fill[3] = 1.0f;

        float line[4];
        if (buckets[b].line)
            std::memcpy(line, buckets[b].line, sizeof(line));
        else
            std::memcpy(line, fill, sizeof(line));

        std::size_t g = group_count;
        for (std::size_t k = 0; k < group_count; ++k) {
            if (std::memcmp(groups[k].fill, fill, sizeof(fill)) == 0 &&
                std::memcmp(groups[k].line, line, sizeof(line)) == 0) {
                g = k;
                break;
            }
        }
        if (g == group_count) {
            if (group_count >= 8)
                g = 7;
            else {
                std::memcpy(groups[group_count].fill, fill, sizeof(fill));
                std::memcpy(groups[group_count].line, line, sizeof(line));
                g = group_count++;
            }
        }
        for (const auto* e : *buckets[b].entities) {
            if (!e)
                continue;
            const std::uint64_t ch = character_address(*e);
            if (ch) {
                groups[g].items.push_back({ ch, e });
                ++total_chars;
            }
        }
    }
    if (!total_chars)
        return;

    Cheat::Visuals::MeshCache::Get().Refresh(false);

    const float time = (float)ImGui::GetTime();

    const bool use_shader =
        settings::visuals::mesh_chams_style == 1 ||
        settings::visuals::mesh_chams_occlusion ||
        settings::visuals::mesh_chams_outline;
    if (!use_shader) {
        for (std::size_t g = 0; g < group_count; ++g) {
            if (groups[g].items.empty())
                continue;
            const float* f = groups[g].fill;
            const float* li = groups[g].line;
            const ImU32 fill_col = IM_COL32(
                (int)(f[0] * 255.0f), (int)(f[1] * 255.0f),
                (int)(f[2] * 255.0f), (int)(f[3] * 255.0f));
            const ImU32 line_col = IM_COL32(
                (int)(li[0] * 255.0f), (int)(li[1] * 255.0f),
                (int)(li[2] * 255.0f), (int)(li[3] * 255.0f));
            for (const auto& it : groups[g].items) {
                Cheat::Visuals::MeshChams::Draw(dl, it.character, view, viewport, 1.0f, 1.0f, fill_col);
                BoxList boxes;
                Cheat::Visuals::MeshChams::GetFallbackBoxes(it.character, boxes);
                draw_box_edges(dl, boxes, view, viewport, line_col);
            }
        }
        return;
    }

    ensure_init(device, ctx);
    on_resize((unsigned)viewport.x, (unsigned)viewport.y);

    float prev_fill[4];
    std::memcpy(prev_fill, settings::visuals::chams_fill_color, sizeof(prev_fill));

    for (std::size_t g = 0; g < group_count; ++g) {
        if (groups[g].items.empty())
            continue;
        std::memcpy(settings::visuals::chams_fill_color, groups[g].fill,
            sizeof(settings::visuals::chams_fill_color));

        Cheat::Visuals::MeshDxShader::BeginFrame(view, camera, time);

        const float* f = groups[g].fill;
        const float* li = groups[g].line;
        const ImU32 fill_col = IM_COL32(
            (int)(f[0] * 255.0f), (int)(f[1] * 255.0f),
            (int)(f[2] * 255.0f), (int)(f[3] * 255.0f));
        const ImU32 line_col = IM_COL32(
            (int)(li[0] * 255.0f), (int)(li[1] * 255.0f),
            (int)(li[2] * 255.0f), (int)(li[3] * 255.0f));
        for (const auto& it : groups[g].items) {
            Cheat::Visuals::MeshChams::Draw(dl, it.character, view, viewport, 1.0f, 1.0f, fill_col);
            BoxList boxes;
            if (it.ent && Cheat::Visuals::MeshChams::BakedPartCount(it.character) == 0) {
                collect_scattered_boxes(it.ent, camera, boxes);
                for (const auto& b : boxes)
                    Cheat::Visuals::MeshDxShader::QueueBox(make_box_world(b.pos, b.rot, b.sz));
            }
            else {
                Cheat::Visuals::MeshChams::GetFallbackBoxes(it.character, boxes);
            }
            draw_box_edges(dl, boxes, view, viewport, line_col);
        }

        Cheat::Visuals::MeshDxShader::Flush(rtv);
    }

    std::memcpy(settings::visuals::chams_fill_color, prev_fill, sizeof(prev_fill));
}

}
