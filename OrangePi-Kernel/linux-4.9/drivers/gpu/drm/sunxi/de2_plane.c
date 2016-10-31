/*
 * Allwinner DRM driver - DE2 planes
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_crtc_helper.h>

#include "de2_drm.h"
#include "de2_crtc.h"

/* plane formats */
static const uint32_t ui_formats[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
};

static const uint32_t vi_formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_YUV422,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
};

static void de2_plane_disable(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_crtc *crtc = old_state->crtc;
	struct lcd *lcd = crtc_to_lcd(crtc);
	int plane_num = plane - lcd->planes;

	de2_de_plane_disable(lcd->priv, lcd->num, plane_num);
}

static void de2_plane_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct drm_plane_state *state = plane->state;
	struct drm_crtc *crtc = state->crtc;
	struct lcd *lcd = crtc_to_lcd(crtc);
	struct drm_framebuffer *fb = state->fb;
	int plane_num = plane - lcd->planes;

	if (!crtc || !fb) {
		DRM_DEBUG_DRIVER("no crtc/fb\n");
		return;
	}

	de2_de_plane_update(lcd->priv, lcd->num, plane_num,
			    state, old_state);
}

static const struct drm_plane_helper_funcs plane_helper_funcs = {
	.atomic_disable = de2_plane_disable,
	.atomic_update = de2_plane_update,
};

static const struct drm_plane_funcs plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static int de2_one_plane_init(struct drm_device *drm,
				struct drm_plane *plane,
				int type, int possible_crtcs,
				const uint32_t *formats,
				int nformats)
{
	int ret;

	ret = drm_universal_plane_init(drm, plane, possible_crtcs,
				&plane_funcs,
				formats, nformats, type, NULL);
	if (ret >= 0)
		drm_plane_helper_add(plane, &plane_helper_funcs);

	return ret;
}

int de2_plane_init(struct drm_device *drm, struct lcd *lcd)
{
	int ret, possible_crtcs = 1 << lcd->crtc_idx;

	ret = de2_one_plane_init(drm, &lcd->planes[DE2_PRIMARY_PLANE],
				DRM_PLANE_TYPE_PRIMARY, possible_crtcs,
				ui_formats, ARRAY_SIZE(ui_formats));
	if (ret >= 0)
		ret = de2_one_plane_init(drm, &lcd->planes[DE2_CURSOR_PLANE],
				DRM_PLANE_TYPE_CURSOR, possible_crtcs,
				ui_formats, ARRAY_SIZE(ui_formats));
	if (ret >= 0)
		ret = de2_one_plane_init(drm, &lcd->planes[DE2_VI_PLANE],
				DRM_PLANE_TYPE_OVERLAY, possible_crtcs,
				vi_formats, ARRAY_SIZE(vi_formats));
	if (ret < 0)
		dev_err(lcd->dev, "Couldn't initialize the planes err %d\n",
				ret);

	return ret;
}
