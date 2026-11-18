// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  window.h - Xbox (UWP) window handling
//
//  There is exactly one window on the Xbox: the app's
//  CoreWindow. MAME's render target is drawn into it with
//  bgfx (Direct3D 11 swap chain for the CoreWindow).
//
//============================================================
#ifndef MAME_OSD_UWP_WINDOW_H
#define MAME_OSD_UWP_WINDOW_H

#pragma once

#include "emucore.h"
#include "render.h"

#include "modules/osdwindow.h"

#include <memory>


//============================================================
//  helpers provided by uwpmain.cpp
//============================================================

// ABI pointer (an IUnknown *) to the app's CoreWindow, for bgfx
void *uwp_core_window_abi();

// size of the CoreWindow in physical pixels
osd_dim uwp_core_window_size();

// does the app currently have focus / is it visible?
bool uwp_app_has_focus();

// pump the CoreWindow event queue without blocking
void uwp_process_core_events();

// show or hide the system pointer
void uwp_set_pointer_visible(bool visible);


//============================================================
//  uwp_window_info
//============================================================

class uwp_window_info : public osd_window_t<void *>
{
public:
	uwp_window_info(
			running_machine &machine,
			render_module &renderprovider,
			int index,
			const std::shared_ptr<osd_monitor_info> &monitor,
			const osd_window_config *config);

	static std::unique_ptr<uwp_window_info> create(
			running_machine &machine,
			render_module &renderprovider,
			int index,
			const std::shared_ptr<osd_monitor_info> &monitor,
			const osd_window_config *config);

	virtual osd_dim get_size() override;

	virtual void update() override;

	virtual void capture_pointer() override { }
	virtual void release_pointer() override { }
	virtual void show_pointer() override { uwp_set_pointer_visible(true); }
	virtual void hide_pointer() override { uwp_set_pointer_visible(false); }

	virtual void complete_destroy() override;

private:
	bool complete_create();
};

#endif // MAME_OSD_UWP_WINDOW_H
