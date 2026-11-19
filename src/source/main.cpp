#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <timeapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "winmm.lib")

#include <thread>
#include <chrono>

#include <system/globals.h>
#include <game/sdk.h>
#include <game/game/game.h>
#include <game/cache/core/cache.h>
#include <system/logger/logger.h>
#include <menu/render/render.h>
#include <hacks/system/performance/performance.h>
#include <hacks/luau/vm/vm.h>
#include <system/scanner/rescan.h>
#include <game/wallcheck/wallcheck.h>
#include <system/protection/protection.h>

#include <hacks/combat/aimbot/aimbot.h>
#include <hacks/combat/trigger/trigger.h>
#include <hacks/exploits/locomotion/movement/movement.h>
#include <hacks/combat/silent/mouse/mouse.h>
#include <hacks/combat/silent/raycast/raycast.h>
#include <hacks/combat/hbe/hbe.h>
#include <hacks/visuals/chams/chams.h>
#include <hacks/visuals/chams/native/native.h>
#include <hacks/visuals/chams/highlight/highlight.h>
#include <hacks/exploits/utility/misc/misc.h>
#include <hacks/system/config/config.h>
#include <hacks/visuals/lighting/lighting.h>
#include <hacks/visuals/skyshader/skyshader.h>
#include <hacks/system/settings/settings.h>
#include <hacks/exploits/character/avatar/avatar.h>
#include <hacks/exploits/character/skinchanger/skinchanger.h>
#include <hacks/exploits/character/dhskinchanger/dhskinchanger.h>
#include <hacks/exploits/character/changer/changer.h>
#include <hacks/exploits/character/materialchanger/materialchanger.h>
#include <hacks/exploits/utility/npcsystem/npcsystem.h>
#include <hacks/system/notifications/notifications.h>
#include <hacks/visuals/model/modelviewer.h>
#include <hacks/exploits/environment/btools/btools.h>
#include <hacks/exploits/locomotion/unified/unifiedexploits.h>
#include <hacks/exploits/audio/hitsound/hitsounds.h>
#include <hacks/exploits/environment/skybox/skybox.h>
#include <hacks/exploits/environment/explorer/explorer.h>
#include <hacks/exploits/utility/freecam/freecam.h>
#include <hacks/luau/mcp/mcp_server.h>
#include <system/utility/creator/creator.h>
#include <menu/menu/settings/functions.h>

namespace {

bool should_render_ui()
{
	if (var->gui.menu_opened)
		return true;

	HWND hwnd = GetForegroundWindow();
	HWND roblox_window = game::get_roblox_window();
	HWND overlay_window = (render && render->detail) ? render->detail->window : nullptr;

	if (roblox_window != nullptr && IsWindow(roblox_window))
		return (hwnd == roblox_window || hwnd == overlay_window);

	return true;
}

void setup_console()
{
	AllocConsole();
	FILE* pCout;
	FILE* pCin;
	FILE* pCerr;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	freopen_s(&pCin, "CONIN$", "r", stdin);
	freopen_s(&pCerr, "CONOUT$", "w", stderr);

	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE)
	{
		DWORD dwMode = 0;
		GetConsoleMode(hOut, &dwMode);
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, dwMode);
	}

	HWND console_window = GetConsoleWindow();
	if (console_window)
		ShowWindow(console_window, SW_SHOW);
}

void reset_globals()
{
	game::datamodel = { 0 };
	game::visualengine = { 0 };
}

void wait_for_roblox()
{
	if (memory->is_attached() && memory->is_alive())
		return;

	if (memory->is_attached())
		memory->detach();

	Cheat::Console::Log(Cheat::Console::Color::Yellow, "waiting for roblox");
	while (!memory->attach_to_process(BINARY_NAME))
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void on_roblox_attached(bool reattached)
{
	memory->find_module_address(BINARY_NAME);
	Cheat::Console::Clear();
	Cheat::Console::Log(Cheat::Console::Color::Green, reattached ? "reattached" : "attached");
	if (reattached)
		Cheat::Console::DumpLastCrash();
}

void refresh_game_instances()
{
	game::datamodel = { rbx::c_datamodel::get() };
	game::visualengine = { rbx::c_visualengine::get() };
	wallcheck->cache_workspace();
}

void init_environment()
{
	paw_guard::init();
	SetProcessDPIAware();
	setup_console();
	SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG {
		return EXCEPTION_EXECUTE_HANDLER;
	});
	timeBeginPeriod(1);

	SetConsoleCtrlHandler([](DWORD) -> BOOL {
		return FALSE;
	}, TRUE);

	SetConsoleTitleA("Cortex");

	Cheat::Console::Log(Cheat::Console::Color::Gray, "configs");
	config::ensure_config_directory();
	settings::misc::hide_console = false;
	LuaVM::Initialize();
}

void init_renderer()
{
	if (!render->create_window() || !render->create_device() || !render->create_imgui())
	{
		Cheat::Console::Log(Cheat::Console::Color::Red, "failed to initialize renderer");
		std::this_thread::sleep_for(std::chrono::seconds(10));
		std::exit(0);
	}

	config::load_autoload();
	config::auto_load_theme();

	if (render && render->detail && render->detail->device && render->detail->device_context)
	{
		ModelViewer::g_model_viewer.Init(render->detail->device, render->detail->device_context);
		ModelViewer::g_player_model_viewer.Init(render->detail->device, render->detail->device_context);
	}

	creator::install_console_handler();
}

