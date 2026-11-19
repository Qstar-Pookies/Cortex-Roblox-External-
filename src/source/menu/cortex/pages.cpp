
#include "cortex_ui.h"
#include <hacks/system/settings/settings.h>
#include <hacks/system/config/config.h>
#include <hacks/system/notifications/notifications.h>
#include <hacks/exploits/audio/hitsound/hitsoundsdata.h>
#include <hacks/luau/vm/vm.h>
#include <hacks/exploits/environment/explorer/explorer.h>
#include <hacks/visuals/model/modelviewer.h>
#include <hacks/visuals/chams/memory/shader.h>
#include <hacks/visuals/chams/memory/chams.h>
#include <hacks/visuals/chams/native/native.h>
#include <hacks/visuals/skyshader/skyshader.h>
#include <hacks/visuals/chams/highlight/highlight.h>
#include <hacks/system/playerlist/playerlist.h>
#include <menu/render/render.h>
#include <game/cache/core/cache.h>
#include <menu/editor/editor.h>
#include <game/game/game.h>
#include <hacks/system/waypoints/waypoints.h>
#include <hacks/visuals/lighting/lighting.h>
#include <hacks/luau/mcp/mcp_server.h>
#include <hacks/visuals/manager/manager.h>
#include <hacks/system/serverbrowser/serverbrowser.h>
#include <hacks/exploits/locomotion/movement/movement.h>
#include <hacks/exploits/utility/misc/misc.h>
#include <hacks/system/keybind/keybind.h>
#include <menu/menu/settings/theme.h>
#include <menu/menu/settings/functions.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <shellapi.h>
#include <Windows.h>
#include <commdlg.h>

static const char* s_target_parts[]       = { "Head", "Torso", "HumanoidRootPart", "Left Arm", "Right Arm", "Left Leg", "Right Leg", "Closest Part", "Nearest" };
static const char* s_aim_modes[]           = { "Mouse", "Camera" };
static const char* s_sa_methods[]          = { "Raycast", "Mouse", "Viewport", "Camera" };
static const char* s_sky_modes[]           = { "Nebula", "Aurora", "Starfield", "Galaxy", "Synthwave", "Sunset", "Plasma" };
static const char* s_box_types[]           = { "Corner", "Full" };
static const char* s_tracer_origins[]      = { "Bottom", "Center", "Top" };
static const char* s_chams_types[]         = { "Clipper2", "Memory", "Engine", "Native", "Highlight" };
static const char* s_hb_positions[]        = { "Left", "Right", "Bottom", "Top" };
static const char* s_hb_gradients[]        = { "Vertical", "Horizontal", "Pulse" };
static const char* s_server_sorts[]        = { "Lowest Players", "Highest Players", "Lowest Ping", "Highest FPS" };
static const char* s_speed_modes[]         = { "Velocity", "Walkspeed", "CFrame" };
static const char* s_fly_modes[]           = { "Velocity", "CFrame", "CFrame (smooth)" };
static const char* s_spamtp_modes[]        = { "Orbit", "Random", "Behind", "Above" };
static const char* s_btools_types[]        = { "Hammer", "Grab", "Clone" };
static const char* s_mlook_modes[]         = { "Hold RMB", "Always" };
static const char* s_wb_methods[]          = { "Engine", "Position" };
static const char* s_name_modes[]          = { "Display Name", "Username", "Both" };
static const char* s_name_positions[]      = { "Top", "Bottom", "Left", "Right" };
static const char* s_name_transforms[]     = { "None", "Uppercase", "Lowercase" };
static const char* s_name_grad_types[]     = { "Wave", "Rainbow", "Left to Right" };
static const char* s_arrow_types[]         = { "Normal", "Flat" };
static const char* s_arrow_modes[]         = { "Screen Center", "Mouse Position" };
static const char* s_global_outline_types[]= { "None", "Outline", "Shadow" };
static const char* s_engine_styles[]       = { "Wireframe Normal", "Wireframe Textured", "Character Wireframe", "Character Mesh" };
static const char* s_engine_modes[]        = { "Glow", "Invisible", "Always On Top" };
static const char* s_engine_colors[]       = {
    "White","Red","Green","Blue","Yellow","Cyan","Magenta","Orange",
    "Purple","Pink","Lime","Aqua","Gold","Violet","Teal","Crimson",
    "Navy","Emerald","Coral","Silver"
};
static const char* s_gradient_styles[]     = { "Vertical", "Horizontal", "Pulse" };
static const char* s_fill_gradients[]      = { "Side-to-Side", "Bottom-to-Top", "Pulse" };
static const char* s_material_names[]      = {
    "ForceField","Neon","Glass","Plastic","SmoothPlastic","Foil","Ice","Metal",
    "CorrodedMetal","DiamondPlate","Wood","WoodPlanks","Marble","Slate","Concrete",
    "Granite","Brick","Pebble","Cobblestone","Sand","Fabric"
};
static const char* s_notify_positions[]    = { "Top Right", "Top Left", "Bottom Right", "Bottom Left" };
static const char* s_preview_models[]      = { "Arsenal", "Roblox Avatar", "Tung Tung Tung Sahur", "Custom (Resources)" };

static bool s_select_sound_file(char* out_path, size_t max_len)
{
    OPENFILENAMEA ofn;
    char buf[MAX_PATH] = "";
    if (out_path && strlen(out_path) > 0) strncpy_s(buf, out_path, sizeof(buf) - 1);
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (::render && ::render->detail) ? ::render->detail->window : NULL;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf);
    ofn.lpstrFilter = "Audio Files (*.wav;*.mp3)\0*.wav;*.mp3\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) { strncpy_s(out_path, max_len, buf, max_len - 1); return true; }
    return false;
}

