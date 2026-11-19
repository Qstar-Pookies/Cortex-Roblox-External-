#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "native.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <game/cache/core/cache.h>
#include <game/game/game.h>
#include <game/math/math.h>
#include <game/offsets/fflags.h>
#include <game/offsets/offsets.h>
#include <game/offsets/rva.h>
#include <game/sdk.h>
#include <hacks/system/playerlist/playerlist.h>
#include <hacks/system/settings/settings.h>
#include <hacks/system/teamcheck/teamcheck.h>
#include <hacks/visuals/chams/memory/NaziBridge.h>
#include <hacks/visuals/chams/memory/chams_main.h>
#include <hacks/visuals/skyshader/skyshader.h>
#include <system/logger/logger.h>

#ifndef CFG_CALL_TARGET_VALID
#define CFG_CALL_TARGET_VALID 0x00000001
#endif

#pragma comment(lib, "d3dcompiler.lib")

namespace nativechams {
namespace {

constexpr int k_slot_create_buf = 3;
constexpr int k_slot_create_il = 11;
constexpr int k_slot_create_vs = 12;
constexpr int k_slot_create_ps = 15;
constexpr int k_slot_create_blend = 20;
constexpr int k_slot_create_dss = 21;
constexpr int k_slot_create_raster = 22;
constexpr int k_slot_vscb = 7;
constexpr int k_slot_psset = 9;
constexpr int k_slot_vsset = 11;
constexpr int k_slot_drawidx = 12;
constexpr int k_slot_pscb = 16;
constexpr int k_slot_ilset = 17;
constexpr int k_slot_vbset = 18;
constexpr int k_slot_ibset = 19;
constexpr int k_slot_topo = 24;
constexpr int k_slot_omset = 33;
constexpr int k_slot_blendset = 35;
constexpr int k_slot_dssset = 36;
constexpr int k_slot_rsset = 43;
constexpr int k_slot_update = 48;
constexpr int k_slot_psget = 74;
constexpr int k_slot_blendget = 91;
constexpr int k_slot_dssget = 92;
constexpr int k_slot_present = 8;
constexpr int k_slot_dc_draw = 28;
constexpr int k_vt_cap = 510;

constexpr std::uint32_t k_cmd_vs = 1;
constexpr std::uint32_t k_cmd_ps = 2;
constexpr std::uint32_t k_cmd_il = 3;
constexpr std::uint32_t k_cmd_buf = 4;
constexpr std::uint32_t k_cmd_blend = 5;
constexpr std::uint32_t k_cmd_raster = 6;
constexpr std::uint32_t k_cmd_dss = 7;

constexpr int k_max_targets = 512;
constexpr int k_max_ents = 256;
constexpr int k_ent_budget = 2048;
constexpr int k_harvest_max = 1024;

constexpr char k_hlsl[] = R"HLSL(
cbuffer Cham : register(b13)
{
    float4 color;
    float4 glow;
    float time;
    int mode;
    float depth_scale;
    float depth_bias;
};

struct Out
{
    float4 c : SV_TARGET;
    float z : SV_DEPTH;
};

float3 screen_normal(float4 pos)
{
    float2 g = float2(ddx(pos.w), ddy(pos.w)) / max(abs(pos.w), 1e-6) * 480.0;
    return normalize(float3(g, 1.0));
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float vnoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + float2(1.0, 0.0));
    float c = hash21(i + float2(0.0, 1.0));
    float d = hash21(i + float2(1.0, 1.0));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float fbm2(float2 p)
{
    float a = 0.0;
    float w = 0.5;
    [unroll] for (int k = 0; k < 4; ++k)
    {
        a += w * vnoise(p);
        p = p * 2.02 + 7.3;
        w *= 0.5;
    }
    return a;
}

float3 hue_rot(float3 c, float h)
{
    float3 k = float3(0.57735, 0.57735, 0.57735);
    return c * cos(h) + cross(k, c) * sin(h) + k * dot(k, c) * (1.0 - cos(h));
}

float3 lit(float3 fill, float3 N, float amb, float k)
{
    return fill * (amb + saturate(dot(N, normalize(float3(0.45, -0.70, 0.55)))) * k);
}

void add_glow(inout float3 c, inout float a, float mask)
{
    float w = saturate(mask) * glow.a;
    float t = w / (1.0 + w);
    c = lerp(c, glow.rgb, t);
    c += glow.rgb * t * t * 0.75;
    a = saturate(a + t * (1.0 - a) * 0.9);
}

Out ps_main(float4 pos : SV_POSITION)
{
    float3 fill = color.rgb;
    float a = color.a;
    float3 N = screen_normal(pos);
    float rim = saturate(1.0 - saturate(N.z));
    float rp = pow(abs(rim), 2.4);
    float edge = saturate(length(fwidth(N)) * 2.2);
    float2 uv = pos.xy;
    float3 c = fill;
    float gmask = rp * 0.55 + edge * 0.75;

    switch (mode)
    {
    case 0:
        gmask = rp * 0.55 + edge * 0.8;
        break;
    case 1:
        c = lit(fill, N, 0.32, 0.68);
        gmask = pow(abs(rim), 2.0) * 0.7 + edge * 0.55;
        break;
    case 2:
    {
        float2 hp = uv * float2(0.013, 0.010);
        float rise = time * 0.85;
        float n1 = fbm2(hp + float2(0.0, -rise));
        float n2 = fbm2(hp * 1.85 + n1 * 1.7 + float2(time * 0.07, -rise * 1.9));
        float flame = smoothstep(0.30, 0.72, n2);
        float lick = 0.5 + 0.5 * sin(uv.y * 0.05 + time * 3.4 + n1 * 7.5);
        float ember = step(0.84, vnoise(hp * 5.2 + float2(sin(time * 1.3) * 3.0, -rise * 3.6)));
        float hot = flame * (0.72 + lick * 0.58);
        float3 fire = lerp(float3(1.00, 0.20, 0.02), float3(1.00, 0.88, 0.34),
                           saturate(flame * 1.15));
        c = lerp(fill * (0.10 + hot * 0.45), fire * (0.50 + hot * 0.95), saturate(flame * 0.95));
        c += fire * ember * 0.55;
        gmask = saturate(hot * 1.10 + ember * 0.60 + rp * 0.40);
        a = saturate(a * 0.90 + flame * 0.12);
    }
        break;
    case 3:
    {
        float2 lp = uv * 0.015;
        float n = fbm2(lp + float2(time * 0.05, -time * 0.03));
        float ridge = 1.0 - abs(fbm2(lp * 1.6 + n * 1.4 + time * 0.04) * 2.0 - 1.0);
        float crack = pow(saturate(ridge), 9.0);
        float crust = 0.20 + 0.42 * n;
        c = fill * crust;
        float3 mol = lerp(float3(0.85, 0.16, 0.02), float3(1.00, 0.86, 0.28), crack * 0.85);
        c += mol * crack * (0.85 + 0.30 * sin(time * 2.2 + n * 9.0));
        gmask = saturate(crack * 1.35 + rp * 0.35 + edge * 0.45);
        a = saturate(a * 0.85 + crack * 0.15);
    }
        break;
    case 4:
    {
        float2 pp = uv * 0.011;
        float s = fbm2(pp + float2(time * 0.24, -time * 0.17));
        float bands = 0.5 + 0.5 * sin(s * 9.0 + time * 2.4);
        float3 pc = hue_rot(float3(0.30, 0.78, 1.00), s * 6.0 + time * 1.1);
        pc = lerp(pc, fill, 0.22);
        c = pc * (0.22 + bands * 0.85) + pc * bands * bands * 0.40;
        a = saturate(a * (0.50 + bands * 0.55));
        gmask = saturate(bands * 0.80 + rp * 0.48 + edge * 0.40);
    }
        break;
    case 5:
    {
        float seed = floor(time * 4.0);
        float2 ep = uv * 0.020;
        float a1 = fbm2(ep + float2(time * 1.7, hash21(float2(seed, 1.0)) * 9.0));
        float a2 = fbm2(ep * 2.4 - float2(time * 1.1, hash21(float2(seed, 7.0)) * 9.0));
        float ridge = saturate(1.0 - abs(a1 - a2) * 2.5);
        float arc = pow(ridge, 10.0);
        float flick = 0.55 + 0.45 * sin(time * 23.0 + a2 * 11.0);
        c = fill * (0.12 + arc * 1.10) + float3(0.55, 0.85, 1.00) * arc * flick;
        gmask = saturate(arc * 1.45 + rp * 0.32 + edge * 0.55);
        a = saturate(a * 0.78 + arc * 0.30);
    }
        break;
    case 6:
    {
        float2 ap = uv * float2(0.0075, 0.0055);
        float w = fbm2(ap + float2(time * 0.13, time * 0.04));
        float curtain = 0.5 + 0.5 * sin(uv.y * 0.016 + w * 10.0 + time * 1.5);
        float band = pow(saturate(curtain), 2.2);
        float3 ac = lerp(float3(0.20, 0.95, 0.82), float3(0.44, 0.34, 1.00),
                         0.5 + 0.5 * sin(w * 6.5 + time * 0.8));
        c = lerp(fill * 0.45, ac, 0.75) * (0.14 + band * 1.05);
        a = saturate(a * (0.32 + band * 0.68));
        gmask = saturate(band * 0.95 + edge * 0.45 + rp * 0.30);
    }
        break;
    case 7:
    {
        float rings = 0.5 + 0.5 * sin(rim * 16.0 - time * 6.5);
        float ring = pow(saturate(rings), 4.0);
        float beat = 0.5 + 0.5 * sin(time * 3.4);
        c = fill * (0.18 + ring * (0.45 + beat * 0.55));
        a = saturate(a * (0.62 + ring * 0.38));
        gmask = saturate(ring * 1.05 + edge * 0.50 + rp * 0.38);
    }
        break;
    case 8:
    {
        float band = floor((uv.y + time * 90.0) * 0.035);
        float tear = step(0.86, hash21(float2(band, floor(time * 12.0))));
        float split = 0.35 + tear * 2.4;
        float scan = 0.87 + 0.13 * sin(uv.y * 1.7 - time * 16.0);
        float3 gc;
        gc.r = fill.r * (0.68 + 0.45 * sin(uv.y * 0.9 + uv.x * 0.45 + time * 21.0));
        gc.g = fill.g * (0.68 + 0.45 * sin(uv.y * 0.9 + uv.x * 0.45 + time * 21.0 + split * 0.35));
        gc.b = fill.b * (0.68 + 0.45 * sin(uv.y * 1.2 - uv.x * 0.30 - time * 15.0 + split * 0.55));
        c = gc * (0.62 + tear * 0.85) * scan;
        a = saturate(a * (0.78 + tear * 0.22));
        gmask = saturate(tear * 1.05 + edge * 0.70 + rp * 0.32);
    }
        break;
    case 9:
    {
        float2 wp = uv * 0.009;
        float s1 = fbm2(wp + float2(time * 0.10, -time * 0.24));
        float s2 = fbm2(wp * 2.1 + s1 * 1.9 + float2(-time * 0.15, time * 0.07));
        float wisp = smoothstep(0.34, 0.80, s2);
        c = fill * (0.08 + wisp * 0.38) + fill * pow(wisp, 3.0) * 0.55;
        a = saturate(a * (0.22 + wisp * 0.88));
        gmask = saturate(wisp * 0.90 + rp * 0.50 + edge * 0.50);
    }
        break;
    case 10:
    {
        float seed = floor(time * 6.0);
        float ph = frac(time * 6.0);
        float env = saturate(1.6 - ph * 2.4);
        env *= env;
        float2 bp = uv * (1.0 / 1080.0);
        float bolt = 0.0;
        float halo = 0.0;
        [unroll] for (int i = 0; i < 3; ++i)
        {
            float fi = (float)i;
            float h1 = hash21(float2(seed, 1.7 + fi * 3.1));
            float h2 = hash21(float2(seed, 4.3 + fi * 2.7));
            float cx = 0.25 + h1 * 1.65;
            float wob = sin(bp.y * 2.4 + h2 * 6.2831 + fi * 2.0) * 0.55
                      + sin(bp.y * 5.3 - h1 * 4.0 + fi) * 0.20;
            float d = abs(bp.x - cx - wob) * (240.0 + fi * 70.0);
            float core = saturate(1.0 - d);
            core *= core;
            core *= core;
            bolt += core * (0.95 - fi * 0.25);
            halo += saturate(1.0 - d * 0.30) * (0.40 - fi * 0.09);
        }
        float s = saturate(bolt * env);
        float h = saturate(halo * env);
        c = fill * (0.14 + s * 0.30);
        c += float3(0.78, 0.90, 1.00) * s * 1.70;
        c += float3(0.35, 0.55, 1.00) * h * 0.55;
        a = saturate(a * 0.82 + s * 0.35);
        gmask = saturate(s * 1.50 + h * 0.60 + rp * 0.30 + edge * 0.60);
    }
        break;
    default:
        gmask = rp * 0.55 + edge * 0.8;
        break;
    }

    add_glow(c, a, gmask);

    const float3 backdrop = float3(0.66, 0.68, 0.72);
    Out o;
    o.c = float4(saturate(lerp(backdrop, c, saturate(a))), 1.0);
    float dz = pos.z;
    if (depth_scale > 1.0001)
        dz = (depth_bias > 0.0) ? (1.0 - (1.0 - dz) / depth_scale)
                                : (dz / depth_scale);
    o.z = saturate(dz + depth_bias);
    return o;
}
)HLSL";

#pragma pack(push, 8)
struct FrameCB {
	float vp[16];
	float cam[3];
	float time;
	int mode;
	float pad[3];
};
static_assert(sizeof(FrameCB) == 96);

struct RemoteState {
	std::uint32_t enabled;
	std::uint32_t cmd;
	std::uint32_t ready;
	std::uint32_t ngeom;
	std::uint64_t device;
	std::uint64_t ctx;
	std::uint64_t orig_present;
	std::uint64_t orig_draw;
	std::uint64_t vs;
	std::uint64_t ps;
	std::uint64_t il;
	std::uint64_t cb;
	std::uint64_t cb1;
	std::uint64_t blend;
	std::uint64_t raster;
	std::uint64_t dss;
	std::uint64_t last_rtv;
	std::uint64_t last_dsv;
	std::uint64_t bytecode;
	std::uint64_t bytecode_len;
	std::uint64_t out_res;
	std::uint64_t init_sys;
	std::uint64_t geoms;
	std::uint32_t entered;
	std::uint32_t draws;
	std::uint32_t creates;
	std::uint32_t busy;
	std::uint32_t stage;
	std::uint32_t il_count;
	FrameCB frame;
	std::uint64_t fn_create_vs;
	std::uint64_t fn_create_ps;
	std::uint64_t fn_create_il;
	std::uint64_t fn_create_buf;
	std::uint64_t fn_create_blend;
	std::uint64_t fn_create_raster;
	std::uint64_t fn_create_dss;
	std::uint64_t fn_vsset;
	std::uint64_t fn_psset;
	std::uint64_t fn_ilset;
	std::uint64_t fn_vbset;
	std::uint64_t fn_ibset;
	std::uint64_t fn_topo;
	std::uint64_t fn_omset;
	std::uint64_t fn_omget;
	std::uint64_t fn_blendset;
	std::uint64_t fn_dssset;
	std::uint64_t fn_rsset;
	std::uint64_t fn_vscb;
	std::uint64_t fn_pscb;
	std::uint64_t fn_update;
	std::uint64_t fn_drawidx;
	std::uint64_t dc;
	std::uint64_t fn_psget;
	std::uint64_t fn_dssget;
	std::uint64_t fn_blendget;
	std::uint32_t hits;
};
#pragma pack(pop)

static_assert(offsetof(RemoteState, cmd) == 0x04);
static_assert(offsetof(RemoteState, device) == 0x10);
static_assert(offsetof(RemoteState, orig_present) == 0x20);
static_assert(offsetof(RemoteState, orig_draw) == 0x28);
static_assert(offsetof(RemoteState, vs) == 0x30);
static_assert(offsetof(RemoteState, last_rtv) == 0x70);
static_assert(offsetof(RemoteState, bytecode) == 0x80);
static_assert(offsetof(RemoteState, geoms) == 0xA0);
static_assert(offsetof(RemoteState, entered) == 0xA8);
static_assert(offsetof(RemoteState, busy) == 0xB4);
static_assert(offsetof(RemoteState, frame) == 0xC0);
static_assert(offsetof(RemoteState, fn_create_vs) == 0x120);
static_assert(offsetof(RemoteState, fn_omget) == 0x190);
static_assert(offsetof(RemoteState, fn_vscb) == 0x1B0);
static_assert(offsetof(RemoteState, fn_pscb) == 0x1B8);
static_assert(offsetof(RemoteState, fn_update) == 0x1C0);
static_assert(offsetof(RemoteState, fn_drawidx) == 0x1C8);
static_assert(offsetof(RemoteState, dc) == 0x1D0);
static_assert(offsetof(RemoteState, fn_psget) == 0x1D8);
static_assert(offsetof(RemoteState, fn_dssget) == 0x1E0);
static_assert(offsetof(RemoteState, fn_blendget) == 0x1E8);
static_assert(offsetof(RemoteState, hits) == 0x1F0);
static_assert(sizeof(RemoteState) == 0x1F8);

struct Patch {
	std::uintptr_t at = 0;
	std::uint64_t old = 0;
};

struct Target {
	std::uint64_t geom;
	float color[4];
	float glow[4];
	float time;
	int mode;
	float depth_scale;
	float depth_bias;
};
static_assert(sizeof(Target) == 56);

struct Hook {
	std::uintptr_t state = 0;
	std::uintptr_t geoms = 0;
	std::uintptr_t cave = 0;
	std::size_t cave_n = 0;
	std::uintptr_t present_thunk = 0;
	std::uintptr_t create_stub = 0;
	std::uintptr_t draw_thunk = 0;
	std::uintptr_t device = 0;
	std::uintptr_t ctx = 0;
	std::uintptr_t dc = 0;
	std::uintptr_t dc_vt = 0;
	std::uintptr_t swap = 0;
	std::uintptr_t swap_vt = 0;
	std::uintptr_t vt_mem = 0;
	std::uintptr_t orig_present = 0;
	std::uintptr_t orig_draw = 0;
	std::uintptr_t module = 0;
	std::uintptr_t ps_dxbc = 0;
	std::size_t ps_len = 0;
	Patch patches[16]{};
	int npatch = 0;
	int stage = 0;
	int ntarget = 0;
	int slot = 0;
	int nnode = 0;
	int nent = 0;
	int ndrop = 0;
	int nbind = 0;
	int nrec = 0;
	int cl_begin = 0;
	int cl_end = 0;
	int nswvt = 0;
	int ndcvt = 0;
	bool hooked = false;
	bool depth_rev = false;
	bool cmd_wait = false;
	bool logged = false;
	bool walls = false;
	bool occl_off = false;

