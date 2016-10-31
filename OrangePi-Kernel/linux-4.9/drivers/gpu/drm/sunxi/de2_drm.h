#ifndef __DE2_DRM_H__
#define __DE2_DRM_H__
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
#include <drm/drmP.h>
#include <drm/drm_fb_cma_helper.h>

struct lcd;

#define N_LCDS 2

/* SoC types */
#define SOC_A83T 0
#define SOC_H3 1

struct priv {
	void __iomem *mmio;
	struct clk *clk;
	struct clk *gate;
	struct reset_control *rstc;

	int soc_type;

	struct drm_fbdev_cma *fbdev;

	struct lcd *lcds[N_LCDS];
};

/* in de2_crtc.c */
int de2_enable_vblank(struct drm_device *drm, unsigned crtc);
void de2_disable_vblank(struct drm_device *drm, unsigned crtc);
extern struct platform_driver de2_lcd_platform_driver;

/* in de2_de.c */
int de2_de_init(struct priv *priv, struct device *dev);
void de2_de_cleanup(struct priv *priv);

#endif /* __DE2_DRM_H__ */
