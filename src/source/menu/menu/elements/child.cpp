#include "../settings/functions.h"
#include <unordered_map>
#include <vector>
#include <string>

struct subtab {
    std::string name;
    bool open = false;
};

static std::unordered_map<ImGuiID, int> child_subtab_map;
static std::unordered_map<ImGuiID, std::vector<subtab>> subtab_list_map;

int c_gui::get_child_subtab(ImGuiID id) {
    return child_subtab_map[id];
}

void set_child_subtab(ImGuiID id, int v) {
    child_subtab_map[id] = v;
}

void set_child_subtabs(ImGuiID id, const std::vector<std::string>& names) {
    auto& tabs = subtab_list_map[id];
    if (tabs.size() != names.size()) {
        tabs.clear();
        for (auto& n : names)
            tabs.push_back({ n, false });
        if (!tabs.empty())
            tabs[0].open = true;
    }
}

void draw_child_subtabs(ImGuiWindow* parent_window, ImGuiID id, const ImVec2& size_arg) {
    auto& tabs = subtab_list_map[id];
    if (tabs.empty()) return;

    int active = c_gui::get_child_subtab(id);
    if (active < 0 || active >= static_cast<int>(tabs.size()))
        active = 0;

    int count = static_cast<int>(tabs.size());
    float tab_h = var->window.titlebar + 4.f;

    float tab_w = size_arg.x / count;
    ImVec2 base = parent_window->DC.CursorPos;

    for (int i = 0; i < count; ++i) {
        ImVec2 p_min = base + ImVec2(tab_w * i, 0.f);
        ImVec2 p_max = p_min + ImVec2(tab_w, tab_h);

        if (IsMouseHoveringRect(p_min, p_max, false) && (IsMouseClicked(0) || IsMouseReleased(0))) {
            active = i;
        }

        bool selected = (i == active);
        bool hovered = IsMouseHoveringRect(p_min, p_max, false);

        if (selected || hovered)
            draw->fade_rect_filled(parent_window->DrawList, p_min, p_max, draw->get_clr(clr->window.background_two), draw->get_clr(clr->window.background_one), fade_direction::vertically);
        else
            draw->fade_rect_filled(parent_window->DrawList, p_min, p_max, draw->get_clr(clr->window.background_one), draw->get_clr(clr->window.background_two), fade_direction::vertically);

        draw->rect(parent_window->DrawList, p_min, p_max, draw->get_clr(clr->window.stroke), 0.f);

        if (selected)
             draw->line(parent_window->DrawList, ImVec2(p_min.x + 1, p_max.y - 1), p_max - ImVec2(1, 1), draw->get_clr(clr->window.background_one));

        draw->text_clipped_outline(parent_window->DrawList, var->font.tahoma, p_min, p_max, selected ? draw->get_clr(clr->accent) : draw->get_clr(clr->widgets.text_inactive), tabs[i].name.c_str(), NULL, NULL, ImVec2(0.5f, 0.5f));
        tabs[i].open = selected;
    }

    draw->line(parent_window->DrawList, base + ImVec2(2, 2), base + ImVec2(size_arg.x - 2, 2), draw->get_clr(clr->accent));

    set_child_subtab(id, active);
}

void c_gui::begin_multi_subtab(std::string_view name, int x, int y, int tab_count, const ImVec2& size, const std::vector<std::string>& tab_names) {
    ImGuiID id = GetCurrentWindow()->GetID(name.data());
    set_child_subtabs(id, tab_names);

    gui->push_style_var(ImGuiStyleVar_WindowPadding, elements->widgets.padding);
    bool begin_child_ex(const char* name, ImGuiID id, int x, int y, const ImVec2& size_arg, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags, bool is_unsafe = false);
    begin_child_ex(name.data(), id, x, y, size, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoMove);
    gui->push_style_var(ImGuiStyleVar_ItemSpacing, elements->widgets.spacing);
}

bool c_gui::is_subtab_open(std::string_view name, int tab_index) {
    ImGuiID id = GetCurrentWindow()->GetID(name.data());
    auto& tabs = subtab_list_map[id];
    if (tab_index < 0 || tab_index >= static_cast<int>(tabs.size())) return false;
    return tabs[tab_index].open;
}