static void DrawEspSubtab(settings::visuals::priority_settings_t* cur)
{
    using namespace cx;

    BeginCard("box_esp", "Box ESP", "Bounding box around players.", &cur->box);
    if (cur->box) {
        RowCombo("Box Type", &cur->box_type, s_box_types, 2);
        if (cur->box_type == 0) RowSlider("Corner Length", &cur->corner_box_length, 0.1f, 1.0f, "%.2f");
        RowCheck("Box Inline", &cur->box_inline);
        RowCheck("Box Gradient", &cur->box_gradient);
        if (cur->box_gradient) {
            RowCombo("Gradient Style", &cur->box_gradient_type, s_gradient_styles, 3);
            if (cur->box_gradient_type == 2) RowSlider("Pulse Speed", &cur->box_pulse_speed, 0.1f, 10.0f, "%.1fx");
        }
        RowCheck("Box Filled", &cur->box_fill);
        if (cur->box_fill) {
            RowCheck("Fill Gradient", &cur->box_fill_gradient);
            if (cur->box_fill_gradient) {
                RowCombo("Fill Gradient Style", &cur->box_fill_gradient_type, s_fill_gradients, 3);
                if (cur->box_fill_gradient_type == 2) RowSlider("Pulse Speed", &cur->box_fill_pulse_speed, 0.1f, 10.0f, "%.1fx");
                RowCheck("Gradient Spin", &cur->box_fill_gradient_spin);
                if (cur->box_fill_gradient_spin) RowSlider("Spin Speed", &cur->box_fill_gradient_speed, 0.1f, 10.0f, "%.1fx");
            }
        }
        RowSlider("Box Thickness", &settings::visuals::box_thickness, 0.5f, 5.0f);
    }
    EndCard();

    BeginCard("name_esp", "Name ESP", "Player name above head.", &cur->username);
    if (cur->username) {
        RowCombo("Name Mode", &cur->username_type, s_name_modes, 3);
        RowCombo("Name Position", &cur->username_position, s_name_positions, 4);
        RowCombo("Name Transform", &cur->name_transform, s_name_transforms, 3);
        RowCheck("Name Gradient", &cur->username_gradient);
        if (cur->username_gradient) {
            RowCombo("Gradient Style", &cur->username_gradient_type, s_name_grad_types, 3);
            RowSlider("Gradient Speed", &cur->username_gradient_speed, 0.5f, 10.0f, "%.1fx");
        }
    }
    EndCard();

    BeginCard("health_esp", "Health Bar", "Health bar around bounding box.", &cur->healthbar);
    if (cur->healthbar) {
        RowCombo("Position", &cur->healthbar_style, s_hb_positions, 4);
        RowSlider("Padding", &cur->healthbar_padding, 0.f, 20.f);
        RowCheck("Health Text", &cur->healthbar_text);
        RowCheck("Value Follow", &cur->value_follow);
        RowCheck("Health Lerp", &cur->healthbar_lerp);
        RowCheck("Ignore Full HP", &cur->ignore_full);
        RowCheck("Bar Fold", &cur->bar_fold);
        RowCheck("Bar Fill", &settings::visuals::bar_fill);
        RowCombo("Gradient Style", &cur->healthbar_gradient_type, s_hb_gradients, 3);
        if (cur->healthbar_gradient_type == 2) RowSlider("Pulse Speed", &cur->healthbar_pulse_speed, 0.5f, 5.0f);
        RowSlider("Bar Thickness", &settings::visuals::healthbar_thickness, 1.0f, 6.0f);
    }
    EndCard();

    BeginCard("tracers", "Tracers", "Lines from screen edge to player.", &cur->tracers);
    if (cur->tracers) {
        RowCombo("Origin", &cur->tracers_origin, s_tracer_origins, 3);
        RowSlider("Thickness", &cur->tracers_thickness, 0.5f, 5.0f);
    }
    EndCard();

    BeginCard("skeleton", "Skeleton", "Bone lines over player model.", &cur->skeleton);
    if (cur->skeleton) RowSlider("Thickness", &settings::visuals::skeleton_thickness, 0.5f, 5.0f);
    EndCard();

    BeginCard("arrows", "Offscreen Arrows", "Arrow pointing toward off-screen players.", &cur->offscreen_arrows);
    if (cur->offscreen_arrows) {
        RowCombo("Arrow Type", &cur->arrow_type, s_arrow_types, 2);
        RowCombo("Arrow Mode", &cur->arrow_mode, s_arrow_modes, 2);
        RowCheck("Arrows Glow", &cur->arrows_glow);
        RowSlider("Arrow Radius", &cur->offscreen_arrows_radius, 50.f, 400.f);
        RowSlider("Arrow Size", &cur->offscreen_arrows_size, 5.f, 30.f);
    }
    EndCard();

    BeginCard("chams_card", "Chams", "Player highlight/material chams.", &cur->chams);
    if (cur->chams || cur->native || settings::visuals::engine_chams) {
        RowCheck("Engine Chams", &settings::visuals::engine_chams);
        RowCheck("Native Chams", &cur->native);
        if (cur->chams) {
            RowCombo("Chams Type", &cur->chams_type, s_chams_types, 5);
            if (cur->chams_type == 1) {
                static const char* mesh_styles[] = { "Imgui", "Dx" };
                RowCombo("Mesh Style", &settings::visuals::mesh_chams_style, mesh_styles, 2);
                if (settings::visuals::mesh_chams_style == 1)
                    RowCombo("DX Mode", &settings::visuals::mesh_chams_dx_mode, Cheat::Visuals::MeshDxShader::ModeNames(), Cheat::Visuals::MeshDxShader::ModeNameCount());
                RowCheck("Occluded", &settings::visuals::mesh_chams_occlusion);
                if (settings::visuals::mesh_chams_occlusion)
                    RowCombo("Occluded Mode", &settings::visuals::mesh_chams_occluded_dx_mode, Cheat::Visuals::MeshDxShader::ModeNames(), Cheat::Visuals::MeshDxShader::ModeNameCount());
                RowCheck("Outline", &settings::visuals::mesh_chams_outline);
                if (settings::visuals::mesh_chams_outline) {
                    RowCombo("Outline Style", &settings::visuals::mesh_chams_outline_style, Cheat::Visuals::MeshChams::OutlineStyleNames(), Cheat::Visuals::MeshChams::OutlineStyleNameCount());
                    RowSlider("Outline Fade", &settings::visuals::mesh_chams_outline_fade, 0.35f, 2.0f);
                }
            }
            if (settings::visuals::engine_chams || cur->chams_type == 2) {
                RowCombo("Engine Style", &settings::visuals::engine_chams_style, s_engine_styles, 4);
                RowCombo("Engine Mode", &settings::visuals::engine_chams_mode, s_engine_modes, 3);
                RowCombo("Engine Color", &settings::visuals::engine_chams_color_index, s_engine_colors, 20);
                RowCheck("Weapon & Arms", &settings::visuals::engine_chams_weapons);
                if (settings::visuals::engine_chams_weapons) {
                    RowCombo("Weapon Style", &settings::visuals::engine_chams_weapons_style, s_engine_styles, 4);
                    RowCombo("Weapon Mode", &settings::visuals::engine_chams_weapons_mode, s_engine_modes, 3);
                    RowCombo("Weapon Color", &settings::visuals::engine_chams_weapons_color_index, s_engine_colors, 20);
                }
            }
            if (cur->native || cur->chams_type == 3) {
                RowCombo("Native Shader", &settings::visuals::native_render_shader, nativechams::ShaderNames(), nativechams::ShaderNameCount());
                RowCheck("Native Glow", &settings::visuals::native_glow);
                if (settings::visuals::native_glow) RowSlider("Glow Strength", &settings::visuals::native_glow_strength, 0.0f, 2.0f);
                RowCheck("Through Walls", &settings::visuals::native_walls);
                RowSlider("Depth", &settings::visuals::native_depth, 0.0f, 1.0f);
                RowCheck("Team Check", &settings::visuals::native_teamcheck);
            }
            if (cur->chams_type == 4) {
                RowCheck("Through Walls", &settings::visuals::highlight_through_walls);
                RowCheck("Outline", &settings::visuals::highlight_outline);
            } else {
                RowCheck("Chams Outline", &cur->mesh_outline);
                if (cur->mesh_outline) RowSlider("Outline Thickness", &cur->mesh_outline_thickness, 0.5f, 6.0f);
                RowCheck("Outline Glow", &cur->chams_outline_glow);
                if (cur->chams_outline_glow) {
                    RowSlider("Glow Radius", &cur->chams_glow_radius, 1.0f, 20.0f);
                    RowSlider("Glow Intensity", &cur->chams_glow_intensity, 1.0f, 100.0f, "%.0f%%");
                }
                RowSlider("Chams Thickness", &settings::visuals::chams_thickness, 0.0f, 5.0f);
            }
        }
    }
    EndCard();

    BeginCard("misc_esp", "Misc Flags");
    RowCheck("Distance", &cur->distance);
    RowCheck("Held Tool", &cur->tool);
    RowCheck("Head Dot", &cur->head_dot);
    if (cur->head_dot) {
        static const char* head_types[] = { "Dot", "Hexagon" };
        RowCombo("Head Type", &cur->head_type, head_types, 2);
        RowCheck("Circular Dot", &cur->circular_head_dot);
        RowSlider("Dot Size", &cur->head_dot_size, 1.f, 10.f);
    }
    RowCheck("Corpse Chams", &cur->corpse);
    if (cur->corpse) {
        RowSlider("Corpse Shift", &settings::visuals::corpse_shift, -10.f, 10.f, "%.1f studs");
        RowSlider("Corpse Max Dist", &settings::visuals::corpse_max_dist, 50.f, 5000.f, "%.0f studs");
    }
    RowCheck("Corpse Names", &cur->corpse_names);
    RowCheck("View Angles", &cur->view_angle_lines);
    if (cur->view_angle_lines) {
        RowSlider("Line Thickness", &cur->view_angle_lines_thickness, 0.1f, 5.0f, "%.1fpx");
        RowSlider("Line Length", &cur->view_angle_lines_length, 1.0f, 100.0f, "%.0f studs");
    }
    RowCheck("Sound ESP", &cur->sound_esp);
    if (cur->sound_esp) {
        RowSlider("Sound Radius", &cur->sound_esp_radius, 1.f, 25.f);
        RowSlider("Sound Speed", &cur->sound_esp_speed, 0.1f, 5.0f);
    }
    RowCheck("Footprints", &cur->footprints);
    if (cur->footprints) {
        RowCheck("Footprints Glow", &cur->footprints_glow);
        RowSlider("Footprint Radius", &cur->footprints_radius, 1.f, 10.f);
        RowSlider("Footprint Interval", &cur->footprints_speed, 0.1f, 5.0f);
    }
    RowCheck("Movement Trails", &cur->movement_trails);
    if (cur->movement_trails) {
        RowSlider("Trail Thickness", &cur->movement_trails_thickness, 0.1f, 10.0f, "%.1fpx");
        RowSlider("Trail Length", &cur->movement_trails_length, 0.1f, 10.0f, "%.1fs");
    }
    RowCheck("Material Changer", &settings::materialchanger::enabled);
    if (settings::materialchanger::enabled)
        RowCombo("Material", &settings::materialchanger::material_index, s_material_names, 21);
    EndCard();
}

