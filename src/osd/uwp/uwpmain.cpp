// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  uwpmain.cpp - Xbox (UWP) app entry point and OSD core
//
//  The app is a plain CoreApplication (C++/WinRT, no XAML).
//  MAME runs on the CoreWindow's own thread, inside Run(),
//  and pumps the window's events while it emulates.
//
//  Files:
//    - read-only data (hash, plugins, bgfx shaders, artwork)
//      comes from the app's install folder
//    - settings, saves and the log go to the app's LocalState
//    - ROMs are looked for on every drive in the usual places
//      (X:\roms, X:\MAME\roms, X:\ES-DE\ROMs\mame, ...)
//
//============================================================

// MAMEOS headers
#include "winmain.h"
#include "window.h"
#include "winopts.h"
#include "winfile.h"

#include "modules/monitor/monitor_module.h"
#include "modules/render/render_module.h"
#include "modules/lib/osdlib.h"

// MAME headers
#include "emu.h"
#include "emuopts.h"
#include "main.h"
#include "uiinput.h"
#include "strconv.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// standard windows headers
#include <windows.h>
#include <wtypes.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>

#undef interface

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;


// from uwpinput.cpp
void uwp_input_start();
unsigned uwp_input_count();


//============================================================
//  GLOBAL VARIABLES
//============================================================

osd_video_config video_config;

namespace {

CoreWindow g_window{ nullptr };
bool g_has_focus = true;
bool g_visible = true;
running_machine *g_current_machine = nullptr;

// per-window configuration data
osd_window_config s_window_config[MAX_VIDEO_WINDOWS];


//============================================================
//  small helpers
//============================================================

std::string utf8(std::wstring_view text)
{
	return osd::text::from_wstring(std::wstring(text));
}

bool directory_exists(std::string const &path)
{
	DWORD const attr = GetFileAttributesW(osd::text::to_wstring(path).c_str());
	return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

void make_directory(std::string const &path)
{
	CreateDirectoryW(osd::text::to_wstring(path).c_str(), nullptr);
}

std::string local_state_folder()
{
	try
	{
		return utf8(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());
	}
	catch (...)
	{
		return std::string(".");
	}
}


//============================================================
//  file_log_output - everything MAME prints goes to
//  LocalState\mame-log.txt, since there's no console
//============================================================

class file_log_output : public osd_output
{
public:
	file_log_output(std::string const &path) : m_file(osd::text::to_wstring(path), std::ios::out | std::ios::trunc)
	{
	}

	virtual void output_callback(osd_output_channel channel, const util::format_argument_pack<char> &args) override
	{
		{
			std::lock_guard<std::mutex> guard(m_mutex);
			if (m_file.is_open())
			{
				switch (channel)
				{
				case OSD_OUTPUT_CHANNEL_ERROR:   m_file << "[error] ";   break;
				case OSD_OUTPUT_CHANNEL_WARNING: m_file << "[warning] "; break;
				default: break;
				}
				util::stream_format(m_file, args);
				m_file.flush();
			}
		}
		chain_output(channel, args);
	}

	void line(std::string const &text)
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		if (m_file.is_open())
		{
			m_file << text << '\n';
			m_file.flush();
		}
	}

private:
	std::ofstream m_file;
	std::mutex m_mutex;
};


//============================================================
//  build_arguments - MAME's command line on the Xbox
//============================================================

std::vector<std::string> build_arguments(std::string const &local, file_log_output &log)
{
	std::vector<std::string> args;
	args.emplace_back("mame");

	// writable folders in LocalState
	char const *const writable[] = { "cfg", "nvram", "inp", "sta", "snap", "diff", "comments", "share", "roms" };
	for (char const *dir : writable)
		make_directory(local + "\\" + dir);

	// find ROM folders on every drive
	std::string rompath = local + "\\roms";
	char const *const romdirs[] = {
			"\\roms", "\\MAME\\roms", "\\MAME\\ROMs", "\\ROMs\\mame", "\\ROMs\\arcade",
			"\\ES-DE\\ROMs\\mame", "\\ES-DE\\ROMs\\arcade", "\\ES-DE\\ROMs\\fbneo", "\\MAME" };
	for (char drive = 'C'; drive <= 'Z'; ++drive)
	{
		std::string const root = std::string(1, drive) + ":";
		for (char const *dir : romdirs)
		{
			std::string const candidate = root + dir;
			if (directory_exists(candidate))
			{
				rompath += ";" + candidate;
				log.line("ROM folder found: " + candidate);
			}
		}
	}

	args.insert(args.end(), {
			"-rompath",          rompath,
			"-inipath",          local,
			"-homepath",         local,
			"-cfg_directory",    local + "\\cfg",
			"-nvram_directory",  local + "\\nvram",
			"-input_directory",  local + "\\inp",
			"-state_directory",  local + "\\sta",
			"-snapshot_directory", local + "\\snap",
			"-diff_directory",   local + "\\diff",
			"-comment_directory", local + "\\comments",
			"-share_directory",  local + "\\share",
			"-video",            "bgfx",
			"-bgfx_backend",     "d3d11",
			"-sound",            "xaudio2",
			"-joystickprovider", "xinput",
			"-keyboardprovider", "none",
			"-mouseprovider",    "none",
			"-lightgunprovider", "none",
			"-monitorprovider",  "uwp",
			"-uifontprovider",   "none",
			"-output",           "none",
			"-joystick",
			"-skip_gameinfo",
			"-verbose" });

	return args;
}

} // anonymous namespace


