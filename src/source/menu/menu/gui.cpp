
#include "settings/functions.h"
#include "settings/theme.h"
#include <hacks/system/settings/settings.h>
#include <hacks/system/config/config.h>
#include <hacks/system/notifications/notifications.h>
#include <hacks/system/playerlist/playerlist.h>
#include <hacks/visuals/chams/native/native.h>
#include <hacks/visuals/skyshader/skyshader.h>
#include <menu/render/render.h>
#include <menu/cortex/cortex_ui.h>
#include <hacks/exploits/environment/explorer/explorer.h>
#include <thread>
#include <chrono>
#include <Windows.h>

static bool s_show_unload_confirm = false;

void c_gui::render()
{
    
    static bool s_cx_init = false;
    if (!s_cx_init)
    {
        
        ImFont* def = ImGui::GetIO().FontDefault;
        cx::SetFonts(def, def, def);
        cx::ApplyTheme();
        cx::RegisterPages();
        s_cx_init = true;
    }

    playerlist::tick();

    
    bool open = var->gui.menu_opened;
    cx::RenderMenu(&open);
    var->gui.menu_opened = open;

    
    cx::DrawWatermark(settings::watermark::enabled && !open);

    
    cx::DrawToasts();

    
    explorer::explorer->render_decompiled_windows();

    
    if (s_show_unload_confirm)
    {
        ImGui::SetNextWindowSize(ImVec2(270, 105), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 135.f,
                   ImGui::GetIO().DisplaySize.y * 0.5f - 52.5f),
            ImGuiCond_Appearing);
        if (ImGui::Begin("Confirm Unload##UnloadDlg", &s_show_unload_confirm,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRectFullScreen();
            dl->AddRectFilled(ImVec2(0,0), ImGui::GetIO().DisplaySize, ImColor(0,0,0,140));
            dl->PopClipRect();

            ImGui::Text("Unload Cortex?");
            if (ImGui::Button("Confirm##ul_ok"))
            {
                s_show_unload_confirm = false;
                notifications::add("Unloading...");
                HWND overlay = (::render && ::render->detail) ? ::render->detail->window : nullptr;
                std::thread([overlay](){
                    Cheat::Features::SkyShader::Stop();
                    nativechams::Stop();
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    if (overlay && IsWindow(overlay)) ShowWindow(overlay, SW_HIDE);
                    HWND con = GetConsoleWindow();
                    if (con && IsWindow(con)) ShowWindow(con, SW_HIDE);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    ExitProcess(0);
                }).detach();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel##ul_no")) s_show_unload_confirm = false;
        }
        ImGui::End();
    }
}
