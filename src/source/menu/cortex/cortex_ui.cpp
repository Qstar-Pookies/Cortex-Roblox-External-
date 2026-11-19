

#include "cortex_ui.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cfloat>
#include <ctime>
#include <functional>

namespace cx {

static ImFont *F_reg = nullptr, *F_small = nullptr, *F_bold = nullptr;
void SetFonts(ImFont* r, ImFont* s, ImFont* b) { F_reg = r; F_small = s; F_bold = b; }
static ImFont* Fr() { return F_reg ? F_reg : ImGui::GetFont(); }
static ImFont* Fs() { return F_small ? F_small : ImGui::GetFont(); }
static ImFont* Fb() { return F_bold ? F_bold : ImGui::GetFont(); }
static ImVec2 TextSz(ImFont* f, const char* t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, t); }
static void Txt(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t) { dl->AddText(f, f->FontSize, p, c, t); }

static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float Mn(float a, float b) { return a < b ? a : b; }
static float Mx(float a, float b) { return a > b ? a : b; }
static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}
static ImVec4 A(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, c.w * a); }

static ImU32 Col(const ImVec4& c, float a = 1.f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * a * ImGui::GetStyle().Alpha));
}

static const ImVec4 kWin   (0.043f, 0.043f, 0.051f, 1.00f);
static const ImVec4 kSide  (0.055f, 0.055f, 0.064f, 1.f);
static const ImVec4 kCard  (0.075f, 0.075f, 0.084f, 1.f);
static const ImVec4 kHi    (0.105f, 0.105f, 0.118f, 1.f);
static const ImVec4 kFrame (0.115f, 0.115f, 0.128f, 1.f);
static const ImVec4 kText  (0.930f, 0.930f, 0.945f, 1.f);
static const ImVec4 kSoft  (0.780f, 0.780f, 0.800f, 1.f);
static const ImVec4 kDim   (0.520f, 0.520f, 0.550f, 1.f);
static const ImVec4 kFaint (0.330f, 0.330f, 0.360f, 1.f);
static const ImVec4 kLine  (1.f, 1.f, 1.f, 0.055f);

struct Theme { const char* name; ImVec4 acc, acc2; };
static Theme g_themes[] = {
    { "Fire",    ImVec4(1.00f, 0.16f, 0.10f, 1.f), ImVec4(1.00f, 0.64f, 0.10f, 1.f) },
    { "Crimson", ImVec4(0.80f, 0.04f, 0.14f, 1.f), ImVec4(1.00f, 0.30f, 0.36f, 1.f) },
    { "Mono",    ImVec4(0.96f, 0.96f, 0.98f, 1.f), ImVec4(0.62f, 0.62f, 0.68f, 1.f) },
    { "Mint",    ImVec4(0.10f, 0.92f, 0.62f, 1.f), ImVec4(0.10f, 0.70f, 0.98f, 1.f) },
    { "Violet",  ImVec4(0.86f, 0.26f, 1.00f, 1.f), ImVec4(0.42f, 0.30f, 1.00f, 1.f) },
    { "Custom",  ImVec4(1.00f, 0.20f, 0.55f, 1.f), ImVec4(1.00f, 0.75f, 0.20f, 1.f) },
};
static const int kThemeCount = 6;
static int    g_theme = 0;
static ImVec4 g_acc = g_themes[0].acc, g_acc2 = g_themes[0].acc2;

static float g_glowMul = 1.f;     
static float g_animMul = 1.f;     
static bool  g_sweep = true;      
static bool  g_outerGlow = true;  
static bool  g_flicker = true;    
static bool  g_watermark = true;
static float g_outlineSpeed = 1.f; 

static void RecolorConic(ImDrawList* dl, int v0, ImVec2 c, float phase, float hot = 0.f) {
    for (int i = v0; i < dl->VtxBuffer.Size; ++i) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float ang = atan2f(v.pos.y - c.y, v.pos.x - c.x);
        float t = 0.5f + 0.5f * sinf(ang * 2.f - phase);
        ImVec4 m = Mix(g_acc, g_acc2, t);
        if (hot > 0.f) { 
            float k = powf(0.5f + 0.5f * cosf(ang - phase * 0.5f), 24.f) * hot;
            m = Mix(m, ImVec4(1.f, 0.95f, 0.85f, 1.f), Clamp01(k));
        }
        unsigned a = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
        v.col = IM_COL32((int)(Clamp01(m.x) * 255), (int)(Clamp01(m.y) * 255), (int)(Clamp01(m.z) * 255), a);
    }
}

static ImVec4 OnAcc() { 
    ImVec4 m = Mix(g_acc, g_acc2, 0.5f);
    float l = 0.299f * m.x + 0.587f * m.y + 0.114f * m.z;
    return l > 0.62f ? ImVec4(0.06f, 0.06f, 0.07f, 1) : ImVec4(1, 1, 1, 1);
}

static ImGuiID HashID(const char* s) {
    ImGuiID h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h ? h : 1u;
}

static std::unordered_map<ImGuiID, float> g_anim;
static float Anim(ImGuiID id, float target, float speed = 14.f) {
    auto it = g_anim.find(id);
    if (it == g_anim.end()) it = g_anim.emplace(id, target).first;
    it->second += (target - it->second) * Clamp01(ImGui::GetIO().DeltaTime * speed * g_animMul);
    return it->second;
}
static float AnimS(const char* key, float target, float speed = 14.f) { return Anim(ImGui::GetID(key), target, speed); }

static void Glow(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec4 c, float rounding, float strength, int layers = 6, float step = 1.6f) {
    strength *= g_glowMul;
    if (strength <= 0.01f) return;
    for (int i = 1; i <= layers; ++i) {
        float s = i * step;
        dl->AddRectFilled(ImVec2(a.x - s, a.y - s), ImVec2(b.x + s, b.y + s),
                          Col(c, 0.07f * strength * (1.f - (float)i / (layers + 1))), rounding + s);
    }
}

static void Grad(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c0, ImU32 c1, float rounding, bool vertical = false, ImDrawFlags fl = 0) {
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding, fl);
    ImVec4 f0 = ImGui::ColorConvertU32ToFloat4(c0), f1 = ImGui::ColorConvertU32ToFloat4(c1);
    for (int i = v0; i < dl->VtxBuffer.Size; ++i) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float t = vertical ? (v.pos.y - a.y) / Mx(1.f, b.y - a.y) : (v.pos.x - a.x) / Mx(1.f, b.x - a.x);
        v.col = ImGui::ColorConvertFloat4ToU32(Mix(f0, f1, Clamp01(t)));
    }
}