//============================================================
//  helpers used by window.cpp and uwpmonitor.cpp
//============================================================

void *uwp_core_window_abi()
{
	return g_window ? winrt::get_abi(g_window) : nullptr;
}

osd_dim uwp_core_window_size()
{
	if (!g_window)
		return osd_dim(1920, 1080);

	Rect const bounds = g_window.Bounds();
	double scale = 1.0;
	try
	{
		scale = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
	}
	catch (...)
	{
	}
	if (scale <= 0.0)
		scale = 1.0;

	return osd_dim(int(bounds.Width * scale + 0.5), int(bounds.Height * scale + 0.5));
}

bool uwp_app_has_focus()
{
	return g_has_focus && g_visible;
}

void uwp_process_core_events()
{
	if (g_window)
		g_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
}

void uwp_set_pointer_visible(bool visible)
{
	if (!g_window)
		return;
	if (visible)
		g_window.PointerCursor(CoreCursor(CoreCursorType::Arrow, 0));
	else
		g_window.PointerCursor(nullptr);
}


//============================================================
//  windows_osd_interface - core
//============================================================

void windows_osd_interface::output_oslog(const char *buffer)
{
	if (IsDebuggerPresent())
		OutputDebugStringA(buffer);
	osd_printf_verbose("%s", buffer);
}


windows_osd_interface::windows_osd_interface(windows_options &options)
	: osd_common_t(options)
	, m_options(options)
	, m_com_status(false) // C++/WinRT already started the apartment
	, m_last_event_check(std::chrono::steady_clock::time_point::min())
{
}


windows_osd_interface::~windows_osd_interface()
{
}


void windows_osd_interface::init(running_machine &machine)
{
	// call our parent
	osd_common_t::init(machine);

	auto &options = downcast<windows_options &>(machine.options());

	// determine if we are benchmarking, and adjust options appropriately
	int const bench = options.bench();
	if (bench > 0)
	{
		options.set_value(OPTION_SLEEP, false, OPTION_PRIORITY_MAXIMUM);
		options.set_value(OPTION_THROTTLE, false, OPTION_PRIORITY_MAXIMUM);
		options.set_value(OSDOPTION_SOUND, "none", OPTION_PRIORITY_MAXIMUM);
		options.set_value(OSDOPTION_VIDEO, "none", OPTION_PRIORITY_MAXIMUM);
		options.set_value(OPTION_SECONDS_TO_RUN, bench, OPTION_PRIORITY_MAXIMUM);
	}

	// get number of processors
	char const *const stemp = options.numprocessors();
	osd_num_processors = 0;
	if (strcmp(stemp, "auto") != 0)
	{
		osd_num_processors = atoi(stemp);
		if (osd_num_processors < 1)
		{
			osd_printf_warning("Warning: numprocessors < 1 doesn't make much sense. Assuming auto ...\n");
			osd_num_processors = 0;
		}
	}

	// initialize the subsystems
	osd_common_t::init_subsystems();

	// hook up the debugger log
	if (options.oslog())
		machine.add_logerror_callback(&windows_osd_interface::output_oslog);

	// initialize sockets
	win_init_sockets();

	// note the existence of a machine
	g_current_machine = &machine;
}


void windows_osd_interface::osd_exit()
{
	// no longer have a machine
	g_current_machine = nullptr;

	// cleanup sockets
	win_cleanup_sockets();

	osd_common_t::osd_exit();

	// one last pass at events
	process_events(false, false);
}


//============================================================
//  windows_osd_interface - video
//============================================================

bool windows_osd_interface::video_init()
{
	// extract data from the options
	extract_video_config();

	// initialize the window system so we can make windows
	window_init();

	// the Xbox has one screen: the CoreWindow
	auto &options = downcast<windows_options &>(machine().options());
	auto window = uwp_window_info::create(
			machine(),
			*m_render,
			0,
			m_monitor_module->pick_monitor(options, 0),
			&s_window_config[0]);
	s_window_list.emplace_back(std::move(window));

	return true;
}


void windows_osd_interface::video_exit()
{
	window_exit();
}


void windows_osd_interface::update(bool skip_redraw)
{
	osd_common_t::update(skip_redraw);

	// keep the CoreWindow responsive
	uwp_process_core_events();

	// if we're not skipping this redraw, update all windows
	if (!skip_redraw)
	{
		for (const auto &window : osd_common_t::s_window_list)
			window->update();
	}
}


void windows_osd_interface::input_update(bool relative_reset)
{
	process_events(true, false);
	poll_input_modules(relative_reset);
}


void windows_osd_interface::check_osd_inputs()
{
	// no full-screen toggling or render snapshots on the Xbox
}


