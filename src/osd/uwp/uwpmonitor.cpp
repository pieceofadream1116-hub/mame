// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  uwpmonitor.cpp - Xbox (UWP) monitor: the TV the CoreWindow fills
//
//============================================================

#include "modules/monitor/monitor_common.h"
#include "modules/osdmodule.h"

#include "window.h"

#include "osdcore.h"


namespace osd {

namespace {

class uwp_monitor_info : public osd_monitor_info
{
public:
	uwp_monitor_info(monitor_module &module) :
		osd_monitor_info(module, 1, std::string("Xbox"), 16.0f / 9.0f)
	{
		m_is_primary = true;
		refresh();
	}

	void refresh() override
	{
		osd_dim size = uwp_core_window_size();
		if ((size.width() <= 0) || (size.height() <= 0))
			size = osd_dim(1920, 1080);

		m_pos_size = osd_rect(0, 0, size);
		m_usuable_pos_size = osd_rect(0, 0, size);
		set_aspect(float(size.width()) / float(size.height()));
	}
};


class uwp_monitor_module : public monitor_module_base
{
public:
	uwp_monitor_module() : monitor_module_base(OSD_MONITOR_PROVIDER, "uwp")
	{
	}

	std::shared_ptr<osd_monitor_info> monitor_from_rect(const osd_rect &rect) override
	{
		return first();
	}

	std::shared_ptr<osd_monitor_info> monitor_from_window(const osd_window &window) override
	{
		return first();
	}

protected:
	int init_internal(const osd_options &options) override
	{
		auto monitor = std::make_shared<uwp_monitor_info>(*this);
		add_monitor(monitor);
		m_monitor = monitor;
		return 0;
	}

private:
	std::shared_ptr<osd_monitor_info> first()
	{
		if (!m_initialized)
			return nullptr;
		return m_monitor;
	}

	std::shared_ptr<osd_monitor_info> m_monitor;
};

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(MONITOR_UWP, osd::uwp_monitor_module)