	struct OcclFlag
	{
		std::uintptr_t addr = 0;
		std::uint8_t orig = 0;
	};
	OcclFlag occl[4]{};
	int noccl = 0;
};

Hook g{};
auto g_fail = std::chrono::steady_clock::time_point{};
const char* g_why = nullptr;
int g_nunk = 0;
int g_nfc = 0;
int g_npick = 0;
int g_nnopart = 0;
int g_nnochar = 0;
int g_npartrefs = 0;
int g_ngeorej = 0;
int g_nself = 0;

int g_nworld = 0;
int g_lid = 0;
int g_npl = 0;

int g_cls[3] = { 0, 0, 0 };

int g_on = 0;
std::uint64_t g_unk_vt = 0;

struct CachedPart {
	std::uint64_t cluster = 0;
	std::vector<std::uint64_t> geoms;
	ULONGLONG stamp = 0;
};
std::unordered_map<std::uint64_t, CachedPart> g_part_cache;

struct EntGeom {
	std::vector<std::uint64_t> geoms;
	ULONGLONG stamp = 0;
};
std::unordered_map<std::uint64_t, EntGeom> g_ent_cache;
constexpr ULONGLONG k_ent_expire_ms = 8000;
ULONGLONG g_last_target = 0;
constexpr ULONGLONG k_geom_refresh_ms = 150;
constexpr ULONGLONG k_geom_expire_ms = 8000;

constexpr ULONGLONG k_target_hold_ms = 60000;

std::mutex g_tick_mutex;
bool g_stopping = false;

bool addr_ok(std::uintptr_t a)
{
	return a >= 0x10000ull && a < 0x00007FFFFFFFFFFFull;
}

void append_u64(std::vector<std::uint8_t>& c, std::uint64_t v)
{
	const auto* b = (const std::uint8_t*)&v;
	c.insert(c.end(), b, b + 8);
}

void append_call_rax(std::vector<std::uint8_t>& c)
{
	c.insert(c.end(), { 0xFF, 0xD0 });
}

std::size_t append_jcc32(std::vector<std::uint8_t>& c, std::uint8_t cc)
{
	c.push_back(0x0F);
	c.push_back((std::uint8_t)(0x80 | cc));
	c.insert(c.end(), 4, 0);
	return c.size() - 4;
}

void patch_rel32(std::vector<std::uint8_t>& c, std::size_t at, std::size_t dest)
{
	const std::int32_t rel = (std::int32_t)((std::ptrdiff_t)dest - (std::ptrdiff_t)(at + 4));
	std::memcpy(c.data() + at, &rel, 4);
}

bool mark_cfg(std::uintptr_t t)
{
	static FARPROC proc = nullptr;
	if (!proc)
	{
		const char* mods[] = {
			"kernelbase.dll", "kernel32.dll",
			"api-ms-win-core-memory-l1-1-3.dll"
		};
		for (auto* m : mods)
		{
			HMODULE h = GetModuleHandleA(m);
			if (!h)
				h = LoadLibraryA(m);
			if (!h)
				continue;
			proc = GetProcAddress(h, "SetProcessValidCallTargets");
			if (proc)
				break;
		}
	}
	if (!proc || !g_Memory.GetHandle())
		return false;
	SYSTEM_INFO si{};
	GetSystemInfo(&si);
	const std::size_t page = si.dwPageSize ? (std::size_t)si.dwPageSize : 0x1000u;
	struct Info {
		ULONG_PTR Offset;
		ULONG Flags;
	} info{};
	info.Offset = t & (page - 1);
	info.Flags = CFG_CALL_TARGET_VALID;
	using Fn = BOOL(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, void*);
	return ((Fn)proc)(
		g_Memory.GetHandle(),
		(void*)(t & ~((std::uintptr_t)page - 1)),
		page, 1, &info) != 0;
}

bool write_exec(std::uintptr_t at, const void* data, std::size_t n)
{
	if (!addr_ok(at) || !data || !n)
		return false;
	DWORD old = 0;
	const bool changed = g_Memory.Protect(at, n, PAGE_EXECUTE_READWRITE, &old);
	const bool ok = g_Memory.WriteRaw(at, data, n) == n;
	if (changed)
		g_Memory.Protect(at, n, old, nullptr);
	return ok;
}

bool exec_prot(DWORD p)
{
	const DWORD b = p & 0xFF;
	return b == PAGE_EXECUTE || b == PAGE_EXECUTE_READ ||
		b == PAGE_EXECUTE_READWRITE || b == PAGE_EXECUTE_WRITECOPY;
}

std::uintptr_t find_dll_cave(std::size_t need, std::uintptr_t ignore)
{
	static const wchar_t* pref[] = {
		L"winsta.dll", L"win32u.dll", L"uxtheme.dll", L"dwmapi.dll",
		L"msctf.dll", L"TextInputFramework.dll", L"CoreMessaging.dll",
		L"user32.dll", L"imm32.dll", L"gdi32.dll", L"ole32.dll", L"combase.dll",
	};
	for (auto* name : pref)
	{
		const std::uintptr_t mb = g_Memory.GetModuleBase(name);
		if (!mb)
			continue;
		IMAGE_DOS_HEADER dos{};
		if (g_Memory.ReadRaw(mb, &dos, sizeof(dos)) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE)
			continue;
		IMAGE_NT_HEADERS64 nt{};
		if (g_Memory.ReadRaw(mb + (std::uintptr_t)dos.e_lfanew, &nt, sizeof(nt)) != sizeof(nt)
			|| nt.Signature != IMAGE_NT_SIGNATURE)
			continue;
		const std::size_t ms = nt.OptionalHeader.SizeOfImage;
		if (ms < 0x2000)
			continue;
		std::uintptr_t addr = mb + 0x1000;
		const std::uintptr_t to = mb + ms;
		MEMORY_BASIC_INFORMATION mbi{};
		while (addr < to &&
			VirtualQueryEx(g_Memory.GetHandle(), (void*)addr, &mbi, sizeof(mbi)))
		{
			const auto rb = (std::uintptr_t)mbi.BaseAddress;
			const auto rs = (std::size_t)mbi.RegionSize;
			const std::uintptr_t next = rb + rs;
			if (next <= addr)
				break;
			const bool usable = mbi.State == MEM_COMMIT
				&& !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
				&& exec_prot(mbi.Protect);
			if (usable && rs >= need)
			{
				std::vector<std::uint8_t> buf(rs);
				if (g_Memory.ReadRaw(rb, buf.data(), rs) == rs)
				{
					std::size_t run = 0;
					for (std::size_t i = 0; i < rs; ++i)
					{
						if (buf[i] == 0x00 || buf[i] == 0xCC)
							++run;
						else
							run = 0;
						if (run >= need)
						{
							const std::uintptr_t start = rb + i + 1 - run;
							const std::uintptr_t aligned = (start + 0x0F) & ~std::uintptr_t(0x0F);
							if (ignore && aligned == ignore)
								continue;
							if (aligned + need > rb + rs)
								continue;
							auto hit = [&](std::uintptr_t p, std::size_t n)
							{
								return p && aligned < p + n && p < aligned + need;
							};
							if (hit(g.cave, g.cave_n))
								continue;
							if (Cheat::Features::SkyShader::CaveBusy(aligned, need))
								continue;
							return aligned;
						}
					}
				}
			}
			addr = next;
		}
	}
	return 0;
}

void fail(const char* why)
{
	if (g_why == why)
		return;
	g_why = why;
	Cheat::Console::Log(Cheat::Console::Color::Red, "nativechams: %s", why);
}

std::uintptr_t module_base()
{
	const std::uintptr_t cached = (std::uintptr_t)(memory ? memory->get_base() : 0);
	if (cached)
		return cached;
	return g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
}

std::uintptr_t visual_engine()
{
	const std::uintptr_t base = module_base();
	if (!base)
		return 0;
	const std::uint64_t ve = g_Memory.Read<std::uint64_t>(base + Offsets::VisualEngine::Pointer);
	return g_Memory.IsValid(ve) ? (std::uintptr_t)ve : 0;
}

std::uintptr_t render_view()
{
	const std::uintptr_t ve = visual_engine();
	if (!ve)
		return 0;
	const std::uint64_t rv = g_Memory.Read<std::uint64_t>(ve + Offsets::VisualEngine::RenderView);
	return g_Memory.IsValid(rv) ? (std::uintptr_t)rv : 0;
}

bool want_reverse()
{
	const std::uintptr_t ve = visual_engine();
	if (!ve)
		return g.depth_rev;
	float m[16]{};
	if (g_Memory.ReadRaw(ve + Offsets::VisualEngine::ViewMatrix, m, sizeof(m)) != sizeof(m))
		return g.depth_rev;
	const float zl = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
	const float wl = std::sqrt(m[12] * m[12] + m[13] * m[13] + m[14] * m[14]);
	if (wl < 1e-4f)
		return g.depth_rev;
	return zl < wl * 0.5f;
}

bool find_d3d()
{
	if (g.device && g_Memory.IsValid(g.device) && g.ctx && g_Memory.IsValid(g.ctx) &&
		g.swap && g_Memory.IsValid(g.swap))
		return true;
	const std::uintptr_t base = module_base();
	const std::uintptr_t rv = render_view();
	if (!base || !rv)
		return false;
	const std::uintptr_t wrap = (std::uintptr_t)g_Memory.Read<std::uint64_t>(rv + Offsets::RenderView::DeviceD3D11);
	if (!g_Memory.IsValid(wrap))
		return false;
	if (g_Memory.Read<std::uint64_t>(wrap) != base + Offsets::DeviceD3D11Gfx::VTableRva)
		return false;
	const std::uintptr_t dev = (std::uintptr_t)g_Memory.Read<std::uint64_t>(wrap + Offsets::DeviceD3D11Gfx::DevicePtr);
	const std::uintptr_t ctx = (std::uintptr_t)g_Memory.Read<std::uint64_t>(wrap + Offsets::DeviceD3D11Gfx::ContextPtr);
	const std::uintptr_t swap = (std::uintptr_t)g_Memory.Read<std::uint64_t>(wrap + Offsets::DeviceD3D11Gfx::SwapChainPtr);
	const std::uintptr_t dc = (std::uintptr_t)g_Memory.Read<std::uint64_t>(wrap + Offsets::DeviceD3D11Gfx::ContextObj);
	if (!addr_ok(dev) || !addr_ok(ctx) || !addr_ok(swap) || !addr_ok(dc))
		return false;
	if (g_Memory.Read<std::uint64_t>(dc + 0x18) != ctx)
		return false;
	g.device = dev;
	g.ctx = ctx;
	g.swap = swap;
	g.dc = dc;
	return true;
}
std::uintptr_t vt_fn(std::uintptr_t obj, int slot)
{
	const std::uintptr_t vt = (std::uintptr_t)g_Memory.Read<std::uint64_t>(obj);
	if (!addr_ok(vt))
		return 0;
	const std::uintptr_t fn = (std::uintptr_t)g_Memory.Read<std::uint64_t>(vt + (std::size_t)slot * 8);
	return addr_ok(fn) ? fn : 0;
}

bool compile(const char* entry, const char* model, std::vector<std::uint8_t>& out)
{
	ID3DBlob* blob = nullptr;
	ID3DBlob* err = nullptr;
	const HRESULT hr = D3DCompile(
		k_hlsl, sizeof(k_hlsl) - 1, nullptr, nullptr, nullptr,
		entry, model, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
	if (FAILED(hr) || !blob)
	{
		if (err)
		{
			Cheat::Console::Log(Cheat::Console::Color::Red, "nativechams compile: %s", (const char*)err->GetBufferPointer());
			err->Release();
		}
		return false;
	}
	if (err)
		err->Release();
	const auto* p = (const std::uint8_t*)blob->GetBufferPointer();
	out.assign(p, p + blob->GetBufferSize());
	blob->Release();
	return !out.empty();
}

std::vector<std::uint8_t> make_present_thunk(std::uintptr_t state, std::uintptr_t create)
{
	std::vector<std::uint8_t> c;
	c.insert(c.end(), { 0x51, 0x52, 0x41, 0x50, 0x41, 0x51 });
	c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x28 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xFF, 0x80, 0xA8, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x83, 0xB8, 0xB4, 0x00, 0x00, 0x00, 0x00 });
	const std::size_t j_busy = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0xC7, 0x80, 0xB4, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x83, 0x78, 0x04, 0x00 });
	const std::size_t j_nocmd = append_jcc32(c, 0x04);
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, create);
	append_call_rax(c);
	const std::size_t nocmd = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xC7, 0x80, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	const std::size_t out = c.size();
	c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x28 });
	c.insert(c.end(), { 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59 });
	c.insert(c.end(), { 0x49, 0xBA });
	append_u64(c, state);
	c.insert(c.end(), { 0x4D, 0x8B, 0x5A, 0x20 });
	c.insert(c.end(), { 0x41, 0xFF, 0xE3 });
	patch_rel32(c, j_busy, out);
	patch_rel32(c, j_nocmd, nocmd);
	return c;
}