bool begin_child_ex(const char* name, ImGuiID id, int x, int y, const ImVec2& size_arg, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags, bool is_unsafe)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* parent_window = g.CurrentWindow;
    IM_ASSERT(id != 0);

    const ImGuiChildFlags ImGuiChildFlags_SupportedMask_ = ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_FrameStyle;
    IM_UNUSED(ImGuiChildFlags_SupportedMask_);
    IM_ASSERT((child_flags & ~ImGuiChildFlags_SupportedMask_) == 0 && "Illegal ImGuiChildFlags value. Did you pass ImGuiWindowFlags values instead of ImGuiChildFlags?");
    IM_ASSERT((window_flags & ImGuiWindowFlags_AlwaysAutoResize) == 0 && "Cannot specify ImGuiWindowFlags_AlwaysAutoResize for BeginChild(). Use ImGuiChildFlags_AlwaysAutoResize!");
    if (child_flags & ImGuiChildFlags_AlwaysAutoResize)
    {
        IM_ASSERT((child_flags & (ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY)) == 0 && "Cannot use ImGuiChildFlags_ResizeX or ImGuiChildFlags_ResizeY with ImGuiChildFlags_AlwaysAutoResize!");
        IM_ASSERT((child_flags & (ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) != 0 && "Must use ImGuiChildFlags_AutoResizeX or ImGuiChildFlags_AutoResizeY with ImGuiChildFlags_AlwaysAutoResize!");
    }
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    if (window_flags & ImGuiWindowFlags_AlwaysUseWindowPadding)
        child_flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
#endif
    if (child_flags & ImGuiChildFlags_AutoResizeX)
        child_flags &= ~ImGuiChildFlags_ResizeX;
    if (child_flags & ImGuiChildFlags_AutoResizeY)
        child_flags &= ~ImGuiChildFlags_ResizeY;

    window_flags |= ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoTitleBar;
    window_flags |= (parent_window->Flags & ImGuiWindowFlags_NoMove);
    if (child_flags & (ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize))
        window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
    if ((child_flags & (ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY)) == 0)
        window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (child_flags & ImGuiChildFlags_FrameStyle)
    {
        PushStyleColor(ImGuiCol_ChildBg, g.Style.Colors[ImGuiCol_FrameBg]);
        PushStyleVar(ImGuiStyleVar_ChildRounding, g.Style.FrameRounding);
        PushStyleVar(ImGuiStyleVar_ChildBorderSize, g.Style.FrameBorderSize);
        PushStyleVar(ImGuiStyleVar_WindowPadding, g.Style.FramePadding);
        child_flags |= ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding;
        window_flags |= ImGuiWindowFlags_NoMove;
    }

    g.NextWindowData.Flags |= ImGuiNextWindowDataFlags_HasChildFlags;
    g.NextWindowData.ChildFlags = child_flags;

    ImVec2 size = size_arg;
    if (size.x <= 0)
        size.x = (GetWindowWidth() - elements->content.padding.x * 2.f - elements->content.spacing.x * (x - 1)) / x;
    if (size.y <= 0)
        size.y = (GetWindowHeight() - elements->content.padding.y * 2 - elements->content.spacing.y * (y - 1)) / y;

    const char* display_name_end = name ? FindRenderedTextEnd(name) : NULL;
    bool has_title = (name && display_name_end > name);
    bool has_subtabs = !subtab_list_map[id].empty();
    float titlebar_offset = has_subtabs ? (var->window.titlebar + 4.f) : (has_title ? var->window.titlebar : 0.0f);

    gui->set_next_window_size(size - ImVec2(0, titlebar_offset));
    gui->set_next_window_pos(parent_window->DC.CursorPos + ImVec2(0, titlebar_offset));

    const char* temp_window_name;
    if (name)
        ImFormatStringToTempBuffer(&temp_window_name, NULL, "%s/%s_%08X", parent_window->Name, name, id);
    else
        ImFormatStringToTempBuffer(&temp_window_name, NULL, "%s/%08X", parent_window->Name, id);

    draw->rect_filled(parent_window->DrawList, parent_window->DC.CursorPos, parent_window->DC.CursorPos + size, draw->get_clr(clr->window.background_one));
    draw->line(parent_window->DrawList, parent_window->DC.CursorPos + ImVec2(2, 2), parent_window->DC.CursorPos + ImVec2(size.x - 2, 2), draw->get_clr(clr->accent));
    draw->rect(parent_window->DrawList, parent_window->DC.CursorPos, parent_window->DC.CursorPos + size, draw->get_clr(clr->window.stroke));

    if (!has_subtabs && has_title)
    {
        ImColor title_color = is_unsafe ? clr->widgets.unsafe_text : clr->widgets.text;
        draw->text_outline(parent_window->DrawList, var->font.tahoma, var->font.tahoma->FontSize, parent_window->DC.CursorPos + ImVec2(6, 6), draw->get_clr(title_color), name, display_name_end);
    }

    if (has_subtabs)
    {
        draw_child_subtabs(parent_window, id, size);
    }

    const float backup_border_size = g.Style.ChildBorderSize;
    if ((child_flags & ImGuiChildFlags_Border) == 0)
        g.Style.ChildBorderSize = 0.0f;

    const bool ret = gui->begin(temp_window_name, NULL, window_flags | ImGuiWindowFlags_NoBackground);

    g.Style.ChildBorderSize = backup_border_size;
    if (child_flags & ImGuiChildFlags_FrameStyle)
    {
        PopStyleVar(3);
        PopStyleColor();
    }

    ImGuiWindow* child_window = g.CurrentWindow;
    child_window->ChildId = id;

    if (child_window->BeginCount == 1)
        parent_window->DC.CursorPos = child_window->Pos;

    const ImGuiID temp_id_for_activation = ImHashStr("##Child", 0, id);
    if (g.ActiveId == temp_id_for_activation)
        ClearActiveID();
    if (g.NavActivateId == id && !(window_flags & ImGuiWindowFlags_NavFlattened) && (child_window->DC.NavLayersActiveMask != 0 || child_window->DC.NavWindowHasScrollY))
    {
        FocusWindow(child_window);
        NavInitWindow(child_window, false);
        SetActiveID(temp_id_for_activation, child_window);
        g.ActiveIdSource = g.NavInputSource;
    }
    return ret;
}

