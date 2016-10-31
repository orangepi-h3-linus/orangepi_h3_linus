#ifndef __DE2_CRTC_H__
#define __DE2_CRTC_H__
/*
 * Copyright (C) 2016 Jean-François Moine
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <drm/drm_plane_helper.h>

struct priv;

enum de2_plane2 {
	DE2_PRIMARY_PLANE,
	DE2_CURSOR_PLANE,
	DE2_VI_PLANE,
	DE2_N_PLANES,
};
struct lcd {
	void __iomem *mmio;

	struct device *dev;
	struct drm_crtc crtc;
	struct priv *priv;	/* DRM/DE private data */

	short num;		/* LCD number in hardware */
	short crtc_idx;		/* CRTC index in drm */

	struct clk *clk;
	struct clk *gate;
	struct reset_control *rstc;

	char name[16];

	struct drm_pending_vblank_event *event;

	struct drm_plane planes[DE2_N_PLANES];
};

#define crtc_to_lcd(x) container_of(x, struct lcd, crtc)

/* in de2_de.c */
void de2_de_enable(struct priv *priv, int lcd_num);
void de2_de_disable(struct priv *priv, int lcd_num);
void de2_de_hw_init(struct priv *priv, int lcd_num);
void de2_de_panel_init(struct priv *priv, int lcd_num,
			struct drm_display_mode *mode);
void de2_de_plane_disable(struct priv *priv,
			int lcd_num, int plane_ix);
void de2_de_plane_update(struct priv *priv,
			int lcd_num, int plane_ix,
			struct drm_plane_state *state,
			struct drm_plane_state *old_state);

/* in de2_plane.c */
int de2_plane_init(struct drm_device *drm, struct lcd *lcd);

#endif /* __DE2_CRTC_H__ */