void init_features()
{
	wait_for_roblox();
	on_roblox_attached(false);
	refresh_game_instances();

	std::thread(rescan::rescan_game).detach();
	std::thread(rescan::rescan_process).detach();
	std::thread(cache::run).detach();
	std::thread(movement::run).detach();

	std::thread(aimbot::run).detach();
	std::thread(triggerbot::run).detach();
	std::thread(raycast_silentaim::run).detach();
	std::thread(silentaim::run).detach();

	std::thread(misc::run).detach();
	std::thread(hitsound::run).detach();
	std::thread(exploits::animationchanger::run).detach();
	std::thread(exploits::run_unified).detach();
	std::thread(btools::run).detach();
	std::thread(freecam::run).detach();
	std::thread(mcp::run).detach();
	std::thread(hacks::engine_chams::thread).detach();
	std::thread(nativechams::Run).detach();
	std::thread(Cheat::Features::SkyShader::Run).detach();
	std::thread(highlightchams::Run).detach();

	lighting::run_all();

	config::load_autoload();
	config::auto_load_theme();

	settings::notifications::enabled = true;
	settings::notifications::welcome = true;
	settings::notifications::hit = true;
	settings::notifications::kill = true;
	notifications::add_welcome();
}

void check_connection_health()
{
	if (memory->is_attached() && !memory->is_alive())
	{
		DWORD code = 0;
		if (!memory->get_exit_code(&code))
			code = 0xFFFFFFFF;

		reset_globals();
		creator::notify_datamodel_lost();
		Cheat::Console::Clear();
		Cheat::Console::DumpCrash(code);

		memory->detach();
		wait_for_roblox();
		on_roblox_attached(true);
		refresh_game_instances();
	}
}

void handle_input(HWND roblox_window, HWND overlay_window)
{
	static bool s_toggle_key_was_down = false;
	bool toggle_key_down = ((GetAsyncKeyState(VK_INSERT) & 0x8000) != 0) || ((GetAsyncKeyState(VK_END) & 0x8000) != 0);
	if (toggle_key_down && !s_toggle_key_was_down)
	{
		var->gui.menu_opened = !var->gui.menu_opened;
		if (var->gui.menu_opened)
		{
			var->gui.current_section[0] = true;
			ModelViewer::g_model_viewer.RefreshAvatar();
		}
	}
	s_toggle_key_was_down = toggle_key_down;

	const bool interactive_ui = var->gui.menu_opened;
	static bool s_last_menu_state = false;
	if (interactive_ui != s_last_menu_state)
	{
		s_last_menu_state = interactive_ui;
		if (var->gui.menu_opened && overlay_window)
			SetForegroundWindow(overlay_window);
		else if (!interactive_ui && roblox_window && IsWindow(roblox_window))
			SetForegroundWindow(roblox_window);
	}
}

void update_overlay_style(HWND overlay_window, bool interactive_ui)
{
	static LONG s_current_exstyle = 0;
	LONG target_style = interactive_ui
		? (WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED)
		: (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);

	if (s_current_exstyle != target_style)
	{
		SetWindowLong(overlay_window, GWL_EXSTYLE, target_style);
		SetWindowPos(overlay_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		s_current_exstyle = target_style;
	}
}

void render_frame()
{
	render->start_render();

	HWND roblox_window = game::get_roblox_window();
	HWND overlay_window = (render && render->detail) ? render->detail->window : nullptr;

	static bool s_was_attached = true;
	const bool roblox_attached = memory && memory->is_attached();
	if (!roblox_attached)
	{
		s_was_attached = false;
		if (overlay_window && IsWindow(overlay_window) && IsWindowVisible(overlay_window))
			ShowWindow(overlay_window, SW_HIDE);
		render->end_render();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		return;
	}

	if (!s_was_attached)
	{
		s_was_attached = true;
		if (overlay_window && IsWindow(overlay_window))
		{
			ShowWindow(overlay_window, SW_SHOW);
			SetWindowPos(overlay_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}

	if (!should_render_ui())
	{
		if (overlay_window && IsWindow(overlay_window))
			update_overlay_style(overlay_window, false);
		render->end_render();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		return;
	}

	render->render_visuals();
	handle_input(roblox_window, overlay_window);

	if (overlay_window && IsWindow(overlay_window))
	{
		update_overlay_style(overlay_window, var->gui.menu_opened);
		render->render_menu();

		if (settings::misc::npc_system_window && (var->gui.menu_opened || settings::misc::npc_system_pinned))
		{
			ImVec2 main_pos = ImVec2(100.0f, 100.0f);
			ImVec2 main_size = ImVec2(700.0f, 500.0f);
			npcsystem::render_window(&settings::misc::npc_system_window, main_pos, main_size);
		}

		if (settings::misc::performance_window)
			performance::render_window(&settings::misc::performance_window);

		if (settings::misc::explorer_window && (var->gui.menu_opened || settings::misc::explorer_pinned))
			explorer::explorer->render_window(&settings::misc::explorer_window);
	}

	render->end_render();
}

void limit_framerate(std::chrono::high_resolution_clock::time_point frame_start)
{
	if (settings::performance::main_loop_delay > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(settings::performance::main_loop_delay));
	}
	else if (!settings::vsync && settings::misc::unlock_fps && settings::misc::fps_cap > 0)
	{
		auto target_frame_duration = std::chrono::microseconds(1000000 / settings::misc::fps_cap);
		while (std::chrono::high_resolution_clock::now() - frame_start < target_frame_duration)
		{
			std::this_thread::yield();
		}
	}
}

}

std::int32_t main()
{
	init_environment();
	init_renderer();

	std::thread(init_features).detach();

	render->running = true;
	var->gui.menu_opened = false;
	var->gui.menu_alpha = 0.0f;
	render->menu_open = false;
	render->main_window_open = false;

	while (true)
	{
		auto frame_start = std::chrono::high_resolution_clock::now();
		check_connection_health();

		try
		{
			render_frame();
		}
		catch (...)
		{
			logger->log<ERR>("render error ");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		limit_framerate(frame_start);
	}
}