static void RecolorAcc(ImDrawList* dl, int v0, ImVec2 a, ImVec2 b, float alpha, bool vertical) {
    for (int i = v0; i < dl->VtxBuffer.Size; ++i) {
        ImDrawVert& v = dl->VtxBuffer[i];
        float t = vertical ? (v.pos.y - a.y) / Mx(1.f, b.y - a.y) : (v.pos.x - a.x) / Mx(1.f, b.x - a.x);
        float va = ((v.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.f; 
        v.col = Col(Mix(g_acc, g_acc2, Clamp01(t)), alpha * va);
    }
}

static void AccFill(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha = 1.f, bool vertical = false) {
    int v0 = dl->VtxBuffer.Size;
    dl->AddRectFilled(a, b, IM_COL32_WHITE, rounding);
    RecolorAcc(dl, v0, a, b, alpha, vertical);
}

static void Logo(ImDrawList* dl, ImVec2 c, float r, float alpha = 1.f) {
    const float PI = 3.14159265f;
    for (int i = 1; i <= 5; ++i)
        dl->AddCircleFilled(c, r + i * 1.8f, Col(Mix(g_acc, g_acc2, 0.5f), alpha * 0.05f * g_glowMul * (1.f - i / 6.f)), 32);
    int v0 = dl->VtxBuffer.Size;
    dl->PathArcTo(c, r, PI * 0.27f, PI * 1.73f, 32);
    dl->PathStroke(IM_COL32_WHITE, 0, r * 0.46f);
    RecolorAcc(dl, v0, ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), alpha, true);
    
    dl->AddCircleFilled(ImVec2(c.x + r * 0.62f, c.y), r * 0.2f, Col(g_acc2, alpha), 16);
}

enum Ico { I_Legit, I_Rage, I_Anti, I_Eye, I_Globe, I_Grid, I_User, I_Keys, I_List, I_Pulse, I_Folder, I_Gear,
           I_Play, I_Chat, I_Plane, I_Refresh, I_Close, I_Chevron };

static float g_iconRot = 0.f; 
static void Icon(ImDrawList* dl, int k, ImVec2 c, float s, ImU32 col, float th = 1.5f) {
    const float PI = 3.14159265f;
    switch (k) {
    case I_Legit:
        dl->AddCircle(c, s * 0.72f, col, 24, th);
        dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y - s * 0.38f), col, th);
        dl->AddLine(ImVec2(c.x, c.y + s), ImVec2(c.x, c.y + s * 0.38f), col, th);
        dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * 0.38f, c.y), col, th);
        dl->AddLine(ImVec2(c.x + s, c.y), ImVec2(c.x + s * 0.38f, c.y), col, th);
        dl->AddCircleFilled(c, 1.4f, col);
        break;
    case I_Rage: {
        ImVec2 p[6] = { {c.x, c.y - s}, {c.x + s * 0.85f, c.y - s * 0.55f}, {c.x + s * 0.7f, c.y + s * 0.35f},
                        {c.x, c.y + s}, {c.x - s * 0.7f, c.y + s * 0.35f}, {c.x - s * 0.85f, c.y - s * 0.55f} };
        dl->AddPolyline(p, 6, col, ImDrawFlags_Closed, th);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.45f), ImVec2(c.x, c.y + s * 0.45f), col, th);
        break; }
    case I_Anti: {
        dl->PathArcTo(c, s * 0.78f, 0.5f, 2 * PI - 0.3f, 24);
        dl->PathStroke(col, 0, th);
        ImVec2 e(c.x + cosf(0.5f) * s * 0.78f, c.y + sinf(0.5f) * s * 0.78f);
        dl->AddTriangleFilled(ImVec2(e.x + 3, e.y - 2), ImVec2(e.x - 3.5f, e.y - 1), ImVec2(e.x + 1, e.y + 4), col);
        break; }
    case I_Eye:
        dl->AddEllipse(c, ImVec2(s, s * 0.58f), col, 0, 28, th);
        dl->AddCircleFilled(c, s * 0.3f, col, 16);
        break;
    case I_Globe:
        dl->AddCircle(c, s * 0.85f, col, 24, th);
        dl->AddEllipse(c, ImVec2(s * 0.36f, s * 0.85f), col, 0, 20, th);
        dl->AddLine(ImVec2(c.x - s * 0.85f, c.y), ImVec2(c.x + s * 0.85f, c.y), col, th);
        break;
    case I_Grid: {
        float q = s * 0.78f, g = s * 0.12f;
        dl->AddRect(ImVec2(c.x - q, c.y - q), ImVec2(c.x - g, c.y - g), col, 1.5f, 0, th);
        dl->AddRect(ImVec2(c.x + g, c.y - q), ImVec2(c.x + q, c.y - g), col, 1.5f, 0, th);
        dl->AddRect(ImVec2(c.x - q, c.y + g), ImVec2(c.x - g, c.y + q), col, 1.5f, 0, th);
        dl->AddRectFilled(ImVec2(c.x + g, c.y + g), ImVec2(c.x + q, c.y + q), col, 1.5f);
        break; }
    case I_User:
        dl->AddCircleFilled(ImVec2(c.x, c.y - s * 0.38f), s * 0.38f, col, 16);
        dl->PathArcTo(ImVec2(c.x, c.y + s * 0.95f), s * 0.78f, PI, 2 * PI, 16);
        dl->PathFillConvex(col);
        break;
    case I_Keys:
        dl->AddRect(ImVec2(c.x - s, c.y - s * 0.62f), ImVec2(c.x + s, c.y + s * 0.62f), col, 2.f, 0, th);
        for (int i = 0; i < 4; ++i) dl->AddCircleFilled(ImVec2(c.x - s * 0.55f + i * s * 0.37f, c.y - s * 0.18f), 1.1f, col);
        dl->AddLine(ImVec2(c.x - s * 0.45f, c.y + s * 0.28f), ImVec2(c.x + s * 0.45f, c.y + s * 0.28f), col, th);
        break;
    case I_List:
        for (int i = -1; i <= 1; ++i) {
            dl->AddCircleFilled(ImVec2(c.x - s * 0.8f, c.y + i * s * 0.6f), 1.4f, col);
            dl->AddLine(ImVec2(c.x - s * 0.4f, c.y + i * s * 0.6f), ImVec2(c.x + s, c.y + i * s * 0.6f), col, th);
        }
        break;
    case I_Pulse: {
        ImVec2 p[6] = { {c.x - s, c.y}, {c.x - s * 0.45f, c.y}, {c.x - s * 0.2f, c.y - s * 0.75f},
                        {c.x + s * 0.2f, c.y + s * 0.75f}, {c.x + s * 0.45f, c.y}, {c.x + s, c.y} };
        dl->AddPolyline(p, 6, col, 0, th);
        break; }
    case I_Folder: {
        ImVec2 p[6] = { {c.x - s, c.y - s * 0.65f}, {c.x - s * 0.25f, c.y - s * 0.65f}, {c.x, c.y - s * 0.35f},
                        {c.x + s, c.y - s * 0.35f}, {c.x + s, c.y + s * 0.7f}, {c.x - s, c.y + s * 0.7f} };
        dl->AddConvexPolyFilled(p, 6, col);
        break; }
    case I_Gear:
        for (int i = 0; i < 8; ++i) {
            float a = i * PI / 4 + g_iconRot;
            dl->AddLine(ImVec2(c.x + cosf(a) * s * 0.55f, c.y + sinf(a) * s * 0.55f),
                        ImVec2(c.x + cosf(a) * s * 0.98f, c.y + sinf(a) * s * 0.98f), col, th + 0.8f);
        }
        dl->AddCircle(c, s * 0.62f, col, 16, th);
        dl->AddCircleFilled(c, s * 0.22f, col, 12);
        break;
    case I_Play:
        dl->AddRectFilled(ImVec2(c.x - s, c.y - s * 0.7f), ImVec2(c.x + s, c.y + s * 0.7f), col, s * 0.35f);
        dl->AddTriangleFilled(ImVec2(c.x - s * 0.28f, c.y - s * 0.35f), ImVec2(c.x - s * 0.28f, c.y + s * 0.35f),
                              ImVec2(c.x + s * 0.38f, c.y), Col(kWin));
        break;
    case I_Chat:
        dl->AddRectFilled(ImVec2(c.x - s, c.y - s * 0.68f), ImVec2(c.x + s, c.y + s * 0.6f), col, s * 0.45f);
        dl->AddCircleFilled(ImVec2(c.x - s * 0.36f, c.y - s * 0.05f), s * 0.17f, Col(kWin), 10);
        dl->AddCircleFilled(ImVec2(c.x + s * 0.36f, c.y - s * 0.05f), s * 0.17f, Col(kWin), 10);
        break;
    case I_Plane: {
        ImVec2 p[4] = { {c.x - s, c.y - s * 0.05f}, {c.x + s, c.y - s * 0.8f}, {c.x + s * 0.55f, c.y + s * 0.8f}, {c.x - s * 0.1f, c.y + s * 0.2f} };
        dl->AddConvexPolyFilled(p, 4, col);
        break; }
    case I_Refresh: {
        dl->PathArcTo(c, s * 0.75f, -PI * 0.35f, PI * 1.45f, 24);
        dl->PathStroke(col, 0, th);
        ImVec2 e(c.x + cosf(-PI * 0.35f) * s * 0.75f, c.y + sinf(-PI * 0.35f) * s * 0.75f);
        dl->AddTriangleFilled(ImVec2(e.x - 3.5f, e.y - 2.5f), ImVec2(e.x + 3.5f, e.y - 3.f), ImVec2(e.x + 1.f, e.y + 3.5f), col);
        break; }
    case I_Close:
        dl->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, th);
        dl->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x - s, c.y + s), col, th);
        break;
    case I_Chevron:
        dl->AddLine(ImVec2(c.x - s * 0.35f, c.y - s * 0.7f), ImVec2(c.x + s * 0.35f, c.y), col, th);
        dl->AddLine(ImVec2(c.x + s * 0.35f, c.y), ImVec2(c.x - s * 0.35f, c.y + s * 0.7f), col, th);
        break;
    }
}

void ApplyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 14; s.ChildRounding = 9; s.FrameRounding = 6; s.PopupRounding = 8;
    s.ScrollbarRounding = 8; s.GrabRounding = 6;
    s.WindowBorderSize = 0; s.ChildBorderSize = 1; s.PopupBorderSize = 1; s.FrameBorderSize = 0;
    s.WindowPadding = ImVec2(0, 0); s.ItemSpacing = ImVec2(10, 10); s.ScrollbarSize = 4;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = kWin;
    c[ImGuiCol_ChildBg] = kCard;
    c[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.08f, 0.98f);
    c[ImGuiCol_Border] = kLine;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kDim;
    c[ImGuiCol_FrameBg] = kFrame;
    c[ImGuiCol_FrameBgHovered] = kFrame;
    c[ImGuiCol_FrameBgActive] = kFrame;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.08f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.16f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(1, 1, 1, 0.22f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(1, 1, 1, 0.18f);
    c[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0, 0);
}

static void DrawPill(ImDrawList* dl, ImVec2 a, float t, float hv) {
    const float w = 30, h = 16;
    ImVec2 b(a.x + w, a.y + h);
    Glow(dl, a, b, g_acc, h * 0.5f, t * 0.9f, 5, 1.4f);
    dl->AddRectFilled(a, b, Col(Mix(kFrame, ImVec4(0.17f, 0.17f, 0.19f, 1), hv)), h * 0.5f);
    if (t > 0.01f) AccFill(dl, a, b, h * 0.5f, t);
    dl->AddRect(a, b, Col(ImVec4(1, 1, 1, 0.06f * (1 - t))), h * 0.5f);
    float kx = a.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(kx, a.y + h * 0.5f), h * 0.5f - 3.f, Col(Mix(kDim, OnAcc(), t)), 20);
}

bool Toggle(const char* id, bool* v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, ImVec2(30, 16));
    if (pressed) *v = !*v;
    ImGui::PushID(id);
    float t = AnimS("t", *v ? 1.f : 0.f, 14.f);
    float hv = AnimS("h", ImGui::IsItemHovered() ? 1.f : 0.f);
    ImGui::PopID();
    DrawPill(ImGui::GetWindowDrawList(), p, t, hv);
    return pressed;
}

struct Ripple { ImVec2 pos; float t; };
static std::unordered_map<ImGuiID, Ripple> g_ripples;

bool Button(const char* label, ImVec2 size, bool primary) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* f = Fr();
    ImVec2 ts = TextSz(f, label);
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    if (size.y <= 0) size.y = 32.f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGuiID id = ImGui::GetID(label);
    bool pressed = ImGui::InvisibleButton(label, size);
    bool hov = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImGui::PushID(label);
    float hv = AnimS("h", hov ? 1.f : 0.f, 12.f);
    float pr = AnimS("p", held ? 1.f : 0.f, 22.f);
    ImGui::PopID();
    if (pressed) g_ripples[id] = { ImGui::GetIO().MousePos, 0.f };

    float sh = pr * 1.5f;
    ImVec2 a(p.x + sh, p.y + sh), b(p.x + size.x - sh, p.y + size.y - sh);
    if (primary) {
        Glow(dl, a, b, g_acc, 7.f, 0.5f + hv);
        AccFill(dl, a, b, 7.f);
        dl->AddRectFilled(a, b, Col(ImVec4(1, 1, 1, 0.12f * hv)), 7.f); 
    } else {
        Glow(dl, a, b, g_acc, 7.f, hv * 0.6f);
        dl->AddRectFilled(a, b, Col(Mix(kFrame, kHi, hv)), 7.f);
        dl->AddRect(a, b, Col(Mix(ImVec4(1, 1, 1, 0.07f), A(g_acc, 0.6f), hv)), 7.f);
    }
    auto it = g_ripples.find(id);
    if (it != g_ripples.end()) {
        Ripple& r = it->second;
        r.t += ImGui::GetIO().DeltaTime * 2.4f;
        if (r.t >= 1.f) g_ripples.erase(it);
        else {
            float e = 1.f - (1.f - r.t) * (1.f - r.t);
            dl->PushClipRect(a, b, true);
            ImVec4 rc = primary ? OnAcc() : g_acc;
            dl->AddCircleFilled(r.pos, e * size.x, Col(rc, 0.22f * (1.f - r.t)), 48);
            dl->PopClipRect();
        }
    }
    ImVec4 tc = primary ? OnAcc() : Mix(kSoft, kText, hv);
    Txt(dl, f, ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f), Col(tc), label);
    return pressed;
}

bool SearchBar(const char* id, char* buf, int buf_size, const char* hint) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(32, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f);
    ImGui::SetNextItemWidth(w);
    bool changed = ImGui::InputTextWithHint(id, hint, buf, buf_size);
    bool focus = ImGui::IsItemActive(), hov = ImGui::IsItemHovered();
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImGui::PopStyleVar(2);
    ImGui::PushID(id);
    float f = AnimS("f", focus ? 1.f : 0.f, 12.f);
    float hv = AnimS("h", hov ? 1.f : 0.f);
    ImGui::PopID();
    Glow(dl, mn, mx, g_acc, 7.f, f * 0.8f);
    dl->AddRect(mn, mx, Col(Mix(ImVec4(1, 1, 1, 0.06f + 0.05f * hv), A(g_acc, 0.7f), f)), 7.f);
    ImVec2 c(p.x + 16, (mn.y + mx.y) * 0.5f - 1);
    ImU32 ic = Col(Mix(kDim, g_acc, f));
    dl->AddCircle(c, 5.f, ic, 16, 1.6f);
    dl->AddLine(ImVec2(c.x + 3.7f, c.y + 3.7f), ImVec2(c.x + 7.f, c.y + 7.f), ic, 1.8f);
    return changed;
}

static std::unordered_map<ImGuiID, bool> g_hovCard;
static std::unordered_map<ImGuiID, int> g_cardKey;