namespace cx {

void RegisterPages()
{
    SetBrand("Cortex");

    
    AddSection("COMBAT");

    AddPage("Aimbot", Icon_None, [] {
        BeginCard("aimbot", "Aimbot", "Bone-targeted mouse / camera aim assist.", &settings::aimbot::enabled);
        RowCombo("Aim Mode", &settings::aimbot::mode, s_aim_modes, 2);
        RowCombo("Target Part", &settings::aimbot::target_part, s_target_parts, 9);
        RowCombo("Air Target Part", &settings::aimbot::air_part, s_target_parts, 9);
        if (settings::aimbot::mode == 0)
            RowSlider("Mouse Sensitivity", &settings::aimbot::mouse_sensitivity, 0.f, 10.f);
        RowSlider("FOV", &settings::aimbot::fov, 0.f, 600.f);
        RowCheck("Sticky Aim", &settings::aimbot::sticky_aim);
        RowCheck("Dynamic FOV", &settings::aimbot::dynamic_fov);
        RowCheck("Draw FOV", &settings::aimbot::draw_fov);
        RowCheck("Fill FOV", &settings::aimbot::fill_fov);
        RowCheck("Smoothness", &settings::aimbot::smoothing);
        if (settings::aimbot::smoothing) {
            RowSlider("Smoothing X", &settings::aimbot::smoothingx, 1.f, 100.f);
            RowSlider("Smoothing Y", &settings::aimbot::smoothingy, 1.f, 100.f);
        }
        RowCheck("Prediction", &settings::aimbot::enable_prediction);
        if (settings::aimbot::enable_prediction) {
            RowSlider("Prediction X", &settings::aimbot::prediction_x, 0.f, 100.f);
            RowSlider("Prediction Y", &settings::aimbot::prediction_y, 0.f, 100.f);
        }
        RowCheck("Team Check", &settings::aimbot::teamcheck);
        RowCheck("Universal Teamcheck", &settings::teamcheck);
        RowCheck("Knock Check", &settings::aimbot::knock_check);
        EndCard();
    });

    AddPage("Silent Aim", Icon_None, [] {
        BeginCard("sa", "Silent Aim", "Server-side target manipulation.", &settings::silentaim::enabled);
        RowCombo("Method", &settings::silentaim::method, s_sa_methods, 4);
        RowCombo("Target Bone", &settings::silentaim::target_part, s_target_parts, 9);
        if (settings::silentaim::method == 0) {
            RowCheck("360 Mode", &settings::raycast_silentaim::mode_360);
            RowCheck("Wallbang", &settings::raycast_silentaim::magic_bullet);
            if (settings::raycast_silentaim::magic_bullet)
                RowCombo("Wallbang Method", &settings::raycast_silentaim::magic_bullet_method, s_wb_methods, 2);
        }
        RowSlider("FOV", &settings::silentaim::fov, 0.f, 1000.f);
        RowCheck("Draw FOV", &settings::silentaim::draw_fov);
        RowCheck("Fill FOV", &settings::silentaim::fill_fov);
        RowCheck("Snapline", &settings::silentaim::snapline);
        RowCheck("Team Check", &settings::silentaim::teamcheck);
        RowCheck("Gun Check", &settings::silentaim::guncheck);
        RowCheck("Knock Check", &settings::silentaim::knock_check);
        RowCheck("Forcefield Check", &settings::silentaim::forcefield_check);
        EndCard();

        BeginCard("hitmarkers", "Hitmarkers", "On-screen crosshair on hit.", &settings::raycast_silentaim::hitmarkers);
        RowSlider("Size", &settings::raycast_silentaim::hitmarker_size, 2.f, 20.f);
        RowSlider("Gap", &settings::raycast_silentaim::hitmarker_gap, 1.f, 15.f);
        EndCard();
    });

    AddPage("Triggerbot", Icon_None, [] {
        BeginCard("tb", "Triggerbot", "Auto-fire when target is in crosshair.", &settings::triggerbot::enabled);
        RowCombo("Target Part", &settings::triggerbot::target_part, s_target_parts, 9);
        RowSlider("Delay", &settings::triggerbot::delay_ms, 0.f, 500.f, "%.0fms");
        RowSlider("Cooldown", &settings::triggerbot::cooldown_ms, 0.f, 1000.f, "%.0fms");
        RowCheck("Team Check", &settings::triggerbot::teamcheck);
        RowCheck("Wall Check", &settings::triggerbot::wallcheck);
        RowCheck("Knock Check", &settings::triggerbot::knock_check);
        RowCheck("Gun Check", &settings::triggerbot::guncheck);
        EndCard();
    });

    
    AddSection("VISUALS");

    AddPage("Enemies", Icon_None, [] {
        static bool s_enabled_guard = settings::visuals::enabled;
        BeginCard("vis_en_hdr", "Enemy ESP", "Enable enemy player ESP.");
        cx::RowToggle("Master Enable", &settings::visuals::enabled);
        RowCheck("Team Check", &settings::visuals::teamcheck);
        RowCheck("Universal Teamcheck", &settings::teamcheck);
        RowCheck("Visible Check", &settings::visuals::visible_check);
        RowCheck("Knock Check", &settings::visuals::knock_check);
        RowCheck("Alive Check", &settings::visuals::alive_check);
        RowCheck("Anti-Aliasing", &settings::visuals::anti_aliasing);
        RowCheck("Distance Limit", &settings::visuals::distance_check);
        if (settings::visuals::distance_check)
            RowSlider("Max Distance", &settings::visuals::max_distance, 50.f, 5000.f, "%.0f studs");
        EndCard();
        DrawEspSubtab(&settings::visuals::hostile);
    });

    AddPage("Friendlies", Icon_None, [] {
        DrawEspSubtab(&settings::visuals::friendly);
    });

    AddPage("World", Icon_None, [] {
        BeginCard("fov_card", "Field of View", "Override game FOV.", &cheat::world_settings->custom_fov);
        if (cheat::world_settings->custom_fov)
            RowSlider("FOV", &cheat::world_settings->custom_fov_value, 30.f, 120.f, "%.0f");
        EndCard();

        BeginCard("time_card", "Clock Time", "Lock in-game time of day.", &cheat::world_settings->clock_time_enabled);
        if (cheat::world_settings->clock_time_enabled)
            RowSlider("Clock Time", &cheat::world_settings->clock_time_value, 0.f, 24.f, "%.1fh");
        EndCard();

        BeginCard("fog_card", "Fog", "Custom fog settings.", &cheat::world_settings->fog_enabled);
        if (cheat::world_settings->fog_enabled) {
            RowSlider("Fog Start", &cheat::world_settings->fog_start, 0.f, 1000.f);
            RowSlider("Fog End", &cheat::world_settings->fog_end, 0.f, 3000.f);
        }
        EndCard();

        BeginCard("atmo_card", "Atmosphere", "Custom atmosphere colour.", &cheat::world_settings->atmosphere_enabled);
        if (cheat::world_settings->atmosphere_enabled)
            RowSlider("Density", &cheat::world_settings->atmosphere_density, 0.f, 1.f);
        EndCard();

        BeginCard("bloom_card", "Bloom", "Post-process bloom intensity.", &cheat::world_settings->bloom_enabled);
        if (cheat::world_settings->bloom_enabled) {
            RowSlider("Intensity", &cheat::world_settings->bloom_intensity, 0.f, 5.f);
            RowSlider("Size", &cheat::world_settings->bloom_size, 0.f, 50.f);
            RowSlider("Threshold", &cheat::world_settings->bloom_threshold, 0.f, 4.f);
        }
        EndCard();

        BeginCard("blur_card", "Blur", "Post-process blur.", &cheat::world_settings->blur_enabled);
        if (cheat::world_settings->blur_enabled)
            RowSlider("Size", &cheat::world_settings->blur_size, 0.f, 55.f);
        EndCard();

        BeginCard("cc_card", "Color Correction", "Brightness / contrast override.", &cheat::world_settings->color_correction_enabled);
        if (cheat::world_settings->color_correction_enabled) {
            RowSlider("Brightness", &cheat::world_settings->cc_brightness, -1.f, 1.f);
            RowSlider("Contrast", &cheat::world_settings->cc_contrast, -1.f, 1.f);
        }
        EndCard();

        BeginCard("dof_card", "Depth of Field", "DOF post-process.", &cheat::world_settings->depth_of_field_enabled);
        if (cheat::world_settings->depth_of_field_enabled) {
            RowSlider("Focus Distance", &cheat::world_settings->dof_focus_distance, 0.f, 500.f);
            RowSlider("In Focus Radius", &cheat::world_settings->dof_in_focus_radius, 0.f, 500.f);
            RowSlider("Near Intensity", &cheat::world_settings->dof_near_intensity, 0.f, 1.f);
            RowSlider("Far Intensity", &cheat::world_settings->dof_far_intensity, 0.f, 1.f);
        }
        EndCard();

        BeginCard("misc_world", "Misc World");
        RowCheck("Disable Shadows", &cheat::world_settings->shadows_enabled_toggle);
        RowCheck("Exposure", &cheat::world_settings->exposure_enabled);
        if (cheat::world_settings->exposure_enabled)
            RowSlider("Exposure Level", &cheat::world_settings->exposure_value, -5.f, 5.f);
        RowCheck("Lock Lightning", &cheat::world_settings->lock_lighting);
        if (cheat::world_settings->lock_lighting) {
            if (Button("Update Baseline", ImVec2(0,0), false)) cheat::world_feature->capture_lock_baseline();
        }
        EndCard();
    });

    AddPage("Effects", Icon_None, [] {
        BeginCard("sky_card", "Sky Shader", "Full-screen procedural skybox.", &cheat::world_settings->sky_shader_enabled);
        if (cheat::world_settings->sky_shader_enabled) {
            RowCombo("Sky Shader Mode", &cheat::world_settings->sky_shader_mode, s_sky_modes, 7);
            RowCheck("Sky Tint", &cheat::world_settings->sky_shader_tint);
            RowCheck("Pause Sky", &cheat::world_settings->sky_shader_paused);
        }
        EndCard();

        const auto& clean_names = Cheat::Features::HitSoundsData::GetCleanNamesVector();
        BeginCard("hitsounds", "Hitsounds", "Audio feedback on hits.", &settings::hitsounds::enabled);
        if (settings::hitsounds::enabled && !clean_names.empty()) {
            RowCombo("Preset", &settings::hitsounds::type, clean_names.data(), (int)clean_names.size());
            RowSlider("Volume", &settings::hitsounds::volume, 0.f, 1.f);
            if (settings::hitsounds::type == 0) {
                if (Button("Browse File##hs", ImVec2(0,0), false))
                    s_select_sound_file(settings::hitsounds::custom_path, sizeof(settings::hitsounds::custom_path));
            }
        }
        EndCard();

        BeginCard("killsounds", "Killsounds", "Audio feedback on kills.", &settings::killsounds::enabled);
        if (settings::killsounds::enabled && !clean_names.empty()) {
            RowCombo("Preset", &settings::killsounds::type, clean_names.data(), (int)clean_names.size());
            RowSlider("Volume", &settings::killsounds::volume, 0.f, 1.f);
            if (settings::killsounds::type == 0) {
                if (Button("Browse File##ks", ImVec2(0,0), false))
                    s_select_sound_file(settings::killsounds::custom_path, sizeof(settings::killsounds::custom_path));
            }
        }
        EndCard();

        BeginCard("hitmarkers_card", "Hitmarkers", "On-screen crosshair on hit.", &settings::raycast_silentaim::hitmarkers);
        if (settings::raycast_silentaim::hitmarkers) {
            RowSlider("Size", &settings::raycast_silentaim::hitmarker_size, 2.f, 20.f);
            RowSlider("Gap", &settings::raycast_silentaim::hitmarker_gap, 1.f, 15.f);
        }
        EndCard();
    });

    
    AddSection("EXPLOITS");

    AddPage("Movement", Icon_None, [] {
        BeginCard("speed", "Speed", "Override movement walkspeed.", &settings::movement::speedhack::enabled);
        if (settings::movement::speedhack::enabled) {
            RowCombo("Speed Method", &settings::movement::speedhack::mode, s_speed_modes, 3);
            RowSlider("Speed", &settings::movement::speedhack::speed, 16.f, 300.f);
        }
        EndCard();

        BeginCard("fly", "Flight", "Free-fly movement exploit.", &settings::movement::flyhack::enabled);
        if (settings::movement::flyhack::enabled) {
            RowCombo("Flight Method", &settings::movement::flyhack::mode, s_fly_modes, 3);
            RowSlider("Fly Speed", &settings::movement::flyhack::speed, 10.f, 300.f);
        }
        EndCard();

        BeginCard("spamtp", "Spam TP", "Rapid teleport pattern.", &settings::movement::spam_tp::enabled);
        if (settings::movement::spam_tp::enabled) {
            RowCombo("Pattern", &settings::movement::spam_tp::mode, s_spamtp_modes, 4);
            RowSlider("Radius", &settings::movement::spam_tp::radius, 0.0f, 40.0f, "%.1f studs");
            RowSlider("Height", &settings::movement::spam_tp::height, -15.0f, 40.0f, "%.1f studs");
            RowSlider("Speed", &settings::movement::spam_tp::speed, 1.0f, 30.0f, "%.1fx");
        }
        EndCard();

        BeginCard("bhop", "Bhop", "Bunny-hop speed boost.", &settings::movement::bhop::enabled);
        if (settings::movement::bhop::enabled)
            RowSlider("Bhop Speed", &settings::movement::bhop::speed, 16.f, 200.f);
        EndCard();

        BeginCard("misc_move", "Misc Movement");
        RowToggle("Noclip", &settings::movement::noclip::enabled);
        RowToggle("Infinite Jump", &settings::movement::infjump::enabled);
        RowToggle("Gravity", &settings::movement::gravity::enabled);
        if (settings::movement::gravity::enabled)
            RowSlider("Gravity Level", &settings::movement::gravity::value, 0.f, 400.f);
        RowToggle("Tickrate", &settings::movement::tickrate::enabled);
        if (settings::movement::tickrate::enabled)
            RowSlider("Tickrate", &settings::movement::tickrate::value, 30.f, 500.f);
        RowToggle("Wallbug", &settings::movement::wallbug::enabled);
        if (settings::movement::wallbug::enabled)
            RowSlider("Height", &settings::movement::wallbug::height, 5.f, 100.f);
        RowToggle("Wallslide", &settings::movement::wallslide::enabled);
        RowToggle("Spider", &settings::movement::spiderman::enabled);
        EndCard();
    });

    AddPage("View", Icon_None, [] {
        BeginCard("tp3", "Third Person", "Third-person camera view.", &settings::movement::third_person::enabled);
        if (settings::movement::third_person::enabled)
            RowSlider("Distance", &settings::movement::third_person::distance, 2.f, 80.f);
        EndCard();

        BeginCard("freecam", "Free-Cam", "Detached free camera.", &settings::movement::freecam::enabled);
        if (settings::movement::freecam::enabled) {
            RowCombo("Mouse Look", &settings::movement::freecam::mouse_look_mode, s_mlook_modes, 2);
            RowSlider("Camera Speed", &settings::movement::freecam::speed, 0.1f, 10.0f, "%.1fx");
            RowSlider("Sensitivity", &settings::movement::freecam::sensitivity, 0.0005f, 0.01f, "%.4f");
            RowSlider("Sprint Boost", &settings::movement::freecam::shift_multiplier, 1.0f, 5.0f, "%.1fx");
        }
        EndCard();

        BeginCard("misc_view", "Misc View");
        RowToggle("Desync", &settings::movement::desync::enabled);
        RowToggle("No Fall Damage", &settings::movement::no_fall_damage::enabled);
        RowToggle("Hip Height", &settings::movement::hipheight::enabled);
        if (settings::movement::hipheight::enabled)
            RowSlider("Height", &settings::movement::hipheight::value, 0.f, 25.f);
        RowToggle("Long Neck", &settings::movement::longneck::enabled);
        if (settings::movement::longneck::enabled)
            RowSlider("Length", &settings::movement::longneck::length, 1.f, 30.f);
        RowToggle("Void Hide", &settings::movement::voidhide::enabled);
        RowToggle("Sit State", &settings::movement::sit::enabled);
        RowToggle("Platform Stand", &settings::movement::platformstand::enabled);
        RowToggle("BTools", &settings::btools::enabled);
        if (settings::btools::enabled)
            RowCombo("Tool Type", &settings::btools::tool_type, s_btools_types, 3);
        RowToggle("Auto Emote", &settings::misc::auto_emote);
        if (settings::misc::auto_emote) {
            RowSlider("Emote Delay", &settings::misc::auto_emote_delay, 10.0f, 500.0f, "%.0fms");
            RowCheck("Repeat While Held", &settings::misc::auto_emote_repeat);
            if (settings::misc::auto_emote_repeat)
                RowSlider("Repeat Interval", &settings::misc::auto_emote_repeat_delay, 100.0f, 3000.0f, "%.0fms");
        }
        EndCard();

        if (game::datamodel && game::datamodel->address != 0 && game::datamodel->is_arsenal()) {
            BeginCard("arsenal", "Arsenal");
            RowToggle("Fast Fire Rate", &settings::arsenal::fast_fire_rate);
            RowToggle("No Recoil", &settings::arsenal::no_recoil);
            RowToggle("All Auto", &settings::arsenal::all_auto);
            RowToggle("Infinite Ammo", &settings::arsenal::infinite_ammo);
            EndCard();
        }
    });

    
    AddSection("UTILITY");

    AddPage("Players", Icon_None, [] {
        
        static char s_search[64] = "";
        static std::string s_sel_name;
        static uint64_t s_sel_uid = 0;

        if (::render && ::render->detail && ::render->detail->device)
            visuals::CAvatarManager::Get().ProcessPendingTextures(::render->detail->device);

        std::vector<cache::entity_t> all;
        cache::entity_t local = cache::get_local_player();
        if (local.instance.address) all.push_back(local);
        {
            std::lock_guard<std::mutex> lk(cache::mtx);
            for (const auto& p : cache::players)
                if (p.instance.address != local.instance.address) all.push_back(p);
        }

        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##psearch", s_search, sizeof(s_search));
        ImGui::Spacing();

        std::string ql = s_search;
        std::transform(ql.begin(), ql.end(), ql.begin(), ::tolower);

        for (size_t i = 0; i < all.size(); i++) {
            const auto& p = all[i];
            std::string nl = p.name, dl = p.display_name;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            std::transform(dl.begin(), dl.end(), dl.begin(), ::tolower);
            if (!ql.empty() && nl.find(ql) == std::string::npos && dl.find(ql) == std::string::npos) continue;

            bool is_sel = (s_sel_name == p.name);
            if (is_sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f, 0.13f, 0.17f, 1.0f));
            if (ImGui::Selectable((p.display_name + " (" + p.name + ")##psel" + std::to_string(i)).c_str(), is_sel)) {
                if (is_sel) { s_sel_name.clear(); s_sel_uid = 0; }
                else { s_sel_name = p.name; s_sel_uid = p.user_id; }
            }
            if (is_sel) ImGui::PopStyleColor();
        }

        if (!s_sel_name.empty()) {
            ImGui::Separator();
            
            if (ImGui::Button("Target")) playerlist::set_priority(s_sel_name, cache::player_priority(2));
            ImGui::SameLine();
            if (ImGui::Button("Friend")) playerlist::set_priority(s_sel_name, cache::player_priority(1));
            ImGui::SameLine();
            if (ImGui::Button("Ignore")) playerlist::set_priority(s_sel_name, cache::player_priority(3));
            ImGui::SameLine();
            if (ImGui::Button("Reset")) playerlist::set_priority(s_sel_name, cache::player_priority(0));
        }
    });