std::vector<std::uint8_t> make_create_stub(std::uintptr_t state)
{
	std::vector<std::uint8_t> c;
	c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x48 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x8B, 0x48, 0x04 });
	c.insert(c.end(), { 0x85, 0xC9 });
	const std::size_t j_none = append_jcc32(c, 0x04);
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x83, 0xF9, 0x01 });
	const std::size_t j_nvs = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8B, 0x90, 0x80, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x80, 0x88, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4D, 0x31, 0xC9 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x90, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x89, 0x54, 0x24, 0x20 });
	c.insert(c.end(), { 0xFF, 0x90, 0x20, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin0 = c.size() - 4;
	const std::size_t nvs = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x02 });
	const std::size_t j_nps = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8B, 0x90, 0x80, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x80, 0x88, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4D, 0x31, 0xC9 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x90, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x89, 0x54, 0x24, 0x20 });
	c.insert(c.end(), { 0xFF, 0x90, 0x28, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin1 = c.size() - 4;
	const std::size_t nps = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x03 });
	const std::size_t j_nil = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x10, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x44, 0x8B, 0x80, 0xBC, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x88, 0x80, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0x8B, 0x80, 0x88, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0x89, 0x44, 0x24, 0x20 });
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x4C, 0x8D, 0x90, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x89, 0x54, 0x24, 0x28 });
	c.insert(c.end(), { 0xFF, 0x90, 0x30, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin2 = c.size() - 4;
	const std::size_t nil_ = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x04 });
	const std::size_t j_nbuf = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8B, 0x80, 0x98, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4D, 0x85, 0xC0 });
	c.insert(c.end(), { 0x74, 0x24 });
	c.insert(c.end(), { 0x4C, 0x89, 0x80, 0xD8, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0xE0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x48, 0xC7, 0x80, 0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0xD8, 0x03, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x88, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x38, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin3 = c.size() - 4;
	const std::size_t nbuf = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x05 });
	const std::size_t j_nblend = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x40, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin4 = c.size() - 4;
	const std::size_t nblend = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x06 });
	const std::size_t j_nrast = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x48, 0x01, 0x00, 0x00 });
	c.insert(c.end(), { 0xE9, 0, 0, 0, 0 });
	const std::size_t j_fin5 = c.size() - 4;
	const std::size_t nrast = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0x83, 0xF9, 0x07 });
	const std::size_t j_ndss = append_jcc32(c, 0x05);
	c.insert(c.end(), { 0x48, 0x8B, 0x48, 0x10 });
	c.insert(c.end(), { 0x48, 0x8D, 0x90, 0x00, 0x02, 0x00, 0x00 });
	c.insert(c.end(), { 0x4C, 0x8D, 0x80, 0x90, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x90, 0x50, 0x01, 0x00, 0x00 });
	const std::size_t fin = c.size();
	c.insert(c.end(), { 0x48, 0xB8 });
	append_u64(c, state);
	c.insert(c.end(), { 0xC7, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00 });
	c.insert(c.end(), { 0xFF, 0x80, 0xB0, 0x00, 0x00, 0x00 });
	const std::size_t none = c.size();
	c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x48 });
	c.push_back(0xC3);
	patch_rel32(c, j_none, none);
	patch_rel32(c, j_nvs, nvs);
	patch_rel32(c, j_nps, nps);
	patch_rel32(c, j_nil, nil_);
	patch_rel32(c, j_nbuf, nbuf);
	patch_rel32(c, j_nblend, nblend);
	patch_rel32(c, j_nrast, nrast);
	patch_rel32(c, j_ndss, fin);
	patch_rel32(c, j_fin0, fin);
	patch_rel32(c, j_fin1, fin);
	patch_rel32(c, j_fin2, fin);
	patch_rel32(c, j_fin3, fin);
	patch_rel32(c, j_fin4, fin);
	patch_rel32(c, j_fin5, fin);
	return c;
}