bool BeginCard(const char* id, const char* title, const char* sub, bool* en, bool gear) {
    ImGui::PushID(id);
    ImGuiID hid = ImGui::GetID("##hov");
    float hv = Anim(hid, g_hovCard[hid] ? 1.f : 0.f, 10.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Mix(kCard, ImVec4(0.088f, 0.088f, 0.098f, 1), hv));
    ImGui::PushStyleColor(ImGuiCol_Border, Mix(ImVec4(1, 1, 1, 0.05f), A(g_acc, 0.28f), hv));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild("##card", ImVec2(ImGui::GetContentRegionAvail().x, 0),
                      ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 7));
    g_hovCard[hid] = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float hh = sub ? 38.f : 21.f;
    Txt(dl, Fb(), p, Col(kText), title);
    if (sub) Txt(dl, Fs(), ImVec2(p.x, p.y + 21), Col(kDim), sub);

    float rx = p.x + w;
    if (en) {
        ImGui::SetCursorScreenPos(ImVec2(rx - 30, p.y + (hh - 16) * 0.5f));
        Toggle("##en", en);
        rx -= 44;
    }
    if (gear) {
        ImVec2 gc(rx - 6, p.y + hh * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(gc.x - 8, gc.y - 8));
        if (ImGui::InvisibleButton("##gear", ImVec2(16, 16))) ImGui::OpenPopup("##gearpop");
        bool gh = ImGui::IsItemHovered();
        float gt = AnimS("gr", gh ? 1.f : 0.f);
        Icon(dl, I_Gear, gc, 6.f + gt, Col(Mix(kFaint, kText, gt)), 1.2f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
        if (ImGui::BeginPopup("##gearpop")) {
            static const char* binds[] = { "None", "Mouse 4", "Mouse 5", "Left Alt", "Left Shift" };
            static const char* modes[] = { "Toggle", "Hold", "Always" };
            int& b = g_cardKey[ImGui::GetID("bind")];
            int& m = g_cardKey[ImGui::GetID("mode")];
            ImGui::Dummy(ImVec2(220, 0));
            Txt(ImGui::GetWindowDrawList(), Fb(), ImGui::GetCursorScreenPos(), Col(kText), title);
            ImGui::Dummy(ImVec2(0, 14));
            RowCombo("Hotkey", &b, binds, 5);
            RowCombo("Mode", &m, modes, 3);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, hh));
    float a = en ? AnimS("##dim", *en ? 1.f : 0.45f, 10.f) : 1.f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * a);
    return true;
}
void EndCard() {
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopID();
}

static const float kRowH = 30.f;
struct Row { ImDrawList* dl; ImVec2 p; float w, x0, x1; };
static Row BeginRow(const char* label, float h = kRowH) {
    Row r;
    r.dl = ImGui::GetWindowDrawList();
    r.p = ImGui::GetCursorScreenPos();
    r.w = ImGui::GetContentRegionAvail().x;
    r.x0 = r.p.x + r.w * 0.47f;
    r.x1 = r.p.x + r.w;
    ImGui::PushID(label);
    float hv = AnimS("##lh", ImGui::IsMouseHoveringRect(r.p, ImVec2(r.x1, r.p.y + h)) ? 1.f : 0.f);
    Txt(r.dl, Fr(), ImVec2(r.p.x, r.p.y + (h - Fr()->FontSize) * 0.5f), Col(Mix(kSoft, kText, hv)), label);
    return r;
}
static void EndRow(const Row& r, float h = kRowH) {
    ImGui::SetCursorScreenPos(r.p);
    ImGui::Dummy(ImVec2(r.w, h));
    ImGui::PopID();
}

bool RowToggle(const char* label, bool* v) {
    Row r = BeginRow(label);
    ImGui::SetCursorScreenPos(r.p);
    bool pressed = ImGui::InvisibleButton("##row", ImVec2(r.w, kRowH));
    if (pressed) *v = !*v;
    float t = AnimS("t", *v ? 1.f : 0.f);
    float hv = AnimS("h", ImGui::IsItemHovered() ? 1.f : 0.f);
    DrawPill(r.dl, ImVec2(r.x1 - 30, r.p.y + (kRowH - 16) * 0.5f), t, hv);
    EndRow(r);
    return pressed;
}

bool RowCheck(const char* label, bool* v) {
    Row r = BeginRow(label);
    ImGui::SetCursorScreenPos(r.p);
    bool pressed = ImGui::InvisibleButton("##row", ImVec2(r.w, kRowH));
    if (pressed) *v = !*v;
    float t = AnimS("t", *v ? 1.f : 0.f, 16.f);
    float hv = AnimS("h", ImGui::IsItemHovered() ? 1.f : 0.f);
    const float s = 16;
    ImVec2 a(r.x1 - s, r.p.y + (kRowH - s) * 0.5f), b(r.x1, a.y + s);
    Glow(r.dl, a, b, g_acc, 4.f, t * 0.7f, 4);
    r.dl->AddRectFilled(a, b, Col(Mix(kFrame, ImVec4(0.17f, 0.17f, 0.19f, 1), hv)), 4.f);
    if (t > 0.01f) AccFill(r.dl, a, b, 4.f, t);
    r.dl->AddRect(a, b, Col(ImVec4(1, 1, 1, 0.08f * (1 - t))), 4.f);
    if (t > 0.05f) { 
        ImVec2 p0(a.x + 3.8f, a.y + 8.3f), p1(a.x + 6.8f, a.y + 11.2f), p2(a.x + 12.3f, a.y + 5.f);
        float d = Clamp01(t * 1.6f);
        ImVec2 pts[3] = { p0, p1, p1 };
        int n = 2;
        if (d < 0.4f) pts[1] = ImVec2(p0.x + (p1.x - p0.x) * d / 0.4f, p0.y + (p1.y - p0.y) * d / 0.4f);
        else { float k = (d - 0.4f) / 0.6f; pts[2] = ImVec2(p1.x + (p2.x - p1.x) * k, p1.y + (p2.y - p1.y) * k); n = 3; }
        r.dl->AddPolyline(pts, n, Col(OnAcc(), t), 0, 1.8f);
    }
    EndRow(r);
    return pressed;
}

static bool SliderCore(Row& r, float* v, float* v2, float mn, float mx, const char* valText) {
    ImGui::SetCursorScreenPos(ImVec2(r.x0, r.p.y));
    ImGui::InvisibleButton("##sl", ImVec2(r.x1 - r.x0, kRowH));
    bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    float tw = r.x1 - r.x0;
    float mt = Clamp01((ImGui::GetIO().MousePos.x - r.x0) / tw);
    static std::unordered_map<ImGuiID, int> handle;
    ImGuiID hid = ImGui::GetID("##hd");
    if (v2 && ImGui::IsItemActivated()) {
        float d1 = fabsf(mt - (*v - mn) / (mx - mn)), d2 = fabsf(mt - (*v2 - mn) / (mx - mn));
        handle[hid] = d2 < d1 ? 1 : 0;
    }
    bool changed = false;
    if (act) {
        float nv = mn + (mx - mn) * mt;
        float* tgt = (v2 && handle[hid] == 1) ? v2 : v;
        if (v2) { if (tgt == v) nv = Mn(nv, *v2); else nv = Mx(nv, *v); }
        if (nv != *tgt) { *tgt = nv; changed = true; }
    }
    float hv = AnimS("h", (hov || act) ? 1.f : 0.f);
    float t1 = AnimS("v1", (*v - mn) / (mx - mn), act ? 30.f : 12.f);
    float t0 = 0.f;
    if (v2) { t0 = t1; t1 = AnimS("v2", (*v2 - mn) / (mx - mn), act ? 30.f : 12.f); }

    ImVec2 vs = TextSz(Fs(), valText);
    Txt(r.dl, Fs(), ImVec2(r.x1 - vs.x, r.p.y - 1), Col(Mix(kSoft, kText, hv)), valText);
    float ty = r.p.y + 21.f;
    ImVec2 a(r.x0, ty - 1.5f), b(r.x1, ty + 1.5f);
    r.dl->AddRectFilled(a, b, Col(kFrame), 2.f);
    ImVec2 fa(r.x0 + tw * t0, a.y), fb(r.x0 + tw * t1, b.y);
    Glow(r.dl, fa, fb, g_acc, 2.f, 0.4f + hv * 0.6f, 4, 1.3f);
    { int v0 = r.dl->VtxBuffer.Size; r.dl->AddRectFilled(fa, fb, IM_COL32_WHITE, 2.f); RecolorAcc(r.dl, v0, a, b, 1.f, false); }
    float kr = 4.5f + hv * 1.5f;
    auto knob = [&](float t) {
        ImVec2 c(r.x0 + tw * t, ty);
        r.dl->AddCircleFilled(c, kr + 4.f * hv, Col(g_acc, 0.15f * hv), 24);
        r.dl->AddCircleFilled(c, kr, Col(ImVec4(1, 1, 1, 1)), 24);
    };
    if (v2) knob(t0);
    knob(t1);
    return changed;
}