    AddPage("Explorer", Icon_None, [] {
        
        cx::RowCheck("Show explorer window", &g_explorerOpen);
        cx::TextDim("The explorer tree floats outside the menu.");
    });

    AddPage("Code", Icon_None, [] {
        static editor::lua_editor s_editor;
        static bool s_mcp_shown = false;

        float avail_h = ImGui::GetContentRegionAvail().y - 60.f;
        s_editor.render("##cortex_lua_editor", ImVec2(-1.f, avail_h));
        ImGui::Spacing();
        if (ImGui::Button("Execute##code_exec")) {
            std::string code = s_editor.get_text();
            if (!code.empty()) LuaVM::Execute(code);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##code_clr")) LuaVM::ClearLogs();
        ImGui::SameLine();
        if (ImGui::Button("MCP Bridge##code_mcp")) s_mcp_shown = !s_mcp_shown;

        if (s_mcp_shown) {
            ImGui::Separator();
            bool mcp_en = settings::mcp::enabled;
            if (ImGui::Checkbox("Enable MCP Server", &mcp_en)) {
                settings::mcp::enabled = mcp_en;
                if (settings::mcp::enabled)
                    notifications::add("Mcp Server enabled on port " + std::to_string(settings::mcp::port));
                else
                    notifications::add("Mcp Server disabled");
            }
            if (ImGui::Button("Copy Info##mcp_info")) {
                std::string info = "http://127.0.0.1:" + std::to_string(settings::mcp::port) + "/mcp";
                ImGui::SetClipboardText(info.c_str());
                notifications::add("Mcp connection info copied!");
            }
        }
    });

    AddPage("Servers", Icon_None, [] {
        static char s_job_id[128] = "";

        if (serverbrowser::browser->input_place_id[0] == '\0' || (serverbrowser::browser->all_servers.empty() && !serverbrowser::browser->is_loading)) {
            serverbrowser::browser->refresh_current_game();
            uint64_t pid = serverbrowser::browser->get_target_place_id();
            if (pid > 0 && serverbrowser::browser->all_servers.empty() && !serverbrowser::browser->is_loading) {
                serverbrowser::browser->cursor_history.clear();
                serverbrowser::browser->current_page = 1;
                serverbrowser::browser->fetch_servers_async(pid);
            }
        }

        if (Button("Rescan Servers", ImVec2(0,0), true)) {
            serverbrowser::browser->refresh_current_game();
            uint64_t pid = serverbrowser::browser->get_target_place_id();
            if (pid > 0) {
                serverbrowser::browser->cursor_history.clear();
                serverbrowser::browser->current_page = 1;
                serverbrowser::browser->fetch_servers_async(pid);
                notifications::add("Rescanning...");
            }
        }

        RowCombo("Sort", &serverbrowser::browser->sort_mode, s_server_sorts, 4);
        RowCheck("Exclude Full", &serverbrowser::browser->exclude_full);
        ImGui::Separator();

        std::lock_guard<std::mutex> lk(serverbrowser::browser->mtx);
        if (serverbrowser::browser->is_loading) {
            ImGui::TextColored(ImVec4(0.83f, 0.13f, 0.17f, 1.f), "%s", serverbrowser::browser->status_message.c_str());
        } else {
            uint64_t cpid = serverbrowser::browser->get_target_place_id();
            std::string ajid = serverbrowser::browser->active_job_id;
            for (size_t i = 0; i < serverbrowser::browser->filtered_servers.size(); i++) {
                const auto& s = serverbrowser::browser->filtered_servers[i];
                bool is_cur = (!ajid.empty() && s.id == ajid);
                std::ostringstream ss;
                ss << "#" << (i+1) << " | " << s.playing << "/" << s.max_players << " players";
                if (s.ping > 0) ss << " | " << s.ping << "ms";
                if (is_cur) ss << " [YOU]";
                if (is_cur) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f,0.13f,0.17f,1.f));
                ImGui::TextUnformatted(ss.str().c_str());
                if (is_cur) ImGui::PopStyleColor();
                std::string jlbl = "Join##sj" + std::to_string(i);
                std::string clbl = "Copy ID##sc" + std::to_string(i);
                if (ImGui::SmallButton(jlbl.c_str())) serverbrowser::browser->join_server(cpid, s.id);
                ImGui::SameLine();
                if (ImGui::SmallButton(clbl.c_str())) serverbrowser::browser->copy_job_id(s.id);
                ImGui::Spacing();
            }
        }

        if (serverbrowser::browser->current_page > 1) {
            if (Button("<< Prev", ImVec2(0,0), false)) serverbrowser::browser->prev_page();
            ImGui::SameLine();
        }
        if (!serverbrowser::browser->next_cursor.empty()) {
            if (Button("Next >>", ImVec2(0,0), false)) serverbrowser::browser->next_page();
        }

        ImGui::Separator();
        TextDim("Direct connect:");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##directjob", s_job_id, sizeof(s_job_id));
        if (Button("Join via Job ID", ImVec2(0,0), true)) {
            uint64_t pid = strtoull(serverbrowser::browser->input_place_id, nullptr, 10);
            if (pid > 0 && strlen(s_job_id) > 0) serverbrowser::browser->join_server(pid, s_job_id);
        }
        ImGui::SameLine();
        if (Button("Copy Script", ImVec2(0,0), false)) {
            uint64_t pid = strtoull(serverbrowser::browser->input_place_id, nullptr, 10);
            if (pid > 0 && strlen(s_job_id) > 0) serverbrowser::browser->copy_teleport_script(pid, s_job_id);
        }
    });

    AddPage("Waypoints", Icon_None, [] {
        static char s_wp_name[64] = "Base";
        static int  s_sel_wp = -1;

        std::vector<waypoints::Waypoint>& wps = waypoints::get_active_waypoints();

        BeginCard("wp_list", "Saved Waypoints");
        for (size_t i = 0; i < wps.size(); i++) {
            bool is_sel = (s_sel_wp == (int)i);
            std::ostringstream ss;
            ss << wps[i].name << " [" << (int)wps[i].pos.x << "," << (int)wps[i].pos.y << "," << (int)wps[i].pos.z << "]";
            if (is_sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f,0.13f,0.17f,1.f));
            if (ImGui::Selectable(ss.str().c_str(), is_sel)) {
                s_sel_wp = (int)i;
                strncpy_s(s_wp_name, wps[i].name.c_str(), sizeof(s_wp_name)-1);
            }
            if (is_sel) ImGui::PopStyleColor();
        }
        EndCard();

        BeginCard("wp_actions", "Controls");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##wpname", s_wp_name, sizeof(s_wp_name));
        if (Button("Add Current Pos", ImVec2(0,0), true)) {
            math::vector3 lp{};
            cache::entity_t le = cache::get_local_player();
            if (le.humanoid_root_part.address) { rbx::c_instance hrp(le.humanoid_root_part.address); lp = hrp.get_primitive().get_position(); }
            uint64_t pid=0, gid=0;
            if (game::datamodel && game::datamodel->address >= 0x10000) {
                pid = memory->read<uint64_t>(game::datamodel->address + Offsets::DataModel::PlaceId);
                gid = memory->read<uint64_t>(game::datamodel->address + Offsets::DataModel::GameId);
            }
            waypoints::add_waypoint(s_wp_name, lp, gid, pid);
            s_sel_wp = (int)wps.size()-1;
            notifications::add("Waypoint Added: " + std::string(s_wp_name));
        }
        ImGui::SameLine();
        if (Button("Teleport", ImVec2(0,0), false)) {
            if (s_sel_wp >= 0 && s_sel_wp < (int)wps.size()) {
                if (waypoints::teleport_to_waypoint((size_t)s_sel_wp)) notifications::add("Teleported to " + wps[s_sel_wp].name);
                else notifications::add("Teleport failed");
            }
        }
        if (Button("Delete", ImVec2(0,0), false)) {
            if (s_sel_wp >= 0 && s_sel_wp < (int)wps.size()) { waypoints::remove_waypoint((size_t)s_sel_wp); s_sel_wp=-1; notifications::add("Waypoint Deleted!"); }
        }
        ImGui::SameLine();
        if (Button("Clear All", ImVec2(0,0), false)) { waypoints::clear_waypoints(); s_sel_wp=-1; notifications::add("All Waypoints Cleared!"); }
        EndCard();
    });

    
    AddSection("SETTINGS");

    AddPage("Config", Icon_None, [] {
        static char s_cfg_name[64] = "";
        static int  s_sel_cfg = -1;
        std::vector<config::config_info_t> cfgs = config::get_config_list();

        BeginCard("cfg_list", "Configs");
        for (size_t i = 0; i < cfgs.size(); i++) {
            bool is_sel = (s_sel_cfg == (int)i);
            if (is_sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f,0.13f,0.17f,1.f));
            if (ImGui::Selectable(cfgs[i].name.c_str(), is_sel)) {
                s_sel_cfg = (int)i;
                strncpy_s(s_cfg_name, cfgs[i].name.c_str(), sizeof(s_cfg_name)-1);
            }
            if (is_sel) ImGui::PopStyleColor();
        }
        EndCard();

        BeginCard("cfg_actions", "Actions");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##cfgname", s_cfg_name, sizeof(s_cfg_name));
        ImGui::Spacing();
        float btnW = (ImGui::GetContentRegionAvail().x - 6.f) * 0.5f;
        if (Button("Save", ImVec2(btnW, 0), true)) {
            if (strlen(s_cfg_name)>0) { config::save_config(s_cfg_name); notifications::add("Config Saved!"); }
        }
        ImGui::SameLine(0, 6.f);
        if (Button("Load", ImVec2(btnW, 0), false)) {
            if (s_sel_cfg>=0 && s_sel_cfg<(int)cfgs.size()) { config::load_config(cfgs[s_sel_cfg].name); notifications::add("Config Loaded: "+cfgs[s_sel_cfg].name); }
        }
        ImGui::Spacing();
        if (Button("Delete", ImVec2(btnW, 0), false)) {
            if (s_sel_cfg>=0 && s_sel_cfg<(int)cfgs.size()) { config::delete_config(cfgs[s_sel_cfg].name); s_sel_cfg=-1; notifications::add("Config Deleted!"); }
        }
        ImGui::SameLine(0, 6.f);
        if (Button("Set Autoload", ImVec2(btnW, 0), false)) {
            if (s_sel_cfg>=0 && s_sel_cfg<(int)cfgs.size()) { config::set_autoload(cfgs[s_sel_cfg].name); notifications::add("Autoload Set!"); }
        }
        ImGui::Spacing();
        if (Button("Export Clipboard", ImVec2(btnW, 0), false)) { config::export_config_to_clipboard(); notifications::add("Config exported!"); }
        ImGui::SameLine(0, 6.f);
        if (Button("Import Clipboard", ImVec2(btnW, 0), false)) { config::import_config_from_clipboard(); notifications::add("Config imported!"); }
        ImGui::Spacing();
        if (Button("Open Config Folder", ImVec2(-1.f, 0), false))
            ShellExecuteA(NULL,"open",config::get_config_directory().c_str(),NULL,NULL,SW_SHOWNORMAL);
        EndCard();

        BeginCard("cfg_security", "Security");
        RowToggle("Stream Proof", &settings::streamproof);
        EndCard();

        BeginCard("cfg_opts", "Options");
        RowCheck("Disable Confirmations", &settings::config_system::disable_confirmations);
        RowCheck("Confirm on Unload", &settings::general::confirm_unload);
        RowCheck("Disable Scroll Bars", &settings::misc::disable_scrollbars);
        RowCheck("Bot Support", &settings::misc::bot_support);
        if (settings::misc::bot_support) RowCheck("Only Workspace Bots", &settings::misc::only_workspace_bots);
        EndCard();

        BeginCard("unload_card", "Unload");
        TextDim("Terminate the cheat process.");
        if (Button("Unload Cortex", ImVec2(0,0), true)) {
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
        EndCard();
    });

    AddPage("Style", Icon_None, [] {
        BeginCard("watermark_card", "Watermark", "Overlay pill with FPS and time.", &settings::watermark::enabled);
        if (settings::watermark::enabled) {
            RowCheck("Show Username", &settings::watermark::show_username);
            RowCheck("Show FPS", &settings::watermark::show_fps);
            RowCheck("Show Game ID", &settings::watermark::show_game_id);
            RowCheck("Show Place ID", &settings::watermark::show_place_id);
            RowCheck("Show Job ID", &settings::watermark::show_job_id);
            RowCheck("Show CPU & RAM", &settings::watermark::show_cpu_ram);
            RowCheck("Show Time", &settings::watermark::show_time);
        }
        EndCard();

        BeginCard("notify_card", "Notifications", "Toast popups.", &settings::notifications::enabled);
        if (settings::notifications::enabled) {
            RowCheck("Hit Notifications", &settings::notifications::hit);
            RowCheck("Kill Notifications", &settings::notifications::kill);
            RowCheck("Welcome Notification", &settings::notifications::welcome);
            RowCombo("Position", &settings::notifications::position, s_notify_positions, 4);
            RowSlider("Duration", &settings::notifications::duration, 1.0f, 10.0f);
            if (Button("Send Test##ntfy", ImVec2(0,0), false)) notifications::add("This is a preview notification!");
        }
        EndCard();

        BeginCard("theme_card", "Themes");
        int preset = settings::theme::preset;
        const char* const* theme_names_ptr = Theme::GetThemeNames();
        int theme_count = Theme::GetThemeCount();
        if (preset >= 0 && preset < theme_count) {
            RowCombo("Theme Preset", &settings::theme::preset, theme_names_ptr, theme_count);
            if (settings::theme::preset != preset)
                Theme::ApplyTheme(settings::theme::preset);
        }
        if (Button("Open Config Folder##themes", ImVec2(0,0), false))
            ShellExecuteA(NULL,"open",config::get_config_directory().c_str(),NULL,NULL,SW_SHOWNORMAL);
        EndCard();
    });

    AddPage("NPC Mgr", Icon_None, [] {
        static char s_npc_path[256] = "";

        BeginCard("npc_paths", "NPC Paths");
        for (const auto& p : settings::playerlist::active_npc_paths) {
            ImGui::TextUnformatted(p.c_str());
            ImGui::SameLine();
            std::string rlbl = "X##npcr" + p;
            if (ImGui::SmallButton(rlbl.c_str())) { settings::playerlist::active_npc_paths.erase(p); break; }
        }
        EndCard();

        BeginCard("npc_add", "Add Path");
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputText("##npcpath", s_npc_path, sizeof(s_npc_path));
        if (Button("Add Path", ImVec2(0,0), true)) {
            if (strlen(s_npc_path)>0) { settings::playerlist::active_npc_paths.insert(s_npc_path); notifications::add("NPC Path Added!"); }
        }
        ImGui::SameLine();
        if (Button("Clear All", ImVec2(0,0), false)) { settings::playerlist::active_npc_paths.clear(); notifications::add("All NPC Paths Cleared!"); }
        EndCard();

        BeginCard("npc_opts", "NPC Options");
        RowCheck("Bot Support", &settings::misc::bot_support);
        RowCheck("Only Workspace Bots", &settings::misc::only_workspace_bots);
        EndCard();
    });
}

} 
