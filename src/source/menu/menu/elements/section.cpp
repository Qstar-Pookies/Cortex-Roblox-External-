#include "../settings/functions.h"

bool c_gui::section(std::string_view icon, bool* callback)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID((std::stringstream{} << icon << "section").str().c_str());

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect rect(pos, pos + elements->section.size);
    ItemSize(rect, style.FramePadding.y);
    if (!ItemAdd(rect, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(rect, id, &hovered, &held);

    if (pressed)
        *callback = !*callback;

    draw->fade_rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->window.background_one), draw->get_clr(clr->window.background_two), fade_direction::vertically);
    draw->rect(window->DrawList, rect.Min, rect.Max, var->window.hover_hightlight && hovered ? draw->get_clr(clr->accent) : draw->get_clr(clr->window.stroke));
    if (icon == "SVR")
    {
        ImVec2 center = ImVec2((rect.Min.x + rect.Max.x) * 0.5f, (rect.Min.y + rect.Max.y) * 0.5f);
        ImU32 col = draw->get_clr(clr->accent, *callback ? 1.f : 0.5f);

        const float ox = std::round(center.x);
        const float oy = std::round(center.y);
        const float track_thick = 1.3f;
        const float bar_w = 4.2f;
        const float bar_h = 2.0f;
        const float col_gap = 4.8f;
        const float half_span = 7.0f;

        const float x0 = ox - col_gap;
        const float knob_y0 = oy - 0.5f;
        window->DrawList->AddLine(ImVec2(x0, oy - half_span), ImVec2(x0, knob_y0 - bar_h * 0.5f), col, track_thick);
        window->DrawList->AddLine(ImVec2(x0, knob_y0 + bar_h * 0.5f), ImVec2(x0, oy + half_span), col, track_thick);
        window->DrawList->AddRectFilled(ImVec2(x0 - bar_w * 0.5f, knob_y0 - bar_h * 0.5f), ImVec2(x0 + bar_w * 0.5f, knob_y0 + bar_h * 0.5f), col, 0.8f);

        const float x1 = ox;
        const float knob_y1 = oy + 1.5f;
        window->DrawList->AddLine(ImVec2(x1, oy - half_span), ImVec2(x1, knob_y1 - bar_h * 0.5f), col, track_thick);
        window->DrawList->AddLine(ImVec2(x1, knob_y1 + bar_h * 0.5f), ImVec2(x1, oy + half_span), col, track_thick);
        window->DrawList->AddRectFilled(ImVec2(x1 - bar_w * 0.5f, knob_y1 - bar_h * 0.5f), ImVec2(x1 + bar_w * 0.5f, knob_y1 + bar_h * 0.5f), col, 0.8f);

        const float x2 = ox + col_gap;
        const float knob_y2 = oy - 0.5f;
        window->DrawList->AddLine(ImVec2(x2, oy - half_span), ImVec2(x2, knob_y2 - bar_h * 0.5f), col, track_thick);
        window->DrawList->AddLine(ImVec2(x2, knob_y2 + bar_h * 0.5f), ImVec2(x2, oy + half_span), col, track_thick);
        window->DrawList->AddRectFilled(ImVec2(x2 - bar_w * 0.5f, knob_y2 - bar_h * 0.5f), ImVec2(x2 + bar_w * 0.5f, knob_y2 + bar_h * 0.5f), col, 0.8f);
    }
    else if (icon == "NPC")
    {
        ImVec2 center = ImVec2((rect.Min.x + rect.Max.x) * 0.5f, (rect.Min.y + rect.Max.y) * 0.5f);
        ImU32 col = draw->get_clr(clr->accent, *callback ? 1.f : 0.5f);

        ImVec2 head_pos = center - ImVec2(0.0f, 3.5f);
        window->DrawList->AddCircle(head_pos, 3.2f, col, 16, 1.3f);

        window->DrawList->PathClear();
        window->DrawList->PathArcTo(center + ImVec2(0.0f, 6.0f), 5.5f, IM_PI + 0.35f, IM_PI * 2.0f - 0.35f, 24);
        window->DrawList->PathStroke(col, 0, 1.3f);
    }
    else
    {
        ImFont* font_to_use = (icon.length() > 1 || icon == "{/}") ? var->font.tahoma : var->font.icons[0];
        draw->text_clipped(window->DrawList, font_to_use, rect.Min, rect.Max, draw->get_clr(clr->accent, *callback ? 1.f : 0.5f), icon.data(), NULL, NULL, ImVec2(0.5f, 0.5f));
    }

    gui->sameline();

    return pressed;
}

bool c_gui::sub_section(std::string_view label, int section_id, int& section_variable, float count)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label.data());
    const bool selected = section_id == section_variable;

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect rect(pos, pos + ImVec2((GetWindowWidth() - elements->content.window_padding.x * 2 - g.Style.ItemSpacing.x * (count - 1)) / count, elements->section.height));
    ItemSize(rect, style.FramePadding.y);
    if (!ItemAdd(rect, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(rect, id, &hovered, &held);

    if (pressed)
        section_variable = section_id;

    if (selected || hovered)
        draw->fade_rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->window.background_one), draw->get_clr(clr->window.background_two), fade_direction::vertically);
    else
        draw->fade_rect_filled(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->window.background_two), draw->get_clr(clr->window.background_one), fade_direction::vertically);

    draw->rect(window->DrawList, rect.Min, rect.Max, draw->get_clr(clr->window.stroke));

    if (selected)
        draw->line(GetForegroundDrawList(), ImVec2(rect.Min.x + 1, rect.Max.y - 1), rect.Max - ImVec2(1, 1), draw->get_clr(clr->window.background_two));

    draw->text_clipped_outline(window->DrawList, var->font.tahoma, rect.Min, rect.Max, selected ? draw->get_clr(clr->accent) : draw->get_clr(clr->widgets.text_inactive), label.data(), NULL, NULL, ImVec2(0.5f, 0.5f));

    gui->sameline();

    return pressed;
}