bool RowSlider(const char* label, float* v, float mn, float mx, const char* fmt) {
    Row r = BeginRow(label);
    char buf[64]; snprintf(buf, sizeof(buf), fmt, *v);
    bool c = SliderCore(r, v, nullptr, mn, mx, buf);
    EndRow(r);
    return c;
}

bool RowRange(const char* label, float* lo, float* hi, float mn, float mx, const char* fmt) {
    Row r = BeginRow(label);
    char buf[64]; snprintf(buf, sizeof(buf), fmt, *lo, *hi);
    bool c = SliderCore(r, lo, hi, mn, mx, buf);
    EndRow(r);
    return c;
}

static void PushPopupStyle(float alpha) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.08f));
}
static void PopPopupStyle() { ImGui::PopStyleColor(); ImGui::PopStyleVar(3); }

bool RowCombo(const char* label, int* idx, const char* const* items, int count) {
    Row r = BeginRow(label);
    const float bh = 25.f;
    ImVec2 a(r.x0, r.p.y + (kRowH - bh) * 0.5f), b(r.x1, a.y + bh);
    ImGui::SetCursorScreenPos(a);
    ImGuiID popA = ImGui::GetID("##pa");
    if (ImGui::InvisibleButton("##cb", ImVec2(b.x - a.x, bh))) { ImGui::OpenPopup("##pop"); g_anim[popA] = 0.f; }
    bool hov = ImGui::IsItemHovered();
    bool open = ImGui::IsPopupOpen("##pop");
    float hv = AnimS("h", (hov || open) ? 1.f : 0.f);
    float ot = AnimS("o", open ? 1.f : 0.f, 16.f);

    r.dl->AddRectFilled(a, b, Col(Mix(kFrame, kHi, hv)), 6.f);
    r.dl->AddRect(a, b, Col(Mix(ImVec4(1, 1, 1, 0.05f), A(g_acc, 0.5f), ot)), 6.f);
    Txt(r.dl, Fs(), ImVec2(a.x + 9, a.y + (bh - Fs()->FontSize) * 0.5f), Col(kSoft), items[*idx]);
    
    ImVec2 cc(b.x - 11, a.y + bh * 0.5f);
    float ang = 1.5708f * (1.f - 2.f * ot);
    float ca = cosf(ang), sa = sinf(ang);
    auto rot = [&](float x, float y) { return ImVec2(cc.x + x * ca - y * sa, cc.y + x * sa + y * ca); };
    ImVec2 ch[3] = { rot(-1.8f, -3.5f), rot(1.8f, 0.f), rot(-1.8f, 3.5f) };
    r.dl->AddPolyline(ch, 3, Col(Mix(kDim, kText, hv)), 0, 1.4f);

    bool changed = false;
    float pa = Anim(popA, open ? 1.f : 0.f, 16.f);
    ImGui::SetNextWindowPos(ImVec2(a.x, b.y + 4 + (1.f - pa) * -6.f));
    ImGui::SetNextWindowSize(ImVec2(b.x - a.x, 0));
    PushPopupStyle(pa);
    if (ImGui::BeginPopup("##pop")) {
        ImDrawList* pdl = ImGui::GetWindowDrawList();
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            ImVec2 ip = ImGui::GetCursorScreenPos();
            float iw = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##it", ImVec2(iw, 24))) { *idx = i; changed = true; ImGui::CloseCurrentPopup(); }
            float ih = AnimS("ih", ImGui::IsItemHovered() ? 1.f : 0.f, 18.f);
            bool sel = (*idx == i);
            pdl->AddRectFilled(ip, ImVec2(ip.x + iw, ip.y + 24), Col(kHi, ih), 5.f);
            if (sel) pdl->AddCircleFilled(ImVec2(ip.x + iw - 10, ip.y + 12), 2.5f, Col(g_acc), 12);
            Txt(pdl, Fs(), ImVec2(ip.x + 8 + ih * 3.f, ip.y + (24 - Fs()->FontSize) * 0.5f), Col(sel ? kText : Mix(kDim, kText, ih)), items[i]);
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    PopPopupStyle();
    EndRow(r);
    return changed;
}

bool RowInput(const char* label, char* buf, int buf_size, const char* hint) {
    Row r = BeginRow(label);
    ImGui::PushFont(Fs());
    float bh = Fs()->FontSize + 9.f;
    ImGui::SetCursorScreenPos(ImVec2(r.x0, r.p.y + (kRowH - bh) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9, 4.5f));
    ImGui::SetNextItemWidth(r.x1 - r.x0);
    bool c = ImGui::InputTextWithHint("##in", hint, buf, buf_size);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    bool f = ImGui::IsItemActive(), h = ImGui::IsItemHovered();
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    float ft = AnimS("f", f ? 1.f : 0.f, 12.f), ht = AnimS("h", h ? 1.f : 0.f);
    Glow(r.dl, mn, mx, g_acc, 6.f, ft * 0.6f, 4);
    r.dl->AddRect(mn, mx, Col(Mix(ImVec4(1, 1, 1, 0.05f + 0.05f * ht), A(g_acc, 0.6f), ft)), 6.f);
    EndRow(r);
    return c;
}

bool RowColor(const char* label, float col[3]) {
    Row r = BeginRow(label);
    ImGui::SetCursorScreenPos(ImVec2(r.x0, r.p.y));
    ImGuiID popA = ImGui::GetID("##pa");
    if (ImGui::InvisibleButton("##sw", ImVec2(r.x1 - r.x0, kRowH))) { ImGui::OpenPopup("##cp"); g_anim[popA] = 0.f; }
    float hv = AnimS("h", ImGui::IsItemHovered() ? 1.f : 0.f);
    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", (int)(col[0] * 255 + 0.5f), (int)(col[1] * 255 + 0.5f), (int)(col[2] * 255 + 0.5f));
    ImVec2 hs = TextSz(Fs(), hex);
    ImVec2 c(r.x1 - 8, r.p.y + kRowH * 0.5f);
    Txt(r.dl, Fs(), ImVec2(c.x - 16 - hs.x, c.y - hs.y * 0.5f), Col(Mix(kSoft, kText, hv)), hex);
    ImVec4 cv(col[0], col[1], col[2], 1.f);
    Glow(r.dl, ImVec2(c.x - 7, c.y - 7), ImVec2(c.x + 7, c.y + 7), cv, 7, 0.6f + hv, 4);
    r.dl->AddCircleFilled(c, 7.f + hv, Col(cv), 24);
    r.dl->AddCircle(c, 7.f + hv, Col(ImVec4(1, 1, 1, 0.15f)), 24, 1.f);

    bool changed = false;
    float pa = Anim(popA, ImGui::IsPopupOpen("##cp") ? 1.f : 0.f, 16.f);
    PushPopupStyle(pa);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    if (ImGui::BeginPopup("##cp")) {
        ImGui::SetNextItemWidth(190);
        changed = ImGui::ColorPicker3("##pick", col, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    PopPopupStyle();
    EndRow(r);
    return changed;
}

struct ToastItem { std::string text; float t; };
static std::vector<ToastItem> g_toasts;
void Toast(const char* text) { g_toasts.push_back({ text, 0.f }); }

void DrawToasts() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float y = ds.y - 24.f;
    const float life = 2.8f;
    for (int i = (int)g_toasts.size() - 1; i >= 0; --i) {
        ToastItem& t = g_toasts[i];
        t.t += ImGui::GetIO().DeltaTime;
        if (t.t > life) { g_toasts.erase(g_toasts.begin() + i); continue; }
        float in = Clamp01(t.t / 0.28f), out = Clamp01((life - t.t) / 0.3f);
        float e = 1.f - powf(1.f - in, 3.f), a = Mn(in, out);
        ImVec2 ts = TextSz(Fr(), t.text.c_str());
        float w = ts.x + 52, h = 42;
        float x = ds.x - 24 - w + (1.f - e) * 70.f;
        ImVec2 pa(x, y - h), pb(x + w, y);
        auto C = [&](ImVec4 c, float k = 1.f) { return ImGui::ColorConvertFloat4ToU32(A(c, a * k)); };
        for (int g = 1; g <= 6; ++g)
            dl->AddRectFilled(ImVec2(pa.x - g * 2, pa.y - g * 2), ImVec2(pb.x + g * 2, pb.y + g * 2), C(g_acc, 0.05f * (1 - g / 7.f)), 10.f + g * 2);
        dl->AddRectFilled(pa, pb, C(ImVec4(0.07f, 0.07f, 0.08f, 0.98f)), 10.f);
        dl->AddRect(pa, pb, C(A(g_acc, 0.35f)), 10.f);
        dl->AddCircleFilled(ImVec2(pa.x + 18, pa.y + h * 0.5f), 4.f, C(g_acc));
        dl->AddText(Fr(), Fr()->FontSize, ImVec2(pa.x + 32, pa.y + (h - ts.y) * 0.5f), C(kText), t.text.c_str());
        float prog = 1.f - t.t / life;
        dl->AddRectFilled(ImVec2(pa.x + 10, pb.y - 2), ImVec2(pa.x + 10 + (w - 20) * prog, pb.y), C(g_acc, 0.7f), 1.f);
        y -= (h + 10) * e;
    }
}

struct Page { std::string label; int icon; std::function<void()> render; bool isSection; };
static std::vector<Page> g_pages;
static std::string g_brand = "Cortex";
static int g_page = -1; 

void SetBrand(const char* name) { g_brand = name; }
void AddSection(const char* name) { g_pages.push_back({ name, -1, nullptr, true }); }
void AddPage(const char* label, int icon, std::function<void()> render) {
    g_pages.push_back({ label, icon, std::move(render), false });
    if (g_page < 0) g_page = (int)g_pages.size() - 1;
}

bool BeginColumns(int count) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, 0));
    bool ok = ImGui::BeginTable("##cols", count, ImGuiTableFlags_SizingStretchSame);
    if (ok) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); }
    else ImGui::PopStyleVar();
    return ok;
}
void NextColumn() { ImGui::TableNextColumn(); }
void EndColumns() { ImGui::EndTable(); ImGui::PopStyleVar(); }

