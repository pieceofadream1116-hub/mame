// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
/***************************************************************************

    ui/gridhome.h

    Box-art grid home screen (Xbox build): your available games as
    tiles with their flyer/snapshot art, a clock, the controller
    count and side buttons that show what each controller button
    does. The classic system list is underneath it (Y or B).

***************************************************************************/
#ifndef MAME_FRONTEND_UI_GRIDHOME_H
#define MAME_FRONTEND_UI_GRIDHOME_H

#pragma once

#include "ui/menu.h"
#include "ui/utils.h"

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace ui {

class menu_grid_home : public menu
{
public:
	menu_grid_home(mame_ui_manager &mui, render_target &target);
	virtual ~menu_grid_home() override;

private:
	struct art
	{
		std::unique_ptr<bitmap_argb32> bitmap;
		render_texture *texture = nullptr;
		bool tried = false;
	};

	virtual void populate() override;
	virtual bool handle(event const *ev) override;
	virtual void draw(uint32_t flags) override;

	// drawing helpers (all positions in pixels of the UI target)
	render_texture *make_texture(bitmap_argb32 &bitmap);
	void make_shapes();
	void quad(float x0, float y0, float x1, float y1, rgb_t color, render_texture *tex = nullptr);
	void pill(float x0, float y0, float x1, float y1, rgb_t color);
	void disc(float cx, float cy, float r, rgb_t color);
	void line(float x0, float y0, float x1, float y1, float width, rgb_t color);
	void text(std::string_view str, float cx, float cy, float size, rgb_t color, float maxwidth = 0.0F);
	void side_button(float cx, float cy, int icon, std::string_view hint);
	art &art_for(ui_system_info const &system, bool allow_load);

	void move(int delta);
	void launch(ui_system_info const &system);

	std::vector<ui_system_info const *> m_games;
	int m_selected = 0;
	int m_columns = 5;
	int m_first_row = 0;
	int m_loads_this_frame = 0;
	std::string m_message;

	float m_px = 1.0F;      // UI target width in pixels
	float m_py = 1.0F;      // UI target height in pixels
	float m_s = 1.0F;       // scale relative to 1080p

	std::vector<std::unique_ptr<bitmap_argb32> > m_shape_bitmaps;
	std::vector<render_texture *> m_textures;
	render_texture *m_gradient = nullptr;
	render_texture *m_disc = nullptr;
	render_texture *m_cap_left = nullptr;
	render_texture *m_cap_right = nullptr;
	render_texture *m_tile = nullptr;
	render_texture *m_tile_ring = nullptr;
	render_texture *m_ring = nullptr;

	std::map<game_driver const *, art> m_art;

	static int s_last_selected;
};

} // namespace ui

#endif // MAME_FRONTEND_UI_GRIDHOME_H
