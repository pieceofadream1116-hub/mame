// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
/***************************************************************************

    ui/gridhome.cpp

    Box-art grid home screen (Xbox build).

    Controls:
      D-pad / left stick   move
      A                    play
      X                    history / info for the game
      Y or B               the classic MAME system list (filters,
                           settings, every machine)
      LB / RB              a page up / down

    Box art comes from the flyers folder, then snapshots, then title
    screens (see the ui.ini paths, e.g. E:\MAME\flyers\galaga.png).

***************************************************************************/

#include "emu.h"
#include "ui/gridhome.h"

#include "ui/datmenu.h"
#include "ui/moptions.h"
#include "ui/systemlist.h"
#include "ui/ui.h"

#include "audit.h"
#include "mame.h"

#include "drivenum.h"
#include "emuopts.h"
#include "fileio.h"
#include "input.h"
#include "inputdev.h"
#include "rendutil.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>


namespace ui {

namespace {

enum
{
	ICON_LIST = 0,
	ICON_INFO,
	ICON_GEAR,
	ICON_PAGE,
	ICON_BACK,
	ICON_POWER
};

constexpr float TAU = 6.2831853F;

rgb_t ink(int alpha = 255) { return rgb_t(alpha, 52, 52, 110); }
rgb_t bubble(int alpha = 200) { return rgb_t(alpha, 255, 255, 255); }
rgb_t accent(int alpha = 255) { return rgb_t(alpha, 236, 72, 104); }
rgb_t shadow(int alpha = 50) { return rgb_t(alpha, 60, 50, 160); }

// coverage of a pixel by a rounded rectangle (0..1), for smooth edges
float rounded_coverage(float x, float y, float w, float h, float r)
{
	float const cx = std::clamp(x, r, w - r);
	float const cy = std::clamp(y, r, h - r);
	float const d = std::hypot(x - cx, y - cy);
	return std::clamp(r - d + 0.5F, 0.0F, 1.0F);
}

std::chrono::steady_clock::time_point s_message_time;

} // anonymous namespace


int menu_grid_home::s_last_selected = 0;


//-------------------------------------------------
//  construction
//-------------------------------------------------

menu_grid_home::menu_grid_home(mame_ui_manager &mui, render_target &target)
	: menu(mui, target)
{
	set_process_flags(PROCESS_LR_ALWAYS | PROCESS_LR_REPEAT | PROCESS_CUSTOM_NAV | PROCESS_IGNOREPAUSE);

	// the systems the classic list found ROMs for
	system_list &systems(system_list::instance());
	systems.wait_available(system_list::AVAIL_SORTED_LIST);
	for (ui_system_info &info : systems.sorted_list())
	{
		if (!info.available || !info.driver)
			continue;
		if (info.driver->flags & machine_flags::IS_BIOS_ROOT)
			continue;
		if (!std::strcmp(info.driver->name, "___empty"))
			continue;
		m_games.push_back(&info);
	}

	m_selected = std::clamp(s_last_selected, 0, std::max(0, int(m_games.size()) - 1));
	make_shapes();
}

menu_grid_home::~menu_grid_home()
{
	s_last_selected = m_selected;
	for (auto &entry : m_art)
	{
		if (entry.second.texture)
			machine().render().texture_free(entry.second.texture);
	}
	for (render_texture *tex : m_textures)
		machine().render().texture_free(tex);
}


//-------------------------------------------------
//  shapes: drawn once into bitmaps, then
//  stretched and tinted as needed
//-------------------------------------------------

render_texture *menu_grid_home::make_texture(bitmap_argb32 &bitmap)
{
	render_texture *const tex = machine().render().texture_alloc();
	tex->set_bitmap(bitmap, bitmap.cliprect(), TEXFORMAT_ARGB32);
	m_textures.push_back(tex);
	return tex;
}

void menu_grid_home::make_shapes()
{
	auto shape = [this] (int w, int h, auto &&alpha) -> render_texture *
	{
		auto bmp = std::make_unique<bitmap_argb32>(w, h);
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
				bmp->pix(y, x) = rgb_t(u8(std::clamp(alpha(x + 0.5F, y + 0.5F), 0.0F, 1.0F) * 255.0F + 0.5F), 255, 255, 255);
		render_texture *const tex = make_texture(*bmp);
		m_shape_bitmaps.emplace_back(std::move(bmp));
		return tex;
	};

	// soft lavender-to-periwinkle background
	{
		auto bmp = std::make_unique<bitmap_argb32>(64, 64);
		for (int y = 0; y < 64; ++y)
		{
			for (int x = 0; x < 64; ++x)
			{
				float const u = x / 63.0F, v = y / 63.0F;
				auto mix = [u, v] (float tl, float tr, float bl, float br)
				{
					return (tl * (1 - u) + tr * u) * (1 - v) + (bl * (1 - u) + br * u) * v;
				};
				bmp->pix(y, x) = rgb_t(255,
						u8(mix(205, 170, 150, 118)),
						u8(mix(196, 178, 140, 128)),
						u8(mix(255, 255, 250, 246)));
			}
		}
		m_gradient = make_texture(*bmp);
		m_shape_bitmaps.emplace_back(std::move(bmp));
	}

	m_disc = shape(128, 128, [] (float x, float y) { return 64.0F - std::hypot(x - 64.0F, y - 64.0F) + 0.5F; });
	m_ring = shape(128, 128, [] (float x, float y)
	{
		float const d = std::hypot(x - 64.0F, y - 64.0F);
		return std::min(64.0F - d + 0.5F, d - 56.0F + 0.5F);
	});
	m_cap_left = shape(64, 128, [] (float x, float y) { return 64.0F - std::hypot(x - 64.0F, y - 64.0F) + 0.5F; });
	m_cap_right = shape(64, 128, [] (float x, float y) { return 64.0F - std::hypot(x, y - 64.0F) + 0.5F; });

	// game tile (200 x 250 at 1080p, corner radius 22) and its selection ring
	m_tile = shape(200, 250, [] (float x, float y) { return rounded_coverage(x, y, 200.0F, 250.0F, 22.0F); });
	m_tile_ring = shape(200, 250, [] (float x, float y)
	{
		float const outer = rounded_coverage(x, y, 200.0F, 250.0F, 22.0F);
		float const inner = rounded_coverage(x - 5.0F, y - 5.0F, 190.0F, 240.0F, 17.0F);
		return outer * (1.0F - inner);
	});
}


//-------------------------------------------------
//  drawing helpers (pixel coordinates)
//-------------------------------------------------

void menu_grid_home::quad(float x0, float y0, float x1, float y1, rgb_t color, render_texture *tex)
{
	container().add_quad(x0 / m_px, y0 / m_py, x1 / m_px, y1 / m_py, color, tex, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
}

void menu_grid_home::pill(float x0, float y0, float x1, float y1, rgb_t color)
{
	float const r = (y1 - y0) * 0.5F;
	quad(x0, y0, x0 + r, y1, color, m_cap_left);
	quad(x0 + r, y0, x1 - r, y1, color);
	quad(x1 - r, y0, x1, y1, color, m_cap_right);
}

void menu_grid_home::disc(float cx, float cy, float r, rgb_t color)
{
	quad(cx - r, cy - r, cx + r, cy + r, color, m_disc);
}

void menu_grid_home::line(float x0, float y0, float x1, float y1, float width, rgb_t color)
{
	container().add_line(x0 / m_px, y0 / m_py, x1 / m_px, y1 / m_py, width / m_py, color, PRIMFLAG_BLENDMODE(BLENDMODE_ALPHA));
}

void menu_grid_home::text(std::string_view str, float cx, float cy, float size, rgb_t color, float maxwidth)
{
	if (maxwidth <= 0.0F)
		maxwidth = 2.0F * std::min(cx, m_px - cx);
	ui().draw_text_full(
			container(), str,
			(cx - maxwidth * 0.5F) / m_px, (cy - size * 0.5F) / m_py, maxwidth / m_px,
			text_layout::text_justify::CENTER, text_layout::word_wrapping::TRUNCATE,
			mame_ui_manager::NORMAL, color, rgb_t::transparent(),
			nullptr, nullptr, size / m_py);
}

void menu_grid_home::side_button(float cx, float cy, int icon, std::string_view hint)
{
	float const r = 46.0F * m_s;
	float const t = 4.0F * m_s;
	disc(cx, cy + 3.0F * m_s, r, shadow(40));
	disc(cx, cy, r, bubble(215));

	float const k = r * 0.62F;
	switch (icon)
	{
	case ICON_LIST:
		for (int i = -1; i <= 1; ++i)
		{
			float const y = cy + i * k * 0.4F;
			disc(cx - k * 0.55F, y, t * 0.9F, ink());
			line(cx - k * 0.3F, y, cx + k * 0.65F, y, t, ink());
		}
		break;
	case ICON_INFO:
		quad(cx - k * 0.7F, cy - k * 0.7F, cx + k * 0.7F, cy + k * 0.7F, ink(), m_ring);
		disc(cx, cy - k * 0.32F, t * 0.9F, ink());
		line(cx, cy - k * 0.1F, cx, cy + k * 0.38F, t * 1.1F, ink());
		break;
	case ICON_GEAR:
		for (int i = 0; i < 8; ++i)
		{
			float const a = i * TAU / 8.0F;
			line(cx + std::cos(a) * k * 0.45F, cy + std::sin(a) * k * 0.45F,
					cx + std::cos(a) * k * 0.78F, cy + std::sin(a) * k * 0.78F, t * 1.6F, ink());
		}
		disc(cx, cy, k * 0.5F, ink());
		disc(cx, cy, k * 0.22F, bubble(255));
		break;
	case ICON_PAGE:
		line(cx - k * 0.6F, cy - k * 0.3F, cx, cy - k * 0.7F, t, ink());
		line(cx, cy - k * 0.7F, cx + k * 0.6F, cy - k * 0.3F, t, ink());
		line(cx - k * 0.6F, cy + k * 0.3F, cx, cy + k * 0.7F, t, ink());
		line(cx, cy + k * 0.7F, cx + k * 0.6F, cy + k * 0.3F, t, ink());
		break;
	case ICON_BACK:
		line(cx - k * 0.65F, cy, cx + k * 0.6F, cy, t, ink());
		line(cx - k * 0.65F, cy, cx - k * 0.2F, cy - k * 0.45F, t, ink());
		line(cx - k * 0.65F, cy, cx - k * 0.2F, cy + k * 0.45F, t, ink());
		break;
	case ICON_POWER:
		quad(cx - k * 0.65F, cy - k * 0.6F, cx + k * 0.65F, cy + k * 0.7F, ink(), m_ring);
		quad(cx - k * 0.22F, cy - k * 0.85F, cx + k * 0.22F, cy - k * 0.1F, bubble(255));
		line(cx, cy - k * 0.8F, cx, cy - k * 0.05F, t * 1.2F, ink());
		break;
	}

	text(hint, cx, cy + r + 20.0F * m_s, 24.0F * m_s, ink(220), 220.0F * m_s);
}


//-------------------------------------------------
//  box art: flyer, then snapshot, then title
//-------------------------------------------------

menu_grid_home::art &menu_grid_home::art_for(ui_system_info const &system, bool allow_load)
{
	art &result = m_art[system.driver];
	if (result.tried || !allow_load)
		return result;
	result.tried = true;
	++m_loads_this_frame;

	char const *const paths[] = {
			ui().options().flyers_directory(),
			machine().options().snapshot_directory(),
			ui().options().titles_directory(),
			ui().options().cabinets_directory() };

	std::vector<std::string> names{ system.driver->name };
	int const parent = driver_list::non_bios_clone(*system.driver);
	if (parent >= 0)
		names.emplace_back(driver_list::driver(parent).name);

	auto bitmap = std::make_unique<bitmap_argb32>();
	for (char const *path : paths)
	{
		emu_file file(path, OPEN_FLAG_READ);
		for (std::string const &name : names)
		{
			if (!file.open(name + ".png"))
			{
				render_load_png(*bitmap, file);
				file.close();
			}
			if (!bitmap->valid() && !file.open(name + ".jpg"))
			{
				render_load_jpeg(*bitmap, file);
				file.close();
			}
			if (bitmap->valid())
				break;
		}
		if (bitmap->valid())
			break;
	}

	if (bitmap->valid())
	{
		result.texture = machine().render().texture_alloc();
		result.texture->set_bitmap(*bitmap, bitmap->cliprect(), TEXFORMAT_ARGB32);
		result.bitmap = std::move(bitmap);
	}
	return result;
}


//-------------------------------------------------
//  populate - one placeholder item; the grid
//  draws and moves on its own
//-------------------------------------------------

void menu_grid_home::populate()
{
	item_append(std::string("grid"), 0, nullptr);
}


//-------------------------------------------------
//  input
//-------------------------------------------------

void menu_grid_home::move(int delta)
{
	if (m_games.empty())
		return;
	int const count = int(m_games.size());
	int index = m_selected + delta;
	if (index < 0)
		index = (m_selected > 0) ? 0 : m_selected;
	if (index >= count)
		index = (m_selected < count - 1) ? count - 1 : m_selected;
	m_selected = index;
	m_message.clear();
}

void menu_grid_home::launch(ui_system_info const &system)
{
	game_driver const &driver(*system.driver);

	// check the ROMs first, as the classic list does
	driver_enumerator enumerator(machine().options(), driver);
	enumerator.next();
	media_auditor auditor(enumerator);
	media_auditor::summary const summary = auditor.audit_media(AUDIT_VALIDATE_FAST);
	if ((summary != media_auditor::CORRECT) && (summary != media_auditor::BEST_AVAILABLE) && (summary != media_auditor::NONE_NEEDED))
	{
		std::string missing;
		int shown = 0;
		for (media_auditor::audit_record const &record : auditor.records())
		{
			if (record.status() == media_auditor::audit_status::GOOD)
				continue;
			if (shown == 4)
			{
				missing += ", ...";
				break;
			}
			missing += (shown++ ? ", " : "") + record.name();
		}
		m_message = util::string_format("%s can't start: missing or wrong ROM files (%s)", system.description, missing);
		s_message_time = std::chrono::steady_clock::now();
		return;
	}

	s_last_selected = m_selected;
	machine().options().set_system_name(driver.name);
	mame_machine_manager::instance()->schedule_new_driver(driver);
	machine().schedule_hard_reset();
	stack_reset(ui());
}

bool menu_grid_home::handle(event const *ev)
{
	if (!ev)
		return false;

	int const page = m_columns * 2;
	switch (ev->iptkey)
	{
	case IPT_UI_LEFT:       move(-1); return true;
	case IPT_UI_RIGHT:      move(1); return true;
	case IPT_UI_UP:         move(-m_columns); return true;
	case IPT_UI_DOWN:       move(m_columns); return true;
	case IPT_UI_PAGE_UP:
	case IPT_UI_FOCUS_PREV: move(-page); return true;
	case IPT_UI_PAGE_DOWN:
	case IPT_UI_FOCUS_NEXT: move(page); return true;
	case IPT_UI_HOME:       move(-int(m_games.size())); return true;
	case IPT_UI_END:        move(int(m_games.size())); return true;

	case IPT_UI_SELECT:
		if (!m_games.empty())
			launch(*m_games[m_selected]);
		return true;

	case IPT_UI_CLEAR:      // X: history and info
		if (!m_games.empty())
			menu::stack_push<menu_dats_view>(ui(), target(), m_games[m_selected]);
		return true;

	case IPT_UI_HELP:       // Y: the classic system list underneath
		s_last_selected = m_selected;
		stack_pop();
		return true;

	default:
		return false;
	}
}


//-------------------------------------------------
//  draw
//-------------------------------------------------

void menu_grid_home::draw(uint32_t flags)
{
	m_px = float(std::max<int>(1, int(target().width())));
	m_py = float(std::max<int>(1, int(target().height())));
	m_s = m_py / 1080.0F;
	m_loads_this_frame = 0;
	float const s = m_s;
	float const margin = 40.0F * s;
	float const seconds = float(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());

	// background and a few slowly drifting shapes
	quad(0.0F, 0.0F, m_px, m_py, rgb_t::white(), m_gradient);
	struct blob { float x, y, size; };
	static blob const blobs[] = {
			{ 0.08F, 0.78F, 110 }, { 0.22F, 0.30F, 50 }, { 0.38F, 0.88F, 90 }, { 0.55F, 0.16F, 70 },
			{ 0.70F, 0.70F, 60 },  { 0.86F, 0.40F, 100 }, { 0.93F, 0.85F, 70 }, { 0.30F, 0.60F, 45 } };
	for (blob const &b : blobs)
	{
		float const bob = std::sin(seconds * 0.3F + b.x * 10.0F) * 10.0F * s;
		float const r = b.size * 0.5F * s;
		float const cx = b.x * m_px, cy = b.y * m_py + bob;
		quad(cx - r, cy - r, cx + r, cy + r, rgb_t(45, 255, 255, 255), m_ring);
	}

	// clock (top left)
	{
		std::time_t const now = std::time(nullptr);
		std::tm local{};
#if defined(_WIN32)
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		char time_text[16], date_text[32];
		std::strftime(time_text, sizeof(time_text), "%H:%M", &local);
		std::strftime(date_text, sizeof(date_text), "%d/%m/%Y", &local);

		float const x0 = margin, y0 = margin * 0.6F, x1 = margin + 210.0F * s, y1 = y0 + 92.0F * s;
		pill(x0, y0 + 3.0F * s, x1, y1 + 3.0F * s, shadow(35));
		pill(x0, y0, x1, y1, bubble(185));
		text(time_text, (x0 + x1) * 0.5F, y0 + 36.0F * s, 40.0F * s, ink());
		text(date_text, (x0 + x1) * 0.5F, y0 + 70.0F * s, 22.0F * s, ink(170));
	}

	// title (top middle)
	{
		float const cx = m_px * 0.5F, cy = margin * 0.6F + 46.0F * s;
		disc(cx, cy + 3.0F * s, 44.0F * s, shadow(40));
		disc(cx, cy, 44.0F * s, bubble(235));
		quad(cx - 33.0F * s, cy - 33.0F * s, cx + 33.0F * s, cy + 33.0F * s, accent(), m_ring);
		text("M", cx, cy, 38.0F * s, accent());
		text(util::string_format("%d games", int(m_games.size())), cx, cy + 66.0F * s, 24.0F * s, ink(210), 400.0F * s);
	}

	// controllers (top right)
	{
		int pads = 0;
		input_class &joysticks = machine().input().device_class(DEVICE_CLASS_JOYSTICK);
		for (int i = 0; i <= joysticks.maxindex(); ++i)
			if (joysticks.device(i))
				++pads;

		float const x0 = m_px - margin - 180.0F * s, y0 = margin * 0.6F, x1 = m_px - margin, y1 = y0 + 92.0F * s;
		pill(x0, y0 + 3.0F * s, x1, y1 + 3.0F * s, shadow(35));
		pill(x0, y0, x1, y1, bubble(185));
		float const cx = (x0 + x1) * 0.5F, cy = y0 + 36.0F * s;
		pill(cx - 32.0F * s, cy - 15.0F * s, cx + 32.0F * s, cy + 15.0F * s, ink());
		disc(cx - 15.0F * s, cy, 5.0F * s, bubble(255));
		disc(cx + 15.0F * s, cy, 5.0F * s, bubble(255));
		text(util::string_format("%d pad%s", pads, (pads == 1) ? "" : "s"), cx, y0 + 72.0F * s, 22.0F * s, ink(170));
	}

	// side buttons
	{
		float const left = margin + 50.0F * s, right = m_px - margin - 50.0F * s;
		float const top = m_py * 0.30F, step = 150.0F * s;
		side_button(left, top, ICON_LIST, "Y  all systems");
		side_button(left, top + step, ICON_INFO, "X  history");
		side_button(left, top + step * 2.0F, ICON_GEAR, "View+X  in game");
		side_button(right, top, ICON_PAGE, "LB / RB  page");
		side_button(right, top + step, ICON_BACK, "B  back");
		side_button(right, top + step * 2.0F, ICON_POWER, "View+Menu  quit game");
	}

	// the grid
	float const side = 175.0F * s, top = 210.0F * s, bottom = 140.0F * s;
	float const tile_w = 200.0F * s, tile_h = 250.0F * s, gap = 28.0F * s;
	float const avail_w = m_px - side * 2.0F, avail_h = m_py - top - bottom;
	m_columns = std::max(1, int((avail_w + gap) / (tile_w + gap)));
	int const visible_rows = std::max(1, int((avail_h + gap) / (tile_h + gap)));
	int const count = int(m_games.size());
	int const rows = (count + m_columns - 1) / m_columns;

	if (m_games.empty())
	{
		float const cx = m_px * 0.5F, cy = m_py * 0.5F;
		pill(cx - 470.0F * s, cy - 80.0F * s, cx + 470.0F * s, cy + 80.0F * s, bubble(200));
		text("No games found", cx, cy - 22.0F * s, 40.0F * s, ink());
		text("Put your ROM zips in X:\\MAME\\roms or X:\\roms, then restart (Y shows all systems)", cx, cy + 26.0F * s, 24.0F * s, ink(180), 880.0F * s);
	}
	else
	{
		m_selected = std::clamp(m_selected, 0, count - 1);
		int const sel_row = m_selected / m_columns;
		if (sel_row < m_first_row)
			m_first_row = sel_row;
		if (sel_row >= m_first_row + visible_rows)
			m_first_row = sel_row - visible_rows + 1;
		m_first_row = std::clamp(m_first_row, 0, std::max(0, rows - visible_rows));

		float const grid_w = m_columns * tile_w + (m_columns - 1) * gap;
		float const start_x = (m_px - grid_w) * 0.5F;

		for (int row = m_first_row; row < std::min(rows, m_first_row + visible_rows); ++row)
		{
			for (int col = 0; col < m_columns; ++col)
			{
				int const index = row * m_columns + col;
				if (index >= count)
					break;

				ui_system_info const &system = *m_games[index];
				bool const selected = index == m_selected;
				float const grow = selected ? 1.06F : 1.0F;
				float const cx = start_x + col * (tile_w + gap) + tile_w * 0.5F;
				float const cy = top + (row - m_first_row) * (tile_h + gap) + tile_h * 0.5F;
				float const hw = tile_w * 0.5F * grow, hh = tile_h * 0.5F * grow;

				quad(cx - hw, cy - hh + 5.0F * s, cx + hw, cy + hh + 5.0F * s, shadow(selected ? 70 : 35), m_tile);
				quad(cx - hw, cy - hh, cx + hw, cy + hh, bubble(selected ? 240 : 190), m_tile);

				// art, keeping its shape inside the tile
				float const pad = 16.0F * s;
				float const box_w = hw * 2.0F - pad * 2.0F, box_h = hh * 2.0F - pad * 2.0F;
				art &a = art_for(system, m_loads_this_frame < 2); // a couple of new images per frame

				if (a.texture && a.bitmap)
				{
					float const iw = float(a.bitmap->width()), ih = float(a.bitmap->height());
					float const fit = std::min(box_w / iw, box_h / ih);
					float const w = iw * fit, h = ih * fit;
					quad(cx - w * 0.5F, cy - h * 0.5F, cx + w * 0.5F, cy + h * 0.5F, rgb_t::white(), a.texture);
				}
				else
				{
					quad(cx - box_w * 0.5F, cy - box_h * 0.5F, cx + box_w * 0.5F, cy + box_h * 0.5F, ink(200));
					ui().draw_text_full(
							container(), system.description,
							(cx - box_w * 0.5F + 8.0F * s) / m_px, (cy - box_h * 0.3F) / m_py, (box_w - 16.0F * s) / m_px,
							text_layout::text_justify::CENTER, text_layout::word_wrapping::WORD,
							mame_ui_manager::NORMAL, rgb_t::white(), rgb_t::transparent(),
							nullptr, nullptr, (22.0F * s) / m_py);
				}

				if (selected)
					quad(cx - hw, cy - hh, cx + hw, cy + hh, accent(), m_tile_ring);
			}
		}

		if (m_first_row > 0)
			text("^", m_px * 0.5F, top - 34.0F * s, 34.0F * s, ink(160));
		if (m_first_row + visible_rows < rows)
			text("v", m_px * 0.5F, m_py - bottom + 8.0F * s, 34.0F * s, ink(160));

		// selected game's name and year (bottom middle)
		ui_system_info const &system = *m_games[m_selected];
		std::string const name = util::string_format("%s  (%s, %s)", system.description, system.driver->year, system.driver->manufacturer);
		float const width = std::min(m_px - 2.0F * side, ui().get_string_width(target(), name, (30.0F * s) / m_py) * m_px + 80.0F * s);
		float const cx = m_px * 0.5F, cy = m_py - margin - 36.0F * s;
		pill(cx - width * 0.5F, cy - 30.0F * s, cx + width * 0.5F, cy + 30.0F * s, bubble(225));
		text(name, cx, cy, 30.0F * s, ink(), width - 50.0F * s);
	}

	// error message (e.g. missing ROMs), for a few seconds
	if (!m_message.empty())
	{
		if (std::chrono::steady_clock::now() - s_message_time > std::chrono::seconds(6))
		{
			m_message.clear();
		}
		else
		{
			float const cx = m_px * 0.5F, cy = m_py * 0.5F;
			float const w = std::min(m_px - 120.0F * s, 1400.0F * s);
			pill(cx - w * 0.5F, cy - 44.0F * s, cx + w * 0.5F, cy + 44.0F * s, accent(235));
			text(m_message, cx, cy, 28.0F * s, rgb_t::white(), w - 80.0F * s);
		}
	}
}

} // namespace ui