std::vector<std::uint8_t> make_draw_thunk(std::uintptr_t state)
{
	std::vector<std::uint8_t> c;
	auto bytes = [&](std::initializer_list<std::uint8_t> v) { c.insert(c.end(), v); };
	auto disp32 = [&](std::uint32_t v)
	{
		const auto* b = (const std::uint8_t*)&v;
		c.insert(c.end(), b, b + 4);
	};
	auto ld = [&](std::uint8_t modrm, std::uint32_t off)
	{
		bytes({ 0x49, 0x8B, modrm });
		disp32(off);
	};
	auto ld_rax = [&](std::uint32_t off) { ld(0x86, off); };
	auto ld_rdx = [&](std::uint32_t off) { ld(0x96, off); };
	auto ctx = [&] { bytes({ 0x49, 0x8B, 0x4E, 0x18 }); };
	auto call_state = [&](std::uint32_t fn)
	{
		ld_rax(fn);
		append_call_rax(c);
	};
	auto jmp32 = [&]
	{
		c.push_back(0xE9);
		c.insert(c.end(), 4, 0);
		return c.size() - 4;
	};

	bytes({ 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56 });
	bytes({ 0x48, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0x89, 0xCE });
	bytes({ 0x48, 0x89, 0xD3 });
	bytes({ 0x4D, 0x89, 0xC4 });
	bytes({ 0x4D, 0x89, 0xCD });
	bytes({ 0x49, 0xBE });
	append_u64(c, state);
	bytes({ 0x41, 0xFF, 0x86 });
	disp32(offsetof(RemoteState, hits));
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x40, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x48, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0xC7, 0x44, 0x24, 0x50, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x58, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x60, 0x00, 0x00, 0x00, 0x00 });

	std::vector<std::size_t> to_call;
	bytes({ 0x49, 0x83, 0x7E, 0x70, 0x00 });
	to_call.push_back(append_jcc32(c, 0x05));
	bytes({ 0x41, 0x83, 0x3E, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x41, 0x83, 0x7E, 0x08, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x49, 0x83, 0x7E, 0x38, 0x00 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x41, 0x8B, 0x4E, 0x0C });
	bytes({ 0x85, 0xC9 });
	to_call.push_back(append_jcc32(c, 0x04));
	ld_rdx(offsetof(RemoteState, geoms));
	bytes({ 0x48, 0x85, 0xD2 });
	to_call.push_back(append_jcc32(c, 0x04));
	bytes({ 0x31, 0xC0 });
	const std::size_t scan = c.size();
	bytes({ 0x48, 0x39, 0x1A });
	const std::size_t j_found = append_jcc32(c, 0x04);
	bytes({ 0x48, 0x83, 0xC2, 0x38 });
	bytes({ 0xFF, 0xC0 });
	bytes({ 0x39, 0xC8 });
	patch_rel32(c, append_jcc32(c, 0x02), scan);
	to_call.push_back(jmp32());

	patch_rel32(c, j_found, c.size());
	bytes({ 0x49, 0xC7, 0x46, 0x70, 0x01, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0x89, 0x54, 0x24, 0x58 });

	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x40 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psget));

	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x48 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x50 });
	call_state(offsetof(RemoteState, fn_dssget));

	ctx();
	bytes({ 0x48, 0x8D, 0x54, 0x24, 0x60 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x68 });
	bytes({ 0x4C, 0x8D, 0x4C, 0x24, 0x78 });
	call_state(offsetof(RemoteState, fn_blendget));

	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x48 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	bytes({ 0x48, 0x8B, 0x44, 0x24, 0x58 });
	bytes({ 0x48, 0x83, 0xC0, 0x08 });
	bytes({ 0x48, 0x89, 0x44, 0x24, 0x20 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00 });
	bytes({ 0x48, 0xC7, 0x44, 0x24, 0x30, 0x00, 0x00, 0x00, 0x00 });
	call_state(offsetof(RemoteState, fn_update));

	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x38 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psset));

	ctx();
	bytes({ 0xBA, 0x0D, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00 });
	bytes({ 0x4D, 0x8D, 0x4E, 0x48 });
	call_state(offsetof(RemoteState, fn_pscb));

	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x68 });
	bytes({ 0x45, 0x31, 0xC0 });
	call_state(offsetof(RemoteState, fn_dssset));

	for (std::uint8_t i = 0; i < 4; ++i)
	{
		bytes({ 0xC7, 0x84, 0x24 });
		disp32(0x80u + i * 4u);
		bytes({ 0x00, 0x00, 0x80, 0x3F });
	}
	ctx();
	bytes({ 0x49, 0x8B, 0x56, 0x58 });
	bytes({ 0x4C, 0x8D, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF });
	call_state(offsetof(RemoteState, fn_blendset));

	bytes({ 0x41, 0xFF, 0x86 });
	disp32(offsetof(RemoteState, draws));

	const std::size_t call_site = c.size();
	for (auto at : to_call)
		patch_rel32(c, at, call_site);

	for (std::uint8_t i = 0; i < 4; ++i)
	{
		bytes({ 0x48, 0x8B, 0x84, 0x24 });
		disp32(0xF0u + i * 8u);
		bytes({ 0x48, 0x89, 0x44, 0x24, (std::uint8_t)(0x20 + i * 8) });
	}
	bytes({ 0x48, 0x89, 0xF1 });
	bytes({ 0x48, 0x89, 0xDA });
	bytes({ 0x4D, 0x89, 0xE0 });
	bytes({ 0x4D, 0x89, 0xE9 });
	bytes({ 0x49, 0x8B, 0x46, 0x28 });
	append_call_rax(c);
	bytes({ 0x48, 0x89, 0xC7 });

	bytes({ 0x48, 0x83, 0x7C, 0x24, 0x58, 0x00 });
	const std::size_t j_out = append_jcc32(c, 0x04);

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x40 });
	bytes({ 0x45, 0x31, 0xC0, 0x45, 0x31, 0xC9 });
	call_state(offsetof(RemoteState, fn_psset));

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x48 });
	bytes({ 0x44, 0x8B, 0x44, 0x24, 0x50 });
	call_state(offsetof(RemoteState, fn_dssset));

	ctx();
	bytes({ 0x48, 0x8B, 0x54, 0x24, 0x60 });
	bytes({ 0x4C, 0x8D, 0x44, 0x24, 0x68 });
	bytes({ 0x44, 0x8B, 0x4C, 0x24, 0x78 });
	call_state(offsetof(RemoteState, fn_blendset));

	for (std::uint8_t off : { 0x40, 0x48, 0x60 })
	{
		bytes({ 0x48, 0x8B, 0x4C, 0x24, off });
		bytes({ 0x48, 0x85, 0xC9 });
		bytes({ 0x74, 0x06 });
		bytes({ 0x48, 0x8B, 0x01 });
		bytes({ 0xFF, 0x50, 0x10 });
	}

	bytes({ 0x49, 0xC7, 0x46, 0x70, 0x00, 0x00, 0x00, 0x00 });
	patch_rel32(c, j_out, c.size());
	bytes({ 0x48, 0x89, 0xF8 });
	bytes({ 0x48, 0x81, 0xC4, 0x98, 0x00, 0x00, 0x00 });
	bytes({ 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5B });
	c.push_back(0xC3);
	return c;
}

bool add_patch(std::uintptr_t at, std::uintptr_t thunk, std::uintptr_t* orig)
{
	if (!addr_ok(at) || !addr_ok(thunk) || g.npatch >= 16)
		return false;
	const std::uint64_t old = g_Memory.Read<std::uint64_t>(at);
	if (!old || old == thunk)
		return old == thunk;
	for (int i = 0; i < g.npatch; ++i)
	{
		if (g.patches[i].at == at)
			return true;
	}
	DWORD prot = 0;
	if (!g_Memory.Protect(at, 8, PAGE_READWRITE, &prot))
		return false;
	const bool ok = g_Memory.Write<std::uint64_t>(at, thunk);
	g_Memory.Protect(at, 8, prot, nullptr);
	if (!ok)
		return false;
	g.patches[g.npatch].at = at;
	g.patches[g.npatch].old = old;
	++g.npatch;
	if (orig && !*orig)
		*orig = (std::uintptr_t)old;
	return true;
}

void unpatch()
{
	for (int i = 0; i < g.npatch; ++i)
	{
		if (!g.patches[i].at)
			continue;
		DWORD prot = 0;
		if (g_Memory.Protect(g.patches[i].at, 8, PAGE_READWRITE, &prot))
		{
			g_Memory.Write<std::uint64_t>(g.patches[i].at, g.patches[i].old);
			g_Memory.Protect(g.patches[i].at, 8, prot, nullptr);
		}
		g.patches[i] = {};
	}
	g.npatch = 0;
}

int vt_len(std::uintptr_t vt)
{
	std::uint64_t v[k_vt_cap]{};
	if (g_Memory.ReadRaw(vt, v, sizeof(v)) != sizeof(v))
		return 0;
	int n = 0;
	for (; n < k_vt_cap; ++n)
	{
		const auto fn = (std::uintptr_t)v[n];
		if (!addr_ok(fn))
			break;
		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQueryEx(g_Memory.GetHandle(), (void*)fn, &mbi, sizeof(mbi))
			|| mbi.State != MEM_COMMIT || !exec_prot(mbi.Protect))
			break;
	}
	return n;
}

int clone_vt(std::uintptr_t src, std::uintptr_t dst, int slot, std::uintptr_t thunk, std::uintptr_t* orig)
{
	const int n = vt_len(src);
	if (n <= slot)
		return 0;
	const std::size_t bytes = (std::size_t)(n + 1) * 8;
	std::vector<std::uint64_t> v((std::size_t)n + 1);
	if (g_Memory.ReadRaw(src - 8, v.data(), bytes) != bytes)
		return 0;
	*orig = (std::uintptr_t)v[(std::size_t)slot + 1];
	if (!addr_ok(*orig))
		return 0;
	v[(std::size_t)slot + 1] = thunk;
	if (g_Memory.WriteRaw(dst - 8, v.data(), bytes) != bytes)
		return 0;
	return n;
}

bool patch_vtables()
{
	if (!g.present_thunk || !g.draw_thunk || !g.dc || !g.swap || !g.vt_mem)
		return false;
	const std::uintptr_t dc_vt = (std::uintptr_t)g_Memory.Read<std::uint64_t>(g.dc);
	const std::uintptr_t swap_vt = (std::uintptr_t)g_Memory.Read<std::uint64_t>(g.swap);
	if (!addr_ok(dc_vt) || !addr_ok(swap_vt) || dc_vt == g.dc_vt || swap_vt == g.swap_vt)
		return false;
	g.swap_vt = g.vt_mem + 8;
	g.dc_vt = g.vt_mem + 0x1000 + 8;
	g.nswvt = clone_vt(swap_vt, g.swap_vt, k_slot_present, g.present_thunk, &g.orig_present);
	g.ndcvt = clone_vt(dc_vt, g.dc_vt, k_slot_dc_draw, g.draw_thunk, &g.orig_draw);
	if (!g.nswvt || !g.ndcvt)
		return false;
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, orig_present), g.orig_present);
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, orig_draw), g.orig_draw);
	if (!add_patch(g.swap, g.swap_vt, nullptr))
		return false;
	return add_patch(g.dc, g.dc_vt, nullptr);
}