void c_gui::begin_child(std::string_view name, int x, int y, const ImVec2& size, bool is_unsafe)
{
    ImGuiID id = GetCurrentWindow()->GetID(name.data());

    gui->push_style_var(ImGuiStyleVar_WindowPadding, elements->widgets.padding);
    begin_child_ex(name.data(), id, x, y, size, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoMove, is_unsafe);
    gui->push_style_var(ImGuiStyleVar_ItemSpacing, elements->widgets.spacing);
}

void c_gui::end_child()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* child_window = g.CurrentWindow;

    IM_ASSERT(g.WithinEndChild == false);
    IM_ASSERT(child_window->Flags & ImGuiWindowFlags_ChildWindow);

    gui->pop_style_var();

    g.WithinEndChild = true;
    ImVec2 child_size = child_window->Size;
    gui->end();
    if (child_window->BeginCount == 1)
    {
        ImGuiWindow* parent_window = g.CurrentWindow;
        bool has_subtabs = !subtab_list_map[child_window->ChildId].empty();
        float titlebar_offset = has_subtabs ? (var->window.titlebar + 4.f) : (var->window.titlebar);
        ImVec2 full_size = child_size + ImVec2(0.f, titlebar_offset);
        ImVec2 original_pos = child_window->Pos - ImVec2(0.f, titlebar_offset);
        ImRect bb(original_pos, original_pos + full_size);

        parent_window->DC.CursorPos = original_pos;
        ItemSize(full_size);

        if ((child_window->DC.NavLayersActiveMask != 0 || child_window->DC.NavWindowHasScrollY) && !(child_window->Flags & ImGuiWindowFlags_NavFlattened))
        {
            ItemAdd(bb, child_window->ChildId);
            RenderNavHighlight(bb, child_window->ChildId);

            if (child_window->DC.NavLayersActiveMask == 0 && child_window == g.NavWindow)
                RenderNavHighlight(ImRect(bb.Min - ImVec2(2, 2), bb.Max + ImVec2(2, 2)), g.NavId, ImGuiNavHighlightFlags_Compact);
        }
        else
        {
            ItemAdd(bb, 0);

            if (child_window->Flags & ImGuiWindowFlags_NavFlattened)
                parent_window->DC.NavLayersActiveMaskNext |= child_window->DC.NavLayersActiveMaskNext;
        }
        if (g.HoveredWindow == child_window)
            g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HoveredWindow;
    }
    gui->pop_style_var();
    g.WithinEndChild = false;
    g.LogLinePosY = -FLT_MAX;
}