void TextDim(const char* text) {
    Txt(ImGui::GetWindowDrawList(), Fs(), ImGui::GetCursorScreenPos(), Col(kDim), text);
    ImGui::Dummy(TextSz(Fs(), text));
}

static void EmptyState(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + size.x * 0.5f, p.y + size.y * 0.42f);
    Logo(dl, ImVec2(c.x, c.y - 34), 16.f, 0.9f);
    const char* t1 = "Nothing here yet";
    const char* t2 = "Register pages in src/pages.cpp with cx::AddPage()";
    ImVec2 s1 = TextSz(Fb(), t1), s2 = TextSz(Fs(), t2);
    Txt(dl, Fb(), ImVec2(c.x - s1.x * 0.5f, c.y), Col(kText), t1);
    Txt(dl, Fs(), ImVec2(c.x - s2.x * 0.5f, c.y + 24), Col(kDim), t2);
}

void RenderMenu(bool* open) {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;
    g_acc  = Mix(g_acc,  g_themes[g_theme].acc,  Clamp01(dt * 7.f));
    g_acc2 = Mix(g_acc2, g_themes[g_theme].acc2, Clamp01(dt * 7.f));
    float vis = Anim(HashID("##menu_vis"), *open ? 1.f : 0.f, 11.f);
    if (vis < 0.01f) { DrawToasts(); return; }

    const ImVec2 size(1100, 700);
    const float topH = 52.f, sideW = 220.f;
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, vis);
    ImGui::Begin("##cortex", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), we(wp.x + size.x, wp.y + size.y);
    float time = (float)ImGui::GetTime();

    
    
    auto flick = [&](float phase) { return g_flicker ? 0.5f + 0.5f * sinf(time * 2.1f + phase) * (0.75f + 0.25f * sinf(time * 5.3f + phase * 1.7f)) : 0.35f; };
    
    static float outlinePhase = 0.f;
    outlinePhase += dt * g_outlineSpeed * 1.6f;
    ImVec2 wc((wp.x + we.x) * 0.5f, (wp.y + we.y) * 0.5f);
    if (g_outerGlow) {
        dl->PushClipRectFullScreen();
        int v0 = dl->VtxBuffer.Size;
        for (int i = 1; i <= 14; ++i) {
            float s = i * 2.6f;
            float f = 0.8f + 0.4f * flick(i * 0.45f);
            dl->AddRect(ImVec2(wp.x - s, wp.y - s), ImVec2(we.x + s, we.y + s), Col(ImVec4(1, 1, 1, 1), 0.055f * f * g_glowMul * (1.f - i / 15.f)), 14.f + s, 0, 2.8f);
        }
        RecolorConic(dl, v0, wc, outlinePhase);
        dl->PopClipRect();
    }
    dl->AddRectFilled(wp, we, Col(kWin), 14.f);
    Grad(dl, wp, ImVec2(we.x, wp.y + 180), Col(ImVec4(1, 1, 1, 0.035f)), Col(ImVec4(1, 1, 1, 0.f)), 14.f, true, ImDrawFlags_RoundCornersTop);
    dl->PushClipRect(wp, we, true);
    if (g_sweep) {
        float sx = fmodf(time * 90.f, size.x * 2.2f) - size.x * 0.6f;
        ImVec2 q[4] = { {wp.x + sx, wp.y}, {wp.x + sx + 120, wp.y}, {wp.x + sx - 160, we.y}, {wp.x + sx - 280, we.y} };
        dl->AddConvexPolyFilled(q, 4, Col(ImVec4(1, 1, 1, 0.018f)));
    }
    dl->PopClipRect();
    dl->AddRectFilled(ImVec2(wp.x, wp.y + topH), ImVec2(wp.x + sideW, we.y), Col(kSide), 14.f, ImDrawFlags_RoundCornersBottomLeft);
    dl->AddLine(ImVec2(wp.x, wp.y + topH), ImVec2(we.x, wp.y + topH), Col(kLine));
    dl->AddLine(ImVec2(wp.x + sideW, wp.y + topH), ImVec2(wp.x + sideW, we.y), Col(kLine));
    
    {
        int v0 = dl->VtxBuffer.Size;
        dl->AddRect(ImVec2(wp.x - 1, wp.y - 1), ImVec2(we.x + 1, we.y + 1), Col(ImVec4(1, 1, 1, 1), 0.22f * g_glowMul), 15.f, 0, 4.f);
        dl->AddRect(wp, we, Col(ImVec4(1, 1, 1, 1), 0.95f), 14.f, 0, 1.6f);
        RecolorConic(dl, v0, wc, outlinePhase, 0.9f);
    }

    
    {
        const char* brand = g_brand.c_str();
        ImVec2 bs = TextSz(Fb(), brand);
        float bx = wp.x + size.x * 0.5f - (bs.x + 26) * 0.5f, cy = wp.y + topH * 0.5f;
        Logo(dl, ImVec2(bx + 8, cy), 8.f);
        Txt(dl, Fb(), ImVec2(bx + 26, cy - bs.y * 0.5f), Col(kText), brand);
    }
    {
        ImVec2 c(we.x - 24, wp.y + topH * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(c.x - 10, c.y - 10));
        if (ImGui::InvisibleButton("##close", ImVec2(20, 20))) *open = false;
        float h = AnimS("##closeh", ImGui::IsItemHovered() ? 1.f : 0.f);
        Icon(dl, I_Close, c, 4.f + h, Col(Mix(kDim, ImVec4(1, 0.4f, 0.4f, 1), h)), 1.4f);
    }

    
    static float g_sideScroll = 0.f;
    const float sideTop    = wp.y + topH + 14;
    const float sideBottom = we.y - 60.f; 
    const float sideVisible = sideBottom - sideTop;

    
    float contentH = 0.f;
    for (int i = 0; i < (int)g_pages.size(); ++i) {
        if (g_pages[i].isSection) { if (i > 0) contentH += 10; contentH += 24; }
        else contentH += 36;
    }
    float maxScroll = Mx(0.f, contentH - sideVisible);
    g_sideScroll = Clamp01(g_sideScroll / (maxScroll > 0.f ? maxScroll : 1.f)) * maxScroll;

    
    ImVec2 mpos = ImGui::GetIO().MousePos;
    bool overSide = mpos.x >= wp.x && mpos.x <= wp.x + sideW && mpos.y >= sideTop && mpos.y <= sideBottom;
    if (overSide) {
        g_sideScroll -= ImGui::GetIO().MouseWheel * 36.f;
        g_sideScroll = Mx(0.f, Mn(g_sideScroll, maxScroll));
    }

    
    if (maxScroll > 0.f) {
        float thumbH = Mn(sideVisible, sideVisible * sideVisible / contentH);
        float thumbY = sideTop + (g_sideScroll / maxScroll) * (sideVisible - thumbH);
        ImVec2 ta(wp.x + sideW - 5, thumbY + 2), tb(wp.x + sideW - 3, thumbY + thumbH - 2);
        dl->AddRectFilled(ta, tb, Col(kFaint, 0.6f), 2.f);
    }

    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    dl->PushClipRect(ImVec2(wp.x, sideTop), ImVec2(wp.x + sideW, sideBottom), true);
    float y = sideTop - g_sideScroll;
    float selY = 0;
    auto section = [&](const char* name) {
        Txt(dl, Fs(), ImVec2(wp.x + 22, y + 4), Col(kFaint), name);
        y += 24;
    };
    auto item = [&](const Page& it, int idx) {
        ImVec2 a(wp.x + 10, y), b(wp.x + sideW - 10, y + 34);
        bool visible = (b.y > sideTop) && (a.y < sideBottom);
        ImGui::PushID(idx);
        if (visible) {
            ImGui::SetCursorScreenPos(a);
            if (ImGui::InvisibleButton("##nav", ImVec2(b.x - a.x, 34))) g_page = idx;
        }
        float h = AnimS("h", (visible && ImGui::IsItemHovered()) ? 1.f : 0.f);
        float s = AnimS("s", g_page == idx ? 1.f : 0.f, 12.f);
        ImGui::PopID();
        if (g_page == idx) selY = y;
        if (visible) {
            if (h > 0.01f) dl->AddRectFilled(a, b, Col(ImVec4(1, 1, 1, 0.025f * h)), 7.f);
            float tx = a.x + 14;
            if (it.icon >= 0) { Icon(dl, it.icon, ImVec2(a.x + 18, y + 17), 7.5f, Col(Mix(Mix(kDim, kSoft, h), g_acc, s)), 1.4f); tx = a.x + 36; }
            Txt(dl, Fr(), ImVec2(tx + s * 2.f, y + 17 - Fr()->FontSize * 0.5f), Col(Mix(Mix(kDim, kSoft, h), kText, s)), it.label.c_str());
            if (s > 0.02f) Icon(dl, I_Chevron, ImVec2(b.x - 14 - (1 - s) * 6, y + 17), 6.f, Col(kDim, s), 1.4f);
        }
        y += 36;
    };
    for (int i = 0; i < (int)g_pages.size(); ++i) {
        if (g_pages[i].isSection) { if (i > 0) y += 10; section(g_pages[i].label.c_str()); }
        else item(g_pages[i], i);
    }
    dl->PopClipRect();
    
    dl->ChannelsSetCurrent(0);
    float hy = Anim(ImGui::GetID("##selY"), selY, 16.f);
    if (g_page >= 0 && hy >= sideTop - 4 && hy <= sideBottom - 4) {
        dl->PushClipRect(ImVec2(wp.x, sideTop), ImVec2(wp.x + sideW, sideBottom), true);
        ImVec2 a(wp.x + 10, hy), b(wp.x + sideW - 10, hy + 34);
        dl->AddRectFilled(a, b, Col(kHi), 7.f);
        dl->AddRect(a, b, Col(ImVec4(1, 1, 1, 0.05f)), 7.f);
        Glow(dl, ImVec2(a.x - 1, a.y + 8), ImVec2(a.x + 1.5f, b.y - 8), g_acc, 1.f, 1.f, 4, 1.5f);
        AccFill(dl, ImVec2(a.x - 1, a.y + 8), ImVec2(a.x + 1.5f, b.y - 8), 1.f, 1.f, true);
        dl->PopClipRect();
    }
    dl->ChannelsMerge();

    
    {
        ImVec2 a(wp.x + 10, we.y - 50), b(wp.x + sideW - 10, we.y - 12);
        ImGui::SetCursorScreenPos(a);
        ImGuiID paId = ImGui::GetID("##setpa");
        if (ImGui::InvisibleButton("##settings", ImVec2(b.x - a.x, b.y - a.y))) { ImGui::OpenPopup("##setpop"); g_anim[paId] = 0.f; }
        bool popOpen = ImGui::IsPopupOpen("##setpop");
        float h = AnimS("##seth", (ImGui::IsItemHovered() || popOpen) ? 1.f : 0.f);
        float spin = AnimS("##setspin", popOpen ? 1.f : 0.f, 7.f);

        Glow(dl, a, b, Mix(g_acc, g_acc2, 0.5f), 8.f, h * 0.6f);
        dl->AddRectFilled(a, b, Col(Mix(kCard, kHi, h)), 8.f);
        dl->AddRect(a, b, Col(Mix(ImVec4(1, 1, 1, 0.06f), A(g_acc, 0.55f), h)), 8.f);
        g_iconRot = spin * 3.14159265f * 0.75f;
        Icon(dl, I_Gear, ImVec2(a.x + 20, (a.y + b.y) * 0.5f), 7.5f, Col(Mix(kSoft, g_acc, Mx(h, spin))), 1.4f);
        g_iconRot = 0.f;
        Txt(dl, Fr(), ImVec2(a.x + 38, (a.y + b.y) * 0.5f - Fr()->FontSize * 0.5f), Col(Mix(kSoft, kText, h)), "Settings");
        
        ImVec2 ca(b.x - 40, (a.y + b.y) * 0.5f - 5), cb(b.x - 12, (a.y + b.y) * 0.5f + 5);
        AccFill(dl, ca, cb, 5.f);

        float pa = Anim(paId, popOpen ? 1.f : 0.f, 14.f);
        ImGui::SetNextWindowPos(ImVec2(a.x, a.y - 8 + (1.f - pa) * 14.f), ImGuiCond_Always, ImVec2(0.f, 1.f));
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        PushPopupStyle(pa);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 7));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f);
        if (ImGui::BeginPopup("##setpop")) {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            Txt(pdl, Fb(), p, Col(kText), "Appearance");
            Txt(pdl, Fs(), ImVec2(p.x, p.y + 21), Col(kDim), "Theme, effects and animation.");
            ImGui::Dummy(ImVec2(w, 40));

            
            const float gap = 8.f, sw = (w - gap * 2) / 3.f, shh = 46.f;
            for (int i = 0; i < kThemeCount; ++i) {
                ImGui::PushID(200 + i);
                ImVec2 s0 = ImGui::GetCursorScreenPos();
                if (i % 3) { s0.x = p.x + (i % 3) * (sw + gap); ImGui::SetCursorScreenPos(s0); }
                if (ImGui::InvisibleButton("##sw", ImVec2(sw, shh))) g_theme = i;
                float th = AnimS("h", ImGui::IsItemHovered() ? 1.f : 0.f);
                float ts = AnimS("s", g_theme == i ? 1.f : 0.f);
                ImVec2 s1(s0.x + sw, s0.y + shh);
                pdl->AddRectFilled(s0, s1, Col(Mix(kCard, kHi, th)), 8.f);
                pdl->AddRect(s0, s1, Col(Mix(ImVec4(1, 1, 1, 0.06f), A(g_themes[i].acc, 0.9f), ts)), 8.f, 0, 1.f + ts * 0.6f);
                ImVec2 g0(s0.x + 8, s0.y + 8), g1(s1.x - 8, s0.y + 22 + th * 1.5f);
                Glow(pdl, g0, g1, g_themes[i].acc, 5.f, 0.4f + ts * 0.6f, 4);
                Grad(pdl, g0, g1, Col(g_themes[i].acc), Col(g_themes[i].acc2), 5.f);
                ImVec2 ns = TextSz(Fs(), g_themes[i].name);
                Txt(pdl, Fs(), ImVec2(s0.x + (sw - ns.x) * 0.5f, s1.y - ns.y - 5), Col(Mix(kDim, kText, Mx(th, ts))), g_themes[i].name);
                ImGui::PopID();
                if (i % 3 != 2) ImGui::SameLine(0, 0);
            }
            if (RowColor("Custom start", &g_themes[5].acc.x)) g_theme = 5;
            if (RowColor("Custom end", &g_themes[5].acc2.x)) g_theme = 5;

            ImVec2 sp = ImGui::GetCursorScreenPos();
            pdl->AddLine(ImVec2(sp.x, sp.y + 2), ImVec2(sp.x + w, sp.y + 2), Col(kLine));
            ImGui::Dummy(ImVec2(w, 4));
            RowToggle("Outer glow", &g_outerGlow);
            RowToggle("Fire flicker", &g_flicker);
            RowToggle("Light sweep", &g_sweep);
            RowToggle("Watermark", &g_watermark);
            RowSlider("Glow strength", &g_glowMul, 0.f, 2.f, "%.1fx");
            RowSlider("Animation speed", &g_animMul, 0.3f, 2.f, "%.1fx");
            RowSlider("Outline speed", &g_outlineSpeed, 0.f, 3.f, "%.1fx");
            ImGui::Dummy(ImVec2(0, 2));
            if (Button("Reset to defaults")) {
                g_theme = 0; g_glowMul = 1.f; g_animMul = 1.f; g_outlineSpeed = 1.f;
                g_sweep = g_outerGlow = g_flicker = g_watermark = true;
                Toast("Appearance reset");
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(3);
        PopPopupStyle();
    }

    
    static int lastPage = -1; static float pageT = 1.f;
    if (lastPage != g_page) { lastPage = g_page; pageT = 0.f; }
    pageT = Mn(1.f, pageT + dt * 4.5f);
    float pe = 1.f - powf(1.f - pageT, 3.f);

    ImGui::SetCursorScreenPos(ImVec2(wp.x + sideW + 12, wp.y + topH + 12 + (1.f - pe) * 14.f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, vis * pe);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(5, 0));
    ImGui::BeginChild("##page", ImVec2(size.x - sideW - 24, size.y - topH - 24), 0, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    if (g_page >= 0 && g_page < (int)g_pages.size() && g_pages[g_page].render) {
        ImGui::PushID(g_page);
        g_pages[g_page].render();
        ImGui::PopID();
    } else {
        EmptyState(ImGui::GetContentRegionAvail());
    }
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleVar();
    DrawToasts();
}

void DrawWatermark(bool visible) {
    float a = Anim(HashID("##wm"), (visible && g_watermark) ? 1.f : 0.f, 8.f);
    if (a < 0.01f) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto C = [&](ImVec4 c, float k = 1.f) { return ImGui::ColorConvertFloat4ToU32(A(c, a * k)); };

    char right[64];
    time_t now = time(nullptr);
    tm lt{};
    localtime_s(&lt, &now);
    snprintf(right, sizeof(right), "%.0f fps   %02d:%02d", ImGui::GetIO().Framerate, lt.tm_hour, lt.tm_min);
    const char* brand = "Cortex";
    ImVec2 bs = TextSz(Fb(), brand), rs = TextSz(Fs(), right);
    float x = 16.f + (1.f - a) * -20.f, y = 16.f, h = 30.f;
    float w = 14 + 18 + 6 + bs.x + 14 + 1 + 12 + rs.x + 14;
    ImVec2 pa(x, y), pb(x + w, y + h);

    for (int g = 1; g <= 6; ++g)
        dl->AddRectFilled(ImVec2(pa.x - g * 2, pa.y - g * 2), ImVec2(pb.x + g * 2, pb.y + g * 2), C(g_acc, 0.045f * (1 - g / 7.f)), 9.f + g * 2);
    dl->AddRectFilled(pa, pb, C(ImVec4(0.05f, 0.05f, 0.06f, 0.92f)), 9.f);
    dl->AddRect(pa, pb, C(A(g_acc, 0.4f)), 9.f);

    
    float bx = pa.x + 14, cy = pa.y + h * 0.5f;
    Logo(dl, ImVec2(bx + 7, cy), 7.f, a);
    float tx = bx + 24;
    dl->AddText(Fb(), Fb()->FontSize, ImVec2(tx, cy - bs.y * 0.5f), C(kText), brand);
    tx += bs.x + 14;
    dl->AddLine(ImVec2(tx, pa.y + 8), ImVec2(tx, pb.y - 8), C(ImVec4(1, 1, 1, 0.12f)));
    tx += 13;
    dl->AddText(Fs(), Fs()->FontSize, ImVec2(tx, cy - rs.y * 0.5f), C(kSoft), right);
    
    const char* hint = "INSERT  menu     END  exit";
    dl->AddText(Fs(), Fs()->FontSize, ImVec2(pa.x + 4, pb.y + 8), C(kDim, 0.8f), hint);
}

} 