bool setup_meta()
{
	if (!g.device || !g.state || !g.ctx || !g.dc)
		return false;
	std::vector<std::uint8_t> ps;
	if (!compile("ps_main", "ps_5_0", ps))
		return false;
	if (g.ps_dxbc)
	{
		g_Memory.Free(g.ps_dxbc);
		g.ps_dxbc = 0;
	}
	g.ps_dxbc = g_Memory.Alloc(ps.size() + 16, PAGE_READWRITE);
	if (!g.ps_dxbc)
		return false;
	if (g_Memory.WriteRaw(g.ps_dxbc, ps.data(), ps.size()) != ps.size())
		return false;
	g.ps_len = ps.size();

	if (!g.geoms)
		g.geoms = g_Memory.Alloc((std::size_t)k_max_targets * sizeof(Target) * 2 + 16, PAGE_READWRITE);
	if (!g.geoms)
		return false;

	RemoteState st{};
	g_Memory.ReadRaw(g.state, &st, sizeof(st));
	st.device = g.device;
	st.ctx = g.ctx;
	st.dc = g.dc;
	st.geoms = 0;
	st.ngeom = 0;
	st.fn_create_ps = vt_fn(g.device, k_slot_create_ps);
	st.fn_create_buf = vt_fn(g.device, k_slot_create_buf);
	st.fn_create_dss = vt_fn(g.device, k_slot_create_dss);
	st.fn_psset = vt_fn(g.ctx, k_slot_psset);
	st.fn_psget = vt_fn(g.ctx, k_slot_psget);
	st.fn_pscb = vt_fn(g.ctx, k_slot_pscb);
	st.fn_dssset = vt_fn(g.ctx, k_slot_dssset);
	st.fn_dssget = vt_fn(g.ctx, k_slot_dssget);
	st.fn_blendset = vt_fn(g.ctx, k_slot_blendset);
	st.fn_blendget = vt_fn(g.ctx, k_slot_blendget);
	st.fn_create_blend = vt_fn(g.device, k_slot_create_blend);
	st.fn_update = vt_fn(g.ctx, k_slot_update);
	if (!addr_ok((std::uintptr_t)st.fn_create_ps) || !addr_ok((std::uintptr_t)st.fn_create_dss) ||
		!addr_ok((std::uintptr_t)st.fn_create_buf) || !addr_ok((std::uintptr_t)st.fn_psset) ||
		!addr_ok((std::uintptr_t)st.fn_psget) || !addr_ok((std::uintptr_t)st.fn_pscb) ||
		!addr_ok((std::uintptr_t)st.fn_dssset) || !addr_ok((std::uintptr_t)st.fn_dssget) ||
		!addr_ok((std::uintptr_t)st.fn_blendset) || !addr_ok((std::uintptr_t)st.fn_blendget) ||
		!addr_ok((std::uintptr_t)st.fn_create_blend) || !addr_ok((std::uintptr_t)st.fn_update))
		return false;
	st.busy = 0;
	st.cmd = 0;
	st.ready = 0;
	g_Memory.WriteRaw(g.state, &st, sizeof(st));
	return true;
}

void issue_cmd(std::uint32_t cmd, std::uintptr_t code, std::size_t len, std::uintptr_t init)
{
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, out_res), 0);
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, bytecode), code);
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, bytecode_len), len);
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, init_sys), init);
	g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, cmd), cmd);
	g.cmd_wait = true;
}

void issue_desc_cmd(std::uint32_t cmd, const void* desc, std::size_t n, std::uintptr_t init)
{
	g_Memory.WriteRaw(g.state + 0x200, desc, n);
	issue_cmd(cmd, 0, 0, init);
}

std::uint64_t take_out()
{
	const std::uint32_t cmd = g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, cmd));
	if (cmd)
		return 0;
	g.cmd_wait = false;
	return g_Memory.Read<std::uint64_t>(g.state + offsetof(RemoteState, out_res));
}

void store_res(std::uint64_t off, std::uint64_t v)
{
	g_Memory.Write<std::uint64_t>(g.state + off, v);
}

bool pump_creates()
{
	if (!g.state)
		return false;
	if (g.cmd_wait)
	{
		const std::uint64_t out = take_out();
		if (g.cmd_wait)
			return false;
		if (!out)
		{
			fail("create");
			g.stage = 0;
			return false;
		}
		switch (g.stage)
		{
		case 1: store_res(offsetof(RemoteState, ps), out); g.stage = 2; break;
		case 2:
			store_res(offsetof(RemoteState, dss), out);
			g.stage = 3;
			break;
		case 3: store_res(offsetof(RemoteState, cb), out); g.stage = 4; break;
		case 4:
			store_res(offsetof(RemoteState, blend), out);
			g.stage = 11;
			g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, ready), 1);
			break;
		default:
			break;
		}
	}
	if (g.cmd_wait)
		return g.stage >= 11;

	if (g.stage == 0)
	{
		g.stage = 1;
		issue_cmd(k_cmd_ps, g.ps_dxbc, g.ps_len, 0);
		return false;
	}
	if (g.stage == 2)
	{
		D3D11_DEPTH_STENCIL_DESC d{};

		d.DepthEnable = TRUE;
		d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		d.DepthFunc = g.depth_rev ? D3D11_COMPARISON_GREATER_EQUAL
			: D3D11_COMPARISON_LESS_EQUAL;
		d.StencilEnable = FALSE;
		issue_desc_cmd(k_cmd_dss, &d, sizeof(d), 0);
		return false;
	}
	if (g.stage == 3)
	{
		D3D11_BUFFER_DESC d{};
		d.ByteWidth = 48;
		d.Usage = D3D11_USAGE_DEFAULT;
		d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		issue_desc_cmd(k_cmd_buf, &d, sizeof(d), 0);
		return false;
	}
	if (g.stage == 4)
	{
		D3D11_BLEND_DESC d{};
		auto& rt = d.RenderTarget[0];

		rt.BlendEnable = FALSE;
		rt.SrcBlend = D3D11_BLEND_ONE;
		rt.DestBlend = D3D11_BLEND_ZERO;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		issue_desc_cmd(k_cmd_blend, &d, sizeof(d), 0);
		return false;
	}
	return g.stage >= 11;
}

static settings::visuals::priority_settings_t& class_cfg(int idx)
{
	if (idx == 2)
		return settings::visuals::neutral;
	if (idx == 1)
		return settings::visuals::friendly;
	return settings::visuals::hostile;
}

static bool is_target_native(int idx)
{
	const auto& cfg = class_cfg(idx);
	return cfg.chams && cfg.chams_type == 3;
}

static bool wants_native(int idx)
{
	return class_cfg(idx).native;
}

struct LocalId
{
	std::uint64_t player = 0;
	std::uint64_t model = 0;
	std::uint64_t user = 0;
	std::uint64_t hrp = 0;
};

static LocalId local_identity()
{
	LocalId id{};
	const auto ls = cache::get_local_snap();
	if (ls)
	{
		id.player = ls->instance.address;
		id.user = ls->user_id;
		id.hrp = ls->humanoid_root_part.address;

		id.model = nazichams::character_address(*ls);
	}
	if (!id.model)
		id.model = cache::local_character.address;
	if (!id.model && id.player)
	{
		rbx::c_player p(id.player);
		if (p.address)
			id.model = p.get_model_instance().address;
	}
	return id;
}

static bool is_local_entity(const cache::entity_t& e, const LocalId& id, std::uint64_t character)
{
	if (character && id.model && character == id.model)
		return true;
	if (id.model && e.instance.address == id.model)
		return true;
	if (id.player && e.instance.address == id.player)
		return true;
	if (id.user && e.user_id == id.user)
		return true;
	if (id.hrp && e.humanoid_root_part.address == id.hrp)
		return true;

	if (character && id.hrp)
	{
		std::uint64_t cur = g_Memory.Read<std::uint64_t>(id.hrp + Offsets::Instance::Parent);
		for (int i = 0; i < 3 && addr_ok((std::uintptr_t)cur); ++i)
		{
			if (cur == character)
				return true;
			cur = g_Memory.Read<std::uint64_t>(cur + Offsets::Instance::Parent);
		}
	}
	return false;
}

static int classify_entity(const cache::entity_t& e, std::uint64_t local_team,
	const LocalId& id, std::uint64_t character)
{
	if (is_local_entity(e, id, character))
		return 2;
	bool teammate = false;
	if (e.priority == cache::player_priority::friendly)
		teammate = true;
	else if (local_team && e.team && e.team == local_team &&
		game::datamodel && !game::datamodel->is_mm2() && !game::datamodel->is_arsenal())
		teammate = true;
	else if (!e.name.empty() &&
		playerlist::get_priority(e.name) == cache::player_priority::friendly)
		teammate = true;
	return teammate ? 1 : 0;
}

bool Active()
{
	for (int i = 0; i < 3; ++i)
		if (is_target_native(i) || wants_native(i))
			return true;
	return false;
}

int active_mask()
{
	int m = 0;
	for (int i = 0; i < 3; ++i)
		if (is_target_native(i) || wants_native(i))
			m |= 1 << i;
	return m;
}

bool is_vt(std::uint64_t obj, std::uint64_t rva)
{
	if (!addr_ok((std::uintptr_t)obj) || (obj & 7) || !g.module)
		return false;
	return g_Memory.Read<std::uint64_t>(obj) == g.module + rva;
}

bool is_cluster_vt(std::uint64_t vt)
{
	if (!g.module || !vt)
		return false;
	const std::uint64_t base = g.module;
	return vt == base + Offsets::FastCluster::VTableRva
		|| vt == base + Offsets::FastCluster::VTableRvaSub
		|| vt == base + Offsets::FastCluster::VTableRvaAlt
		|| vt == base + Offsets::FastCluster::VTableRvaSubAlt;
}

bool is_binding_vt(std::uint64_t vt)
{
	return g.module && vt == g.module + Offsets::FastClusterBinding::VTableRva;
}

bool is_entity_vt(std::uint64_t vt)
{
	if (!g.module || !vt)
		return false;
	const std::uint64_t base = g.module;
	return vt == base + Offsets::FastClusterEntity::VTableRva
		|| vt == base + Offsets::FastCluster::MeshEntityVTableRva
		|| vt == base + RVA::FastClusterEntity::VTableRva
		|| vt == base + RVA::Chams::Confirmed::RenderEntityVTable
		|| vt == base + RVA::Chams::Confirmed::FastClusterEntityVTable;
}

std::uint64_t g_geovt[4] = {};
int g_ngeom_known = 0;

void add_geom(std::uint64_t q, std::vector<std::uint64_t>& out)
{
	if (!addr_ok((std::uintptr_t)q) || (q & 7) || !g.module)
		return;
	const std::uint64_t vt = g_Memory.Read<std::uint64_t>(q);
	if (vt <= g.module || vt >= g.module + 0x20000000ull)
		return;
	if (vt == g.module + Offsets::GeometryD3D11::VTableRva)
		++g_ngeom_known;
	else
	{
		++g_ngeorej;
		const std::uint64_t rva = vt - g.module;
		bool seen_vt = false;
		for (int i = 0; i < 4; ++i)
			if (g_geovt[i] == rva)
				seen_vt = true;
		if (!seen_vt)
		{
			for (int i = 0; i < 4; ++i)
			{
				if (g_geovt[i])
					continue;
				g_geovt[i] = rva;
				Cheat::Console::Log(Cheat::Console::Color::Yellow,
					"nativechams geometry-vt +0x%llX accepted as a cham candidate (not GeometryD3D11)",
					(unsigned long long)rva);
				break;
			}
		}
	}
	if (std::find(out.begin(), out.end(), q) == out.end())
		out.push_back(q);
}

static std::uint64_t cluster_base(std::uint64_t obj)
{
	if (!addr_ok((std::uintptr_t)obj) || (obj & 7) || !g.module)
		return 0;
	const std::uint64_t vt = g_Memory.Read<std::uint64_t>(obj);
	const std::uint64_t base = g.module;
	if (vt == base + Offsets::FastCluster::VTableRva ||
		vt == base + Offsets::FastCluster::VTableRvaAlt)
		return obj;
	if (vt == base + Offsets::FastCluster::VTableRvaSub ||
		vt == base + Offsets::FastCluster::VTableRvaSubAlt)
	{
		const std::uint64_t owner = obj - Offsets::FastClusterBinding::Subobject;
		return addr_ok((std::uintptr_t)owner) ? owner : 0;
	}
	return 0;
}