void windows_osd_interface::extract_video_config()
{
	video_config.windowed      = false;
	video_config.prescale      = options().prescale();
	video_config.filter        = options().filter();
	video_config.numscreens    = 1;
	video_config.waitvsync     = options().wait_vsync();
	video_config.syncrefresh   = options().sync_refresh();
	video_config.triplebuf     = options().triple_buffer();
	video_config.switchres     = false;

	if (video_config.prescale < 1 || video_config.prescale > 20)
		video_config.prescale = 1;

	osd_dim const size = uwp_core_window_size();
	s_window_config[0].width = size.width();
	s_window_config[0].height = size.height();
	s_window_config[0].depth = 0;
	s_window_config[0].refresh = 0;
	s_window_config[0].aspect = float(size.width()) / float(size.height());
}


//============================================================
//  windows_osd_interface - input
//============================================================

bool windows_osd_interface::should_hide_mouse() const
{
	return true;
}


bool windows_osd_interface::handle_input_event(input_event eventid, const void *eventdata) const
{
	return false;
}


void windows_osd_interface::customize_input_type_list(std::vector<input_type_entry> &typelist)
{
	for (input_type_entry &entry : typelist)
	{
		switch (entry.type())
		{
		// controller: View + X opens MAME's menu
		case IPT_UI_MENU:
			entry.defseq(SEQ_TYPE_STANDARD).set(KEYCODE_TAB, input_seq::or_code, JOYCODE_SELECT, JOYCODE_BUTTON3);
			break;

		// controller: View + Menu leaves the game
		case IPT_UI_CANCEL:
			entry.defseq(SEQ_TYPE_STANDARD).set(KEYCODE_ESC, input_seq::or_code, JOYCODE_SELECT, JOYCODE_START);
			break;

		default:
			break;
		}
	}
}


//============================================================
//  App - the CoreApplication view
//============================================================

namespace {

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
	IFrameworkView CreateView()
	{
		return *this;
	}

	void Initialize(CoreApplicationView const &)
	{
		// use the whole TV: no TV-safe border, no 2x scaling
		try
		{
			ApplicationViewScaling::TrySetDisableLayoutScaling(true);
		}
		catch (...)
		{
		}
	}

	void Load(hstring const &)
	{
	}

	void Uninitialize()
	{
	}

	void SetWindow(CoreWindow const &window)
	{
		g_window = window;

		window.Activated([] (auto const &, WindowActivatedEventArgs const &args)
		{
			g_has_focus = (args.WindowActivationState() != CoreWindowActivationState::Deactivated);
		});

		window.VisibilityChanged([] (auto const &, VisibilityChangedEventArgs const &args)
		{
			g_visible = args.Visible();
		});

		window.Closed([] (auto const &, auto const &)
		{
			if (g_current_machine)
				g_current_machine->schedule_exit();
		});

		// the controller's B button sends "back": keep it for MAME
		try
		{
			SystemNavigationManager::GetForCurrentView().BackRequested([] (auto const &, BackRequestedEventArgs const &args)
			{
				args.Handled(true);
			});
		}
		catch (...)
		{
		}

		try
		{
			ApplicationView::GetForCurrentView().SetDesiredBoundsMode(ApplicationViewBoundsMode::UseCoreWindow);
		}
		catch (...)
		{
		}
	}

	void Run()
	{
		g_window.Activate();
		uwp_process_core_events();

		std::string const local = local_state_folder();
		file_log_output log(local + "\\mame-log.txt");
		log.line("MAME for Xbox starting");
		log.line("LocalState: " + local);

		// start the controllers and give them a moment to show up
		uwp_input_start();
		auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
		while ((uwp_input_count() == 0) && (std::chrono::steady_clock::now() < deadline))
		{
			uwp_process_core_events();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
		log.line("Controllers connected: " + std::to_string(uwp_input_count()));

		osd_dim const size = uwp_core_window_size();
		log.line("Screen: " + std::to_string(size.width()) + "x" + std::to_string(size.height()));

		std::vector<std::string> args = build_arguments(local, log);

		int result;
		{
			windows_options options;
			windows_osd_interface osd(options);
			osd_output::push(&log);
			osd.register_options();
			result = emulator_info::start_frontend(options, osd, args);
			osd_output::pop(&log);
		}
		log.line("MAME finished with result " + std::to_string(result));

		CoreApplication::Exit();
	}
};

} // anonymous namespace


//============================================================
//  audio property keys used by the XAudio2 module
//  (the Windows build gets these from another library)
//============================================================

extern "C" const PROPERTYKEY PKEY_AudioEndpoint_PhysicalSpeakers = { { 0x1da5d803, 0xd492, 0x4edd, { 0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e } }, 3 };
extern "C" const PROPERTYKEY PKEY_AudioEngine_DeviceFormat = { { 0xf19f064d, 0x082c, 0x4e27, { 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c } }, 0 };


//============================================================
//  WinMain - the app's entry point
//============================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	winrt::init_apartment();
	CoreApplication::Run(make<App>());
	return 0;
}
