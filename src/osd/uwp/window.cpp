// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  window.cpp - Xbox (UWP) window handling
//
//============================================================

#include "window.h"

#include "winmain.h"

#include "modules/monitor/monitor_module.h"
#include "modules/render/render_module.h"

// MAME headers
#include "emu.h"
#include "render.h"

#include <chrono>


//============================================================
//  uwp_window_info
//============================================================

uwp_window_info::uwp_window_info(
		running_machine &machine,
		render_module &renderprovider,
		int index,
		const std::shared_ptr<osd_monitor_info> &monitor,
		const osd_window_config *config)
	: osd_window_t(machine, renderprovider, index, monitor, *config)
{
	// the Xbox only has the one full-screen CoreWindow
	m_fullscreen = true;
	m_prescale = video_config.prescale;
}


std::unique_ptr<uwp_window_info> uwp_window_info::create(
		running_machine &machine,
		render_module &renderprovider,
		int index,
		const std::shared_ptr<osd_monitor_info> &monitor,
		const osd_window_config *config)
{
	auto window = std::make_unique<uwp_window_info>(machine, renderprovider, index, monitor, config);

	window->create_target();

	if (!window->complete_create())
		fatalerror("Unable to complete window creation\n");

	return window;
}


bool uwp_window_info::complete_create()
{
	void *const corewindow = uwp_core_window_abi();
	if (!corewindow)
		return false;

	set_platform_window(corewindow);

	renderer_create();
	if (renderer().create())
		return false;

	return true;
}


osd_dim uwp_window_info::get_size()
{
	return uwp_core_window_size();
}


void uwp_window_info::update()
{
	// draw straight away: everything happens on the CoreWindow's thread
	if ((platform_window() != nullptr) && (target() != nullptr) && has_renderer())
	{
		m_primlist = renderer().get_primitives();
		if (m_primlist)
			renderer().draw(0);
	}
}


void uwp_window_info::complete_destroy()
{
	// the CoreWindow belongs to the app; nothing to destroy here
	set_platform_window(nullptr);
}


//============================================================
//  windows_osd_interface window hooks
//============================================================

bool windows_osd_interface::window_init()
{
	return true;
}


int windows_osd_interface::window_count()
{
	return osd_common_t::window_list().size();
}


void windows_osd_interface::add_audio_to_recording(const int16_t *buffer, int samples_this_frame)
{
	if (osd_common_t::window_list().empty())
		return;

	auto const &window = osd_common_t::window_list().front(); // we only record on the first window
	if (window && window->has_renderer())
		window->renderer().add_audio_to_recording(buffer, samples_this_frame);
}


void windows_osd_interface::window_exit()
{
	// if we hid the pointer during the emulation, show it
	if (!osd_common_t::s_window_list.empty())
		osd_common_t::s_window_list.front()->show_pointer();

	// free all the windows
	while (!osd_common_t::s_window_list.empty())
	{
		auto window = std::move(osd_common_t::s_window_list.back());
		s_window_list.pop_back();
		window->destroy();
	}
}


bool windows_osd_interface::has_focus() const
{
	return uwp_app_has_focus();
}


void windows_osd_interface::process_events()
{
	auto const currticks = std::chrono::steady_clock::now();

	// update once every 1/8th of a second
	if (currticks < (m_last_event_check + std::chrono::milliseconds(1000 / 8)))
		return;

	process_events(true, false);
}


void windows_osd_interface::process_events(bool ingame, bool nodispatch)
{
	m_last_event_check = std::chrono::steady_clock::now();
	uwp_process_core_events();
}


//============================================================
//  osd_set_aggressive_input_focus
//============================================================

void osd_set_aggressive_input_focus(bool aggressive_focus)
{
	// the Xbox app always has the controller's focus
}