std::uint64_t resolve_cluster_raw(std::uint64_t node)
{
	if (!addr_ok((std::uintptr_t)node) || (node & 7) || !g.module)
		return 0;
	if (const std::uint64_t base = cluster_base(node))
		return base;
	const std::uint64_t vt = g_Memory.Read<std::uint64_t>(node);
	if (is_binding_vt(vt))
	{
		++g.nbind;
		const std::uint64_t owner =
			g_Memory.Read<std::uint64_t>(node + Offsets::FastClusterBinding::Owner);
		if (const std::uint64_t base = cluster_base(owner))
			return base;
	}

	for (int off = 8; off <= 0x200; off += 8)
	{
		const std::uint64_t p = g_Memory.Read<std::uint64_t>(node + (std::uintptr_t)off);
		if (!addr_ok((std::uintptr_t)p) || (p & 7))
			continue;
		if (const std::uint64_t base = cluster_base(p))
		{
			++g.nrec;
			return base;
		}
	}
	return 0;
}

struct NodeHit
{
	std::uint64_t cluster = 0;
	ULONGLONG stamp = 0;
};
std::unordered_map<std::uint64_t, NodeHit> g_node_cache;
std::unordered_map<std::uint64_t, NodeHit> g_part_cluster_cache;
constexpr ULONGLONG k_node_ttl_ms = 500;

std::uint64_t resolve_cluster(std::uint64_t node)
{
	if (!node)
		return 0;
	const ULONGLONG now = GetTickCount64();
	auto it = g_node_cache.find(node);
	if (it != g_node_cache.end() && now - it->second.stamp < k_node_ttl_ms)
		return it->second.cluster;
	const std::uint64_t c = resolve_cluster_raw(node);
	g_node_cache[node] = NodeHit{ c, now };
	return c;
}

std::uint64_t scan_part_cluster(std::uint64_t part)
{
	for (int off = 8; off <= 0x400; off += 8)
	{
		const std::uint64_t p = g_Memory.Read<std::uint64_t>(part + (std::uintptr_t)off);
		if (!addr_ok((std::uintptr_t)p) || (p & 7))
			continue;
		if (const std::uint64_t base = cluster_base(p))
			return base;
		if (!is_binding_vt(g_Memory.Read<std::uint64_t>(p)))
			continue;
		const std::uint64_t owner =
			g_Memory.Read<std::uint64_t>(p + Offsets::FastClusterBinding::Owner);
		if (const std::uint64_t base = cluster_base(owner))
			return base;
	}
	return 0;
}

std::uint64_t resolve_part_cluster(std::uint64_t part, std::uint64_t node)
{
	if (const std::uint64_t c = resolve_cluster(node))
		return c;
	const ULONGLONG now = GetTickCount64();
	auto it = g_part_cluster_cache.find(part);
	if (it != g_part_cluster_cache.end() && now - it->second.stamp < k_node_ttl_ms)
		return it->second.cluster;
	const std::uint64_t c = scan_part_cluster(part);
	if (c)
		++g.nrec;
	g_part_cluster_cache[part] = NodeHit{ c, now };
	return c;
}

bool read_span(std::uint64_t cluster, int off, std::uint64_t& begin, std::uint64_t& end)
{
	if (!addr_ok((std::uintptr_t)(cluster + (std::uintptr_t)off)))
		return false;
	begin = g_Memory.Read<std::uint64_t>(cluster + (std::uintptr_t)off);
	end = g_Memory.Read<std::uint64_t>(cluster + (std::uintptr_t)(off + 8));
	if (!addr_ok((std::uintptr_t)begin) || !addr_ok((std::uintptr_t)end) || end <= begin)
		return false;
	if ((end - begin) & 7)
		return false;
	const std::uint64_t n = (end - begin) / 8;
	return n >= 1 && n <= (std::uint64_t)k_harvest_max;
}

struct Harvest
{
	int total = 0;
	int known = 0;
	bool geom = false;
};

Harvest harvest_span(std::uint64_t begin, std::uint64_t end, std::vector<std::uint64_t>& out)
{
	Harvest h{};
	const std::uint64_t count = (end - begin) / 8;
	const int n = (int)(count < (std::uint64_t)k_harvest_max ? count : (std::uint64_t)k_harvest_max);
	std::uint64_t ents[k_harvest_max]{};
	if (g_Memory.ReadRaw(begin, ents, (std::size_t)n * 8) != (std::size_t)n * 8)
		return h;
	for (int i = 0; i < n; ++i)
	{
		if (!addr_ok((std::uintptr_t)ents[i]) || (ents[i] & 7))
			continue;
		std::uint64_t hdr[10]{};
		if (g_Memory.ReadRaw(ents[i], hdr, sizeof(hdr)) != sizeof(hdr))
			continue;
		if (!addr_ok((std::uintptr_t)hdr[0]) || (hdr[0] & 7))
			continue;
		if (hdr[0] < g.module || hdr[0] >= g.module + 0x20000000ull)
			continue;
		++h.total;
		if (is_entity_vt(hdr[0]))
			++h.known;
		const int known_before = g_ngeom_known;
		add_geom(hdr[Offsets::FastClusterEntity::MaterialPtr / 8], out);
		add_geom(hdr[Offsets::FastClusterEntity::DecalMaterialPtr / 8], out);

		if (g_ngeom_known != known_before)
			h.geom = true;
	}
	return h;
}

int g_cl_off = -1;

void collect_cluster(std::uint64_t cluster, std::vector<std::uint64_t>& out)
{
	if (g.nent >= k_ent_budget)
		return;
	std::uint64_t begin = 0, end = 0;
	if (g_cl_off >= 0 && read_span(cluster, g_cl_off, begin, end))
	{
		const std::size_t before = out.size();
		const Harvest h = harvest_span(begin, end, out);
		if (h.geom || h.known)
		{
			g.nent += h.total;
			return;
		}
		out.resize(before);
		g_cl_off = -1;
	}

	int weak_off = -1;
	for (int off = 0; off <= 0x600; off += 8)
	{
		if (!read_span(cluster, off, begin, end))
			continue;
		const std::size_t before = out.size();
		const Harvest h = harvest_span(begin, end, out);

		if (!h.known && !h.geom && h.total < 3)
		{
			out.resize(before);
			continue;
		}
		if (h.known || h.geom)
		{
			g_cl_off = off;
			g.nent += h.total;
			return;
		}
		if (weak_off < 0)
			weak_off = off;
		out.resize(before);
	}
	if (weak_off >= 0 && read_span(cluster, weak_off, begin, end))
	{
		const Harvest h = harvest_span(begin, end, out);
		if (h.geom)
		{
			g_cl_off = weak_off;
			g.nent += h.total;
		}
	}
}

struct PartRef {
	std::uint64_t part = 0;
	std::uint64_t node = 0;
};

bool part_node_ok(std::uint64_t node)
{
	if (!addr_ok((std::uintptr_t)node) || (node & 7))
		return false;
	const std::uint64_t vt = g_Memory.Read<std::uint64_t>(node);
	return is_cluster_vt(vt) || is_binding_vt(vt);
}

void read_children(std::uint64_t owner, std::vector<std::uint64_t>& kids)
{
	kids.clear();
	const std::uint64_t holder =
		g_Memory.Read<std::uint64_t>(owner + Offsets::Instance::ChildrenStart);
	if (addr_ok((std::uintptr_t)holder))
	{
		const std::uint64_t begin = g_Memory.Read<std::uint64_t>(holder);
		const std::uint64_t end =
			g_Memory.Read<std::uint64_t>(holder + Offsets::Instance::ChildrenEnd);
		if (addr_ok((std::uintptr_t)begin) && addr_ok((std::uintptr_t)end) && end > begin)
		{
			const std::uint64_t bytes = end - begin;
			if ((bytes % 16) == 0 && bytes <= 0x4000)
			{
				kids.resize((std::size_t)(bytes / 16));
				for (std::size_t i = 0; i < kids.size(); ++i)
					kids[i] = g_Memory.Read<std::uint64_t>(begin + i * 16);
			}
		}
	}
	if (kids.empty())
	{

		for (std::uint64_t child : rbx::c_instance(owner).get_children())
			kids.push_back(child);
	}
}

void add_part_refs(const std::vector<std::uint64_t>& addrs, std::vector<PartRef>& out,
	int depth = 0)
{
	for (std::uint64_t part : addrs)
	{
		if (!addr_ok((std::uintptr_t)part) || (part & 7))
			continue;
		bool dup = false;
		for (const PartRef& r : out)
			if (r.part == part) { dup = true; break; }
		if (dup)
			continue;
		const std::uint64_t node =
			g_Memory.Read<std::uint64_t>(part + Offsets::BasePart::ClusterNode);
		const std::uint64_t vt = (addr_ok((std::uintptr_t)node) && !(node & 7))
			? g_Memory.Read<std::uint64_t>(node) : 0;
		if (!part_node_ok(node))
		{
			if (depth < 3)
			{
				std::vector<std::uint64_t> inner;
				inner.reserve(16);
				read_children(part, inner);
				if (!inner.empty() && inner.size() <= (std::size_t)k_max_ents)
				{
					const std::size_t before = out.size();
					add_part_refs(inner, out, depth + 1);
					bool found = false;
					for (std::size_t i = before; i < out.size() && !found; ++i)
						found = part_node_ok(out[i].node);
					if (found)
						continue;
				}
			}

			++g_nunk;
			if (!g_unk_vt)
				g_unk_vt = vt;
			out.push_back({ part, node });
			continue;
		}
		if (is_cluster_vt(vt))
			++g_nfc;
		out.push_back({ part, node });
	}
}

std::vector<PartRef> character_parts(std::uint64_t character)
{
	std::vector<PartRef> out;
	std::vector<std::uint64_t> kids;
	kids.reserve(32);
	read_children(character, kids);
	if (kids.size() > (std::size_t)k_max_ents)
		kids.resize((std::size_t)k_max_ents);
	add_part_refs(kids, out);
	return out;
}

std::vector<std::uint64_t> g_own_geoms;
ULONGLONG g_own_stamp = 0;
constexpr ULONGLONG k_own_refresh_ms = 250;

void refresh_own_geoms(const std::vector<std::uint64_t>& own_parts)
{
	const ULONGLONG now = GetTickCount64();
	if (now - g_own_stamp < k_own_refresh_ms)
		return;
	g_own_stamp = now;

	const int sn = g.nent, sb = g.nbind, su = g_nunk, sf = g_nfc, sp = g_npick;
	g_own_geoms.clear();
	std::vector<std::uint64_t> geoms;
	for (std::uint64_t part : own_parts)
	{
		if (!addr_ok((std::uintptr_t)part) || (part & 7))
			continue;
		const std::uint64_t node =
			g_Memory.Read<std::uint64_t>(part + Offsets::BasePart::ClusterNode);
		const std::uint64_t cluster = resolve_part_cluster(part, node);
		if (!cluster)
			continue;
		geoms.clear();
		collect_cluster(cluster, geoms);
		for (std::uint64_t q : geoms)
			if (std::find(g_own_geoms.begin(), g_own_geoms.end(), q) == g_own_geoms.end())
				g_own_geoms.push_back(q);
	}
	g.nent = sn;
	g.nbind = sb;
	g_nunk = su;
	g_nfc = sf;
	g_npick = sp;
}

std::vector<PartRef> known_parts(const cache::entity_t& e)
{
	std::vector<std::uint64_t> addrs;
	addrs.reserve(e.parts.size() + 2);
	addrs.push_back(e.humanoid_root_part.address);
	addrs.push_back(e.head_part.address);
	for (const auto& kv : e.parts)
		addrs.push_back(kv.second.address);
	std::vector<PartRef> out;
	add_part_refs(addrs, out);
	return out;
}

bool part_owned_by(std::uint64_t part, std::uint64_t character)
{
	std::uint64_t cur = part;
	for (int i = 0; i < 6; ++i)
	{
		if (!addr_ok((std::uintptr_t)cur) || (cur & 7))
			return false;
		if (cur == character)
			return true;
		cur = g_Memory.Read<std::uint64_t>(cur + Offsets::Instance::Parent);
	}
	return false;
}

void fill_geoms()
{
	if (!g.state || !g.geoms || g.stage < 11)
		return;

	auto off = [&]
	{
		g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 0);
		g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), 0);
		g.ntarget = 0;
		g_last_target = 0;
	};
	auto hold = [&]() -> bool
	{
		if (g_last_target && GetTickCount64() - g_last_target <= k_target_hold_ms)
			return true;
		off();
		return false;
	};
	if (!Active())
	{
		off();
		return;
	}
	static int last_mask = -1;
	const int mask = active_mask();
	if (mask != last_mask)
	{
		last_mask = mask;
		g_last_target = 0;
	}

	auto snap = cache::get_players_snap();
	if (!snap || snap->empty())
	{
		hold();
		return;
	}

	std::uint64_t local_team = 0;
	if (auto ls = cache::get_local_snap())
		local_team = ls->team;
	const LocalId local = local_identity();

	std::vector<std::uint64_t> own_parts;
	if (auto ls = cache::get_local_snap())
	{
		own_parts.reserve(ls->parts.size() + 2);
		own_parts.push_back(ls->humanoid_root_part.address);
		own_parts.push_back(ls->head_part.address);
		for (const auto& kv : ls->parts)
			own_parts.push_back(kv.second.address);
	}

	float pen = settings::visuals::native_walls ? settings::visuals::native_depth : 0.f;
	if (pen < 0.f) pen = 0.f;
	if (pen > 1.f) pen = 1.f;
	constexpr float k_max_pierce = 200.f;
	const float walls_scale = 1.f + pen * k_max_pierce;

	const float walls_bias = g.depth_rev ? 1e-5f : -1e-5f;
	static const auto t0 = std::chrono::steady_clock::now();
	const float now_s = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();

	std::vector<Target> targets;
	targets.reserve(64);
	std::vector<std::uint64_t> seen;
	seen.reserve(32);
	std::vector<std::uint64_t> taken;
	taken.reserve(64);
	g.nnode = 0;
	g.nent = 0;
	g.ndrop = 0;
	g.nbind = 0;
	g.nrec = 0;
	g_nunk = 0;
	g_nfc = 0;
	g_npick = 0;
	g_nnopart = 0;
	g_nnochar = 0;
	g_npartrefs = 0;
	g_ngeorej = 0;
	g_nself = 0;
	g_nworld = 0;
	g_unk_vt = 0;
	g_cls[0] = g_cls[1] = g_cls[2] = 0;
	g_on = active_mask();
	g_lid = (local.model ? 1 : 0) | (local.player ? 2 : 0) | (local.user ? 4 : 0)
		| (local.hrp ? 8 : 0) | (cache::get_local_snap() ? 16 : 0);
	g_npl = (int)snap->size();

	if (own_parts.empty() && local.model)
	{
		for (const PartRef& r : character_parts(local.model))
			own_parts.push_back(r.part);
	}
	refresh_own_geoms(own_parts);
	const ULONGLONG now_ms = GetTickCount64();
	int mode = settings::visuals::native_render_shader;
	if (mode < 0) mode = 0;
	if (mode >= ShaderNameCount()) mode = ShaderNameCount() - 1;
	for (const auto& e : *snap)
	{
		if (e.health <= 0.f && e.max_health > 0.f)
			continue;
		const std::uint64_t character = nazichams::character_address(e);
		const int ent_idx = classify_entity(e, local_team, local, character);
		++g_cls[ent_idx & 3];

		if (ent_idx == 1 && (settings::visuals::native_teamcheck || settings::teamcheck))
			continue;
		if (!wants_native(ent_idx) && !is_target_native(ent_idx))
			continue;
		++g_npick;

		std::vector<PartRef> parts;
		if (character)
			parts = character_parts(character);
		else
			++g_nnochar;
		{
			std::vector<PartRef> known = known_parts(e);
			for (const PartRef& r : known)
			{
				bool dup = false;
				for (const PartRef& p : parts)
					if (p.part == r.part) { dup = true; break; }
				if (!dup)
					parts.push_back(r);
			}
		}
		if (character)
		{
			const std::size_t before = parts.size();
			parts.erase(std::remove_if(parts.begin(), parts.end(),
				[&](const PartRef& r) { return !part_owned_by(r.part, character); }),
				parts.end());
			g_nworld += (int)(before - parts.size());
		}
		g_npartrefs += (int)parts.size();
		if (parts.empty())
		{
			++g_nnopart;
			continue;
		}
		const auto* ent_cfg = (ent_idx == 2) ? &settings::visuals::neutral
			: ((ent_idx == 1) ? &settings::visuals::friendly : &settings::visuals::hostile);
		std::vector<std::uint64_t> ent_geo;
		ent_geo.reserve(8);
		for (const PartRef& ref : parts)
		{
			if (ent_idx != 2 && !own_parts.empty() &&
				std::find(own_parts.begin(), own_parts.end(), ref.part) != own_parts.end())
				continue;

			if (std::find(seen.begin(), seen.end(), ref.part) != seen.end())
				continue;
			seen.push_back(ref.part);
			const std::uint64_t cluster = resolve_part_cluster(ref.part, ref.node);
			if (!cluster)
				continue;
			++g.nnode;

			CachedPart& cache = g_part_cache[ref.part];
			if (cache.geoms.empty() || cache.cluster != cluster ||
				now_ms - cache.stamp >= k_geom_refresh_ms)
			{
				std::vector<std::uint64_t> fresh;
				collect_cluster(cluster, fresh);
				if (!fresh.empty())
				{
					cache.cluster = cluster;
					cache.geoms.swap(fresh);
					cache.stamp = now_ms;
				}
			}
			if (!cache.geoms.empty() && now_ms - cache.stamp > k_geom_expire_ms)
			{
				cache.geoms.clear();
				cache.cluster = 0;
			}

			for (std::uint64_t q : cache.geoms)
			{
				if (std::find(ent_geo.begin(), ent_geo.end(), q) == ent_geo.end())
					ent_geo.push_back(q);
			}
		}

		const std::uint64_t ent_key = character ? character
			: (e.humanoid_root_part.address ? e.humanoid_root_part.address
				: e.instance.address);
		if (!ent_key)
		{
			++g_nnopart;
			continue;
		}
		EntGeom& ec = g_ent_cache[ent_key];
		if (!ent_geo.empty())
		{
			ec.geoms = ent_geo;
			ec.stamp = now_ms;
		}
		else if (!ec.geoms.empty() && now_ms - ec.stamp <= k_ent_expire_ms)
			ent_geo = ec.geoms;
		if (ent_geo.empty())
		{
			++g_nnopart;
			continue;
		}
		for (std::uint64_t q : ent_geo)
		{

			if (ent_idx != 2 && !g_own_geoms.empty() &&
				std::find(g_own_geoms.begin(), g_own_geoms.end(), q) != g_own_geoms.end())
				++g_nself;
			if ((int)targets.size() >= k_max_targets)
				break;
			if (std::find(taken.begin(), taken.end(), q) != taken.end())
				continue;
			taken.push_back(q);
			Target t{};
			t.geom = q;
			if (ent_cfg->chams && ent_cfg->chams_type == 3)
				std::memcpy(t.color, ent_cfg->chams_colour, sizeof(t.color));
			else
				std::memcpy(t.color, settings::visuals::native_render_col, sizeof(t.color));
			t.glow[0] = settings::visuals::native_glow_col[0];
			t.glow[1] = settings::visuals::native_glow_col[1];
			t.glow[2] = settings::visuals::native_glow_col[2];
			t.glow[3] = settings::visuals::native_glow ? settings::visuals::native_glow_strength : 0.f;
			t.mode = mode;
			t.time = now_s;
			t.depth_scale = walls_scale;
			t.depth_bias = walls_bias;
			targets.push_back(t);
		}
	}

	if (g_part_cache.size() > 512)
	{
		for (auto it = g_part_cache.begin(); it != g_part_cache.end();)
		{
			if (now_ms - it->second.stamp > 5000)
				it = g_part_cache.erase(it);
			else
				++it;
		}
	}
	auto prune = [now_ms](auto& map)
	{
		if (map.size() <= 4096)
			return;
		for (auto it = map.begin(); it != map.end();)
		{
			if (now_ms - it->second.stamp > k_node_ttl_ms * 4)
				it = map.erase(it);
			else
				++it;
		}
	};
	prune(g_node_cache);
	prune(g_part_cluster_cache);
	if (g_ent_cache.size() > 256)
	{
		for (auto it = g_ent_cache.begin(); it != g_ent_cache.end();)
		{
			if (now_ms - it->second.stamp > k_ent_expire_ms * 2)
				it = g_ent_cache.erase(it);
			else
				++it;
		}
	}

	if (targets.empty())
	{

		if (!hold())
			g.ntarget = 0;
		return;
	}
	g.ntarget = (int)targets.size();
	g_last_target = now_ms;
	const std::uintptr_t dst = g.geoms + (std::size_t)g.slot * k_max_targets * sizeof(Target);
	g_Memory.WriteRaw(dst, targets.data(), targets.size() * sizeof(Target));
	g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, geoms), dst);
	g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), (std::uint32_t)targets.size());
	g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 1);
	g.slot ^= 1;
}

bool install()
{
	if (g.hooked)
		return true;
	const auto now = std::chrono::steady_clock::now();
	if (g_fail.time_since_epoch().count() != 0 &&
		now - g_fail < std::chrono::milliseconds(800))
		return false;
	const std::uintptr_t base = module_base();
	if (!base || !g_Memory.GetHandle() || !g_Memory.IsAlive())
	{
		g_fail = now;
		return false;
	}
	if (!g.state)
		g.state = g_Memory.Alloc(0x1000, PAGE_READWRITE);
	if (!g.state)
	{
		fail("alloc");
		g_fail = now;
		return false;
	}
	if (!find_d3d())
	{
		fail("no device");
		g_fail = now;
		return false;
	}
	if (!g.ctx)
	{
		fail("no ctx");
		g_fail = now;
		return false;
	}
	if (!setup_meta())
	{
		fail("gpu meta");
		g_fail = now;
		return false;
	}

	auto cr = make_create_stub(g.state);
	auto dr = make_draw_thunk(g.state);
	auto ps = make_present_thunk(g.state, 0x10000);
	if (cr.empty() || dr.empty() || ps.empty())
	{
		fail("thunk");
		g_fail = now;
		return false;
	}
	const auto align16 = [](std::size_t n) { return (n + 15u) & ~std::size_t(15); };
	const std::size_t nps = align16(ps.size());
	const std::size_t ncr = align16(cr.size());
	const std::size_t ndr = align16(dr.size());
	const std::size_t nall = nps + ncr + ndr;
	const std::uintptr_t cave = find_dll_cave(nall, 0);
	if (!cave)
	{
		fail("cave");
		g_fail = now;
		return false;
	}
	g.cave = cave;
	g.cave_n = nall;
	g.present_thunk = cave;
	g.create_stub = cave + nps;
	g.draw_thunk = cave + nps + ncr;
	if (!g.vt_mem)
		g.vt_mem = g_Memory.Alloc(0x2000, PAGE_READWRITE);
	if (!g.vt_mem)
	{
		fail("alloc vt");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	ps = make_present_thunk(g.state, g.create_stub);
	if (ps.size() > nps)
	{
		fail("thunk jmp");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	if (!write_exec(g.create_stub, cr.data(), cr.size())
		|| !write_exec(g.draw_thunk, dr.data(), dr.size())
		|| !write_exec(g.present_thunk, ps.data(), ps.size()))
	{
		fail("write stub");
		g.cave = 0;
		g_fail = now;
		return false;
	}
	FlushInstructionCache(g_Memory.GetHandle(), (void*)g.cave, nall);
	mark_cfg(g.present_thunk);
	mark_cfg(g.create_stub);
	mark_cfg(g.draw_thunk);
	if (!patch_vtables())
	{
		fail("vt");
		unpatch();
		g.cave = 0;
		g_fail = now;
		return false;
	}
	g.module = base;
	g.hooked = true;
	g.stage = 0;
	if (!g.logged)
	{
		g.logged = true;
		Cheat::Console::Log(Cheat::Console::Color::Green, "nativechams on patches=%d cave=0x%llX swapvt=%d dcvt=%d swap=0x%llX dc=0x%llX",
			g.npatch, (unsigned long long)g.cave, g.nswvt, g.ndcvt,
			(unsigned long long)g.swap, (unsigned long long)g.dc);
	}
	return true;
}

void apply_occlusion(bool off)
{
	if (off == g.occl_off || !g.module)
		return;
	if (off)
	{
		const std::uintptr_t consts[] = {
			Offsets::FFlag::RenderFastClusterOcclusionCulling,
			FFlagOffsets::FFlags::RenderFastClusterOcclusionCulling4,
			FFlagOffsets::FFlags::DebugDisableModelClusterOcclusionCulling,
		};
		g.noccl = 0;
		for (std::uintptr_t rva : consts)
		{
			if (!rva || g.noccl >= 4)
				continue;
			const std::uintptr_t addr = g.module + rva;
			bool dup = false;
			for (int i = 0; i < g.noccl; ++i)
				if (g.occl[i].addr == addr)
					dup = true;
			if (dup)
				continue;
			const std::uint8_t forced =
				(rva == FFlagOffsets::FFlags::DebugDisableModelClusterOcclusionCulling) ? 1 : 0;
			Hook::OcclFlag& e = g.occl[g.noccl++];
			e.addr = addr;
			e.orig = g_Memory.Read<std::uint8_t>(addr);
			g_Memory.Write<std::uint8_t>(addr, forced);
			Cheat::Console::Log(Cheat::Console::Color::Yellow,
				"nativechams wall-flag +0x%llX: %u -> %u",
				(unsigned long long)rva, (unsigned)e.orig, (unsigned)forced);
		}
	}
	else
	{
		for (int i = 0; i < g.noccl; ++i)
		{
			if (g.occl[i].addr)
				g_Memory.Write<std::uint8_t>(g.occl[i].addr, g.occl[i].orig);
			g.occl[i] = {};
		}
		if (g.noccl)
			Cheat::Console::Log(Cheat::Console::Color::Yellow,
				"nativechams wall-flags restored (%d) - culling is back on", g.noccl);
		g.noccl = 0;
	}
	g.occl_off = off;
}

void restore_patches()
{
	for (int pass = 0; pass < 3; ++pass)
	{
		bool dirty = false;
		for (int i = 0; i < g.npatch; ++i)
		{
			if (!g.patches[i].at)
				continue;
			if (g_Memory.Read<std::uint64_t>(g.patches[i].at) == g.patches[i].old)
				continue;
			DWORD prot = 0;
			if (!g_Memory.Protect(g.patches[i].at, 8, PAGE_READWRITE, &prot))
				continue;
			const bool ok = g_Memory.Write<std::uint64_t>(g.patches[i].at, g.patches[i].old);
			g_Memory.Protect(g.patches[i].at, 8, prot, nullptr);
			if (ok)
				dirty = true;
		}
		if (!dirty)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(4));
	}
	for (int i = 0; i < g.npatch; ++i)
		g.patches[i] = {};
	g.npatch = 0;
}

void teardown()
{
	if (g.state)
	{

		g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, enabled), 0);
		g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, ngeom), 0);
		g_Memory.Write<std::uint64_t>(g.state + offsetof(RemoteState, geoms), 0);
	}
	apply_occlusion(false);
	if (!g.hooked)
		return;
	restore_patches();
	g.hooked = false;
}

}

void Tick()
{
	std::lock_guard<std::mutex> lock(g_tick_mutex);
	if (g_stopping)
	{

		teardown();
		return;
	}
	if (!g_Memory.IsAttached() || !g_Memory.IsAlive())
	{
		teardown();
		g.device = 0;
		g.ctx = 0;
		g.swap = 0;
		g.dc = 0;
		g.orig_present = 0;
		g.orig_draw = 0;
		return;
	}

	const std::uintptr_t base = module_base();
	if (g.module && base && g.module != base)
	{
		teardown();
		if (g.state) { g_Memory.Free(g.state); g.state = 0; }
		if (g.geoms) { g_Memory.Free(g.geoms); g.geoms = 0; }
		if (g.vt_mem) { g_Memory.Free(g.vt_mem); g.vt_mem = 0; }
		if (g.ps_dxbc) { g_Memory.Free(g.ps_dxbc); g.ps_dxbc = 0; }
		g = {};
		g_why = nullptr;
		g_part_cache.clear();
		g_last_target = 0;
	}

	if (!Active() && !g.hooked)
		return;
	const bool rev = want_reverse();
	const bool walls = settings::visuals::native_walls;
	if (!g.hooked)
	{
		g.depth_rev = rev;
		g.walls = walls;
		if (!install())
			return;
		Cheat::Console::Log(Cheat::Console::Color::Yellow, "nativechams depth=%s walls=%d",
			rev ? "reverse" : "standard", walls ? 1 : 0);
	}
	g.walls = walls;
	if (g.stage >= 11 && g.depth_rev != rev)
	{
		g.depth_rev = rev;
		g.stage = 2;
		g_Memory.Write<std::uint32_t>(g.state + offsetof(RemoteState, ready), 0);
		Cheat::Console::Log(Cheat::Console::Color::Yellow, "nativechams dss rebuilt (reverse=%d walls=%d)",
			rev ? 1 : 0, walls ? 1 : 0);
	}
	pump_creates();
	fill_geoms();

	const bool walls_occl = settings::visuals::native_walls;
	apply_occlusion(walls_occl);
	if (walls_occl && g.module)
	{
		for (int i = 0; i < g.noccl; ++i)
		{
			if (!g.occl[i].addr)
				continue;
			const std::uint8_t forced = (g.occl[i].addr == g.module +
				FFlagOffsets::FFlags::DebugDisableModelClusterOcclusionCulling) ? 1 : 0;
			if (g_Memory.Read<std::uint8_t>(g.occl[i].addr) != forced)
				g_Memory.Write<std::uint8_t>(g.occl[i].addr, forced);
		}
	}
	static auto last_stat = std::chrono::steady_clock::time_point{};
	static int last_stage = -1;
	if (g.stage == 11 && last_stage != 11)
	{
		last_stage = 11;
		Cheat::Console::Log(Cheat::Console::Color::Green, "nativechams ready st=%d cre=%u",
			g.stage, Creates());
	}
	const auto now = std::chrono::steady_clock::now();
	if (last_stat.time_since_epoch().count() == 0 ||
		now - last_stat >= std::chrono::seconds(5))
	{
		last_stat = now;
		Cheat::Console::Log(Cheat::Console::Color::Yellow,
			"nativechams st=%d rdy=%u frames=%u draws=%u geom=%u own=%d pl=%d on=%X cls=%d/%d/%d node=%d ent=%d bind=%d rec=%d drop=%d pick=%d unk=%d fc=%d nopart=%d nochar=%d parts=%d rej=%d world=%d shared=%d lid=%X unkvt=+0x%llX off=%d",
			g.stage, Ready(), Entered(), Draws(), Items(), (int)g_own_geoms.size(), g_npl, g_on,
			g_cls[0], g_cls[1], g_cls[2], g.nnode, g.nent, g.nbind, g.nrec, g.ndrop,
			g_npick, g_nunk, g_nfc, g_nnopart, g_nnochar, g_npartrefs, g_ngeorej, g_nworld,
			g_nself, g_lid,
			(g.module && g_unk_vt > g.module)
				? (unsigned long long)(g_unk_vt - g.module) : 0ull,
			g_cl_off);
	}
}

void Run()
{
	for (;;)
	{
		try { Tick(); }
		catch (...) {}
		std::this_thread::sleep_for(std::chrono::milliseconds(33));
	}
}

void Stop()
{

	std::lock_guard<std::mutex> lock(g_tick_mutex);
	g_stopping = true;
	teardown();

	std::this_thread::sleep_for(std::chrono::milliseconds(60));
	if (g.state) { g_Memory.Free(g.state); g.state = 0; }
	if (g.geoms) { g_Memory.Free(g.geoms); g.geoms = 0; }
	if (g.vt_mem) { g_Memory.Free(g.vt_mem); g.vt_mem = 0; }
	if (g.ps_dxbc) { g_Memory.Free(g.ps_dxbc); g.ps_dxbc = 0; }
	g.swap = 0;
	g.swap_vt = 0;
	g.dc = 0;
	g.dc_vt = 0;
	g.device = 0;
	g.ctx = 0;
	g.cave = 0;
	g.cave_n = 0;
	g.present_thunk = 0;
	g.create_stub = 0;
	g.draw_thunk = 0;
	g.orig_present = 0;
	g.orig_draw = 0;
	g.module = 0;
	g.stage = 0;
	g.ntarget = 0;
	g_part_cache.clear();
	g_ent_cache.clear();
	g_last_target = 0;
	Cheat::Console::Log(Cheat::Console::Color::Yellow, "nativechams off (hooks restored)");
}

void Dump()
{
	if (!g.hooked && !g.state)
		return;
	Cheat::Console::Log(Cheat::Console::Color::Red, "nativechams hooked=%d st=%d rdy=%u draws=%u geom=%u node=%d ent=%d bind=%d drop=%d",
		g.hooked ? 1 : 0, g.stage, Ready(), Draws(), Items(), g.nnode, g.nent, g.nbind, g.ndrop);
	if (!g.state)
		return;
	RemoteState st{};
	g_Memory.ReadRaw(g.state, &st, sizeof(st));
	Cheat::Console::Log(Cheat::Console::Color::Red, "nativechams ps=0x%llX dss=0x%llX cb=0x%llX blend=0x%llX entered=%u last_rtv=0x%llX",
		(unsigned long long)st.ps, (unsigned long long)st.dss,
		(unsigned long long)st.cb, (unsigned long long)st.blend,
		st.entered, (unsigned long long)st.last_rtv);
}

bool Hooked()
{
	return g.hooked;
}

const char* Why()
{
	if (g.hooked)
	return nullptr;
	return g_why ? g_why : "off";
}

std::uintptr_t Cave()
{
	return g.cave;
}

std::size_t CaveSize()
{
	return g.cave_n;
}

bool CaveBusy(std::uintptr_t at, std::size_t n)
{
	if (!at || !n)
		return false;
	return g.cave && at < g.cave + g.cave_n && g.cave < at + n;
}

std::uint32_t Hits()
{
	if (!g.state)
		return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, hits));
}

std::uint32_t Draws()
{
	if (!g.state)
	return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, draws));
}

std::uint32_t Items()
{
	if (!g.state)
	return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, ngeom));
}

std::uint32_t Entered()
{
	if (!g.state)
		return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, entered));
}

std::uint32_t Creates()
{
	if (!g.state)
		return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, creates));
}

std::uint32_t Stage()
{
	return g.stage;
}

std::uint32_t Ready()
{
	if (!g.state)
		return 0;
	return g_Memory.Read<std::uint32_t>(g.state + offsetof(RemoteState, ready));
}

const char* const* ShaderNames()
{
	static const char* k[] = {
		"Flat", "Lit", "Heat", "Lava", "Plasma", "Volt", "Aurora", "Pulse", "Glitch", "Wraith", "Lightning"
	};
	return k;
}

int ShaderNameCount()
{
	return 11;
}

}
