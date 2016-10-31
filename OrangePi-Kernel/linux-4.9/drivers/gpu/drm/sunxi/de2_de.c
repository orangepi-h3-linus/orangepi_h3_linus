/*
 * ALLWINNER DRM driver - Display Engine 2
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Copyright (c) 2016 Allwinnertech Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <asm/io.h>
#include <drm/drm_gem_cma_helper.h>

#include "de2_drm.h"
#include "de2_crtc.h"

static DEFINE_SPINLOCK(de_lock);

#define DE_CLK_RATE_A83T 504000000	/* pll-de */
#define DE_CLK_RATE_H3 432000000	/* de */

/* I/O map */

#define DE_MOD_REG 0x0000	/* 1 bit per LCD */
#define DE_GATE_REG 0x0004
#define DE_RESET_REG 0x0008
#define DE_DIV_REG 0x000c	/* 4 bits per LCD */
#define DE_SEL_REG 0x0010

#define DE_MUX0_BASE 0x00100000
#define DE_MUX1_BASE 0x00200000

/* MUX registers (addr / MUX base) */
#define DE_MUX_GLB_REGS 0x00000		/* global control */
#define DE_MUX_BLD_REGS 0x01000		/* alpha blending */
#define DE_MUX_CHAN_REGS 0x02000	/* VI/UI overlay channels */
#define		DE_MUX_CHAN_SZ 0x1000	/* size of a channel */
#define DE_MUX_VSU_REGS 0x20000		/* VSU */
#define DE_MUX_GSU1_REGS 0x30000	/* GSUs */
#define DE_MUX_GSU2_REGS 0x40000
#define DE_MUX_GSU3_REGS 0x50000
#define DE_MUX_FCE_REGS 0xa0000		/* FCE */
#define DE_MUX_BWS_REGS 0xa2000		/* BWS */
#define DE_MUX_LTI_REGS 0xa4000		/* LTI */
#define DE_MUX_PEAK_REGS 0xa6000	/* PEAK */
#define DE_MUX_ASE_REGS 0xa8000		/* ASE */
#define DE_MUX_FCC_REGS 0xaa000		/* FCC */
#define DE_MUX_DCSC_REGS 0xb0000	/* DCSC/SMBL */

/* global control */
struct de_glb {
	u32 ctl;
#define		DE_MUX_GLB_CTL_rt_en BIT(0)
#define		DE_MUX_GLB_CTL_finish_irq_en BIT(4)
#define		DE_MUX_GLB_CTL_rtwb_port BIT(12)
	u32 status;
	u32 dbuff;
	u32 size;
};

/* alpha blending */
struct de_bld {
	u32 fcolor_ctl;			/* 00 */
	struct {
		u32 fcolor;
		u32 insize;
		u32 offset;
		u32 dum;
	} attr[4];
	u32 dum0[15];			/* (end of clear offset) */
	u32 route;			/* 80 */
	u32 premultiply;
	u32 bkcolor;
	u32 output_size;
	u32 bld_mode[4];
	u32 dum1[4];
	u32 ck_ctl;			/* b0 */
	u32 ck_cfg;
	u32 dum2[2];
	u32 ck_max[4];			/* c0 */
	u32 dum3[4];
	u32 ck_min[4];			/* e0 */
	u32 dum4[3];
	u32 out_ctl;			/* fc */
};

/* VI channel */
struct de_vi {
	struct {
		u32 attr;
#define			VI_CFG_ATTR_en BIT(0)
#define			VI_CFG_ATTR_fcolor_en BIT(4)
#define			VI_CFG_ATTR_fmt_SHIFT 8
#define			VI_CFG_ATTR_fmt_MASK GENMASK(12, 8)
#define			VI_CFG_ATTR_ui_sel BIT(15)
#define			VI_CFG_ATTR_top_down BIT(23)
		u32 size;
		u32 coord;
#define VI_N_PLANES 3
		u32 pitch[VI_N_PLANES];
		u32 top_laddr[VI_N_PLANES];
		u32 bot_laddr[VI_N_PLANES];
	} cfg[4];
	u32 fcolor[4];			/* c0 */
	u32 top_haddr[VI_N_PLANES];	/* d0 */
	u32 bot_haddr[VI_N_PLANES];	/* dc */
	u32 ovl_size[2];		/* e8 */
	u32 hori[2];			/* f0 */
	u32 vert[2];			/* f8 */
};

/* UI channel */
struct de_ui {
	struct {
		u32 attr;
#define			UI_CFG_ATTR_en BIT(0)
#define			UI_CFG_ATTR_alpmod_SHIFT 1
#define			UI_CFG_ATTR_alpmod_MASK GENMASK(2, 1)
#define			UI_CFG_ATTR_fcolor_en BIT(4)
#define			UI_CFG_ATTR_fmt_SHIFT 8
#define			UI_CFG_ATTR_fmt_MASK GENMASK(12, 8)
#define			UI_CFG_ATTR_top_down BIT(23)
#define			UI_CFG_ATTR_alpha_SHIFT 24
#define			UI_CFG_ATTR_alpha_MASK GENMASK(31, 24)
		u32 size;
		u32 coord;
		u32 pitch;
		u32 top_laddr;
		u32 bot_laddr;
		u32 fcolor;
		u32 dum;
	} cfg[4];			/* 00 */
	u32 top_haddr;			/* 80 */
	u32 bot_haddr;
	u32 ovl_size;			/* 88 */
};

/* coordinates and sizes */
#define XY(x, y) (((y) << 16) | (x))
#define WH(w, h) (((h - 1) << 16) | (w - 1))

/* UI video formats */
#define DE2_FORMAT_ARGB_8888 0
#define DE2_FORMAT_BGRA_8888 3
#define DE2_FORMAT_XRGB_8888 4
#define DE2_FORMAT_RGB_888 8
#define DE2_FORMAT_BGR_888 9

/* VI video formats */
#define DE2_FORMAT_YUV422_I_YVYU 1	/* Y-V-Y-U */
#define DE2_FORMAT_YUV422_I_UYVY 2	/* U-Y-V-Y */
#define DE2_FORMAT_YUV422_I_YUYV 3	/* Y-U-Y-V */
#define DE2_FORMAT_YUV422_P 6		/* YYYY UU VV planar */
#define DE2_FORMAT_YUV420_P 10		/* YYYY U V planar */

#define glb_read(base, member) \
	readl_relaxed(base + offsetof(struct de_glb, member))
#define glb_write(base, member, data) \
	writel_relaxed(data, base + offsetof(struct de_glb, member))
#define bld_read(base, member) \
	readl_relaxed(base + offsetof(struct de_bld, member))
#define bld_write(base, member, data) \
	writel_relaxed(data, base + offsetof(struct de_bld, member))
#define ui_read(base, member) \
	readl_relaxed(base + offsetof(struct de_ui, member))
#define ui_write(base, member, data) \
	writel_relaxed(data, base + offsetof(struct de_ui, member))
#define vi_read(base, member) \
	readl_relaxed(base + offsetof(struct de_vi, member))
#define vi_write(base, member, data) \
	writel_relaxed(data, base + offsetof(struct de_vi, member))

static const struct {
	char chan;
	char layer;
	char pipe;
} plane2layer[DE2_N_PLANES] = {
	[DE2_PRIMARY_PLANE] =	{0, 0, 0},
	[DE2_CURSOR_PLANE] =	{1, 0, 1},
	[DE2_VI_PLANE] =	{0, 1, 0},
};

static inline void de_write(struct priv *priv, int reg, u32 data)
{
	writel_relaxed(data, priv->mmio + reg);
}

static inline u32 de_read(struct priv *priv, int reg)
{
	return readl_relaxed(priv->mmio + reg);
}

static void de_lcd_select(struct priv *priv,
			int lcd_num,
			void __iomem *mux_o)
{
	u32 data;

	/* select the LCD */
	data = de_read(priv, DE_SEL_REG);
	data &= ~1;
	de_write(priv, DE_SEL_REG, data);

	/* double register switch */
	glb_write(mux_o + DE_MUX_GLB_REGS, dbuff, 1);
}

void de2_de_plane_update(struct priv *priv,
			int lcd_num, int plane_ix,
			struct drm_plane_state *state,
			struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_gem_cma_object *gem;
	void __iomem *mux_o = priv->mmio;
	void __iomem *chan_o;
	u32 size = WH(state->crtc_w, state->crtc_h);
	u32 coord;
	u32 screen_size;
	u32 data, fcolor;
	u32 ui_sel, alpha_glob;
	int chan, layer, x, y;
	unsigned fmt;
	unsigned long flags;

	chan = plane2layer[plane_ix].chan;
	layer = plane2layer[plane_ix].layer;

	mux_o += (lcd_num == 0) ? DE_MUX0_BASE : DE_MUX1_BASE;
	chan_o = mux_o;
	chan_o += DE_MUX_CHAN_REGS + DE_MUX_CHAN_SZ * chan;

	x = state->crtc_x >= 0 ? state->crtc_x : 0;
	y = state->crtc_y >= 0 ? state->crtc_y : 0;
	coord = XY(x, y);

	/* handle the cursor move */
	if (plane_ix == DE2_CURSOR_PLANE
	 && fb == old_state->fb) {
		spin_lock_irqsave(&de_lock, flags);
		de_lcd_select(priv, lcd_num, mux_o);
		if (chan == 0)
			vi_write(chan_o, cfg[layer].coord, coord);
		else
			ui_write(chan_o, cfg[layer].coord, coord);
		spin_unlock_irqrestore(&de_lock, flags);
		return;
	}

	gem = drm_fb_cma_get_gem_obj(fb, 0);

	ui_sel = alpha_glob = 0;
	switch (fb->pixel_format) {
	case DRM_FORMAT_ARGB8888:
		fmt = DE2_FORMAT_ARGB_8888;
		ui_sel = VI_CFG_ATTR_ui_sel;
		break;
	case DRM_FORMAT_BGRA8888:
		fmt = DE2_FORMAT_BGRA_8888;
		ui_sel = VI_CFG_ATTR_ui_sel;
		break;
	case DRM_FORMAT_XRGB8888:
		fmt = DE2_FORMAT_XRGB_8888;
		ui_sel = VI_CFG_ATTR_ui_sel;
		alpha_glob = (1 << UI_CFG_ATTR_alpmod_SHIFT) |
				(0xff << UI_CFG_ATTR_alpha_SHIFT);
		break;
	case DRM_FORMAT_RGB888:
		fmt = DE2_FORMAT_RGB_888;
		ui_sel = VI_CFG_ATTR_ui_sel;
		break;
	case DRM_FORMAT_BGR888:
		fmt = DE2_FORMAT_BGR_888;
		ui_sel = VI_CFG_ATTR_ui_sel;
		break;
	case DRM_FORMAT_YUYV:
		fmt = DE2_FORMAT_YUV422_I_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		fmt = DE2_FORMAT_YUV422_I_YVYU;
		break;
	case DRM_FORMAT_YUV422:
		fmt = DE2_FORMAT_YUV422_P;
		break;
	case DRM_FORMAT_YUV420:
		fmt = DE2_FORMAT_YUV420_P;
		break;
	case DRM_FORMAT_UYVY:
		fmt = DE2_FORMAT_YUV422_I_UYVY;
		break;
	default:
		pr_err("format %.4s not yet treated\n",
			(char *) &fb->pixel_format);
		return;
	}

	spin_lock_irqsave(&de_lock, flags);

	screen_size = plane_ix == DE2_PRIMARY_PLANE ?
			size :
			glb_read(mux_o + DE_MUX_GLB_REGS, size);

	/* prepare the activation of alpha blending (1 bit per plane) */
	fcolor = bld_read(mux_o + DE_MUX_BLD_REGS, fcolor_ctl)
			| (0x100 << plane2layer[plane_ix].pipe);

	de_lcd_select(priv, lcd_num, mux_o);

	if (chan == 0) {	/* VI channel */
		int i;

		data = VI_CFG_ATTR_en | (fmt << VI_CFG_ATTR_fmt_SHIFT) |
					ui_sel;
		vi_write(chan_o, cfg[layer].attr, data);
		vi_write(chan_o, cfg[layer].size, size);
		vi_write(chan_o, cfg[layer].coord, coord);
		for (i = 0; i < VI_N_PLANES; i++) {
			vi_write(chan_o, cfg[layer].pitch[i],
					fb->pitches[i] ? fb->pitches[i] :
							fb->pitches[0]);
			vi_write(chan_o, cfg[layer].top_laddr[i],
				gem->paddr + fb->offsets[i]);
			vi_write(chan_o, fcolor[layer], 0xff000000);
		}
		if (layer == 0)
			vi_write(chan_o, ovl_size[0], screen_size);

	} else {		/* UI channel */
		data = UI_CFG_ATTR_en | (fmt << UI_CFG_ATTR_fmt_SHIFT) |
			alpha_glob;
		ui_write(chan_o, cfg[layer].attr, data);
		ui_write(chan_o, cfg[layer].size, size);
		ui_write(chan_o, cfg[layer].coord, coord);
		ui_write(chan_o, cfg[layer].pitch, fb->pitches[0]);
		ui_write(chan_o, cfg[layer].top_laddr,
				gem->paddr + fb->offsets[0]);
		if (layer == 0)
			ui_write(chan_o, ovl_size, screen_size);
	}
	bld_write(mux_o + DE_MUX_BLD_REGS, fcolor_ctl, fcolor);

	spin_unlock_irqrestore(&de_lock, flags);
}

void de2_de_plane_disable(struct priv *priv,
			int lcd_num, int plane_ix)
{
	void __iomem *mux_o = priv->mmio;
	void __iomem *chan_o;
	u32 fcolor;
	int chan, layer, chan_disable = 0;
	unsigned long flags;

	chan = plane2layer[plane_ix].chan;
	layer = plane2layer[plane_ix].layer;

	mux_o += (lcd_num == 0) ? DE_MUX0_BASE : DE_MUX1_BASE;
	chan_o = mux_o;
	chan_o += DE_MUX_CHAN_REGS + DE_MUX_CHAN_SZ * chan;

	/* (only 2 layers) */
	if (chan == 0) {
		if (vi_read(chan_o, cfg[1 - layer].attr) == 0)
			chan_disable = 1;
	} else {
		if (ui_read(chan_o, cfg[1 - layer].attr) == 0)
			chan_disable = 1;
	}

	spin_lock_irqsave(&de_lock, flags);

	fcolor = bld_read(mux_o + DE_MUX_BLD_REGS, fcolor_ctl);

	de_lcd_select(priv, lcd_num, mux_o);

	if (chan == 0)
		vi_write(chan_o, cfg[layer].attr, 0);
	else
		ui_write(chan_o, cfg[layer].attr, 0);

	if (chan_disable)
		bld_write(mux_o + DE_MUX_BLD_REGS, fcolor_ctl,
			fcolor & ~(0x100 << plane2layer[plane_ix].pipe));

	spin_unlock_irqrestore(&de_lock, flags);
}

void de2_de_panel_init(struct priv *priv, int lcd_num,
			struct drm_display_mode *mode)
{
	void __iomem *mux_o = priv->mmio;
	u32 size = WH(mode->hdisplay, mode->vdisplay);
	unsigned i;
	unsigned long flags;

	mux_o += (lcd_num == 0) ? DE_MUX0_BASE : DE_MUX1_BASE;

	DRM_DEBUG_DRIVER("%dx%d\n", mode->hdisplay, mode->vdisplay);

	spin_lock_irqsave(&de_lock, flags);

	de_lcd_select(priv, lcd_num, mux_o);

	glb_write(mux_o + DE_MUX_GLB_REGS, size, size);

	/* set alpha blending */
	for (i = 0; i < 4; i++) {
		bld_write(mux_o + DE_MUX_BLD_REGS, attr[i].fcolor, 0xff000000);
		bld_write(mux_o + DE_MUX_BLD_REGS, attr[i].insize, size);
	}
	bld_write(mux_o + DE_MUX_BLD_REGS, output_size, size);
	bld_write(mux_o + DE_MUX_BLD_REGS, out_ctl,
			mode->flags & DRM_MODE_FLAG_INTERLACE ? 2 : 0);

	spin_unlock_irqrestore(&de_lock, flags);
}

void de2_de_enable(struct priv *priv, int lcd_num)
{
	void __iomem *mux_o = priv->mmio;
	unsigned chan, i;
	u32 size = WH(1920, 1080);
	u32 data;
	unsigned long flags;

	DRM_DEBUG_DRIVER("lcd %d\n", lcd_num);

	de_write(priv, DE_RESET_REG,
			de_read(priv, DE_RESET_REG) |
				(lcd_num == 0 ? 1 : 4));
	data = 1 << lcd_num;			/* 1 bit / lcd */
	de_write(priv, DE_GATE_REG,
			de_read(priv, DE_GATE_REG) | data);
	de_write(priv, DE_MOD_REG,
			de_read(priv, DE_MOD_REG) | data);

	mux_o += (lcd_num == 0) ? DE_MUX0_BASE : DE_MUX1_BASE;

	spin_lock_irqsave(&de_lock, flags);

	/* select the LCD */
	data = de_read(priv, DE_SEL_REG);
	if (lcd_num == 0)
		data &= ~1;
	else
		data |= 1;
	de_write(priv, DE_SEL_REG, data);

	/* start init */
	glb_write(mux_o + DE_MUX_GLB_REGS, ctl,
		DE_MUX_GLB_CTL_rt_en | DE_MUX_GLB_CTL_rtwb_port);
	glb_write(mux_o + DE_MUX_GLB_REGS, status, 0);
	glb_write(mux_o + DE_MUX_GLB_REGS, dbuff, 1);	/* dble reg switch */
	glb_write(mux_o + DE_MUX_GLB_REGS, size, size);

	/* clear the VI/UI channels */
	for (chan = 0; chan < 4; chan++) {
		void __iomem *chan_o = mux_o + DE_MUX_CHAN_REGS +
				DE_MUX_CHAN_SZ * chan;

		memset_io(chan_o, 0, chan == 0 ?
				sizeof(struct de_vi) : sizeof(struct de_ui));

		/* only 1 VI and 1 UI in lcd1 */
		if (chan == 2 && lcd_num == 1)
			break;
	}

	/* clear and set alpha blending */
	memset_io(mux_o + DE_MUX_BLD_REGS, 0, offsetof(struct de_bld, dum0));
	bld_write(mux_o + DE_MUX_BLD_REGS, fcolor_ctl, 0x00000101);
						/* fcolor for primary */

	/* prepare route for planes */
	data = 0;
	for (i = 0; i < DE2_N_PLANES; i++)
		data |= plane2layer[i].chan << (plane2layer[i].pipe * 4);
	bld_write(mux_o + DE_MUX_BLD_REGS, route, data);

	bld_write(mux_o + DE_MUX_BLD_REGS, premultiply, 0);
	bld_write(mux_o + DE_MUX_BLD_REGS, bkcolor, 0xff000000);
	bld_write(mux_o + DE_MUX_BLD_REGS, bld_mode[0], 0x03010301);
								/* SRCOVER */
	bld_write(mux_o + DE_MUX_BLD_REGS, bld_mode[1], 0x03010301);
	bld_write(mux_o + DE_MUX_BLD_REGS, out_ctl, 0);

	/* disable the enhancements */
	writel_relaxed(0, mux_o + DE_MUX_VSU_REGS);
	writel_relaxed(0, mux_o + DE_MUX_GSU1_REGS);
	writel_relaxed(0, mux_o + DE_MUX_GSU2_REGS);
	writel_relaxed(0, mux_o + DE_MUX_GSU3_REGS);
	writel_relaxed(0, mux_o + DE_MUX_FCE_REGS);
	writel_relaxed(0, mux_o + DE_MUX_BWS_REGS);
	writel_relaxed(0, mux_o + DE_MUX_LTI_REGS);
	writel_relaxed(0, mux_o + DE_MUX_PEAK_REGS);
	writel_relaxed(0, mux_o + DE_MUX_ASE_REGS);
	writel_relaxed(0, mux_o + DE_MUX_FCC_REGS);
	writel_relaxed(0, mux_o + DE_MUX_DCSC_REGS);

	spin_unlock_irqrestore(&de_lock, flags);
}

void de2_de_disable(struct priv *priv, int lcd_num)
{
	u32 data;

	data = ~(1 << lcd_num);
	de_write(priv, DE_MOD_REG,
			de_read(priv, DE_MOD_REG) & data);
	de_write(priv, DE_GATE_REG,
			de_read(priv, DE_GATE_REG) & data);
	de_write(priv, DE_RESET_REG,
			de_read(priv, DE_RESET_REG) & data);
}

int de2_de_init(struct priv *priv, struct device *dev)
{
	struct resource *res;
	int ret;

	DRM_DEBUG_DRIVER("\n");

	res = platform_get_resource(to_platform_device(dev),
				IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get memory resource\n");
		return -EINVAL;
	}

	priv->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->mmio)) {
		dev_err(dev, "failed to map registers\n");
		return PTR_ERR(priv->mmio);
	}

	priv->gate = devm_clk_get(dev, "gate");	/* optional */

	priv->clk = devm_clk_get(dev, "clock");
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "video clock err %d\n", (int) PTR_ERR(priv->clk));
		return PTR_ERR(priv->clk);
	}

	priv->rstc = devm_reset_control_get_optional(dev, NULL);

	if (!IS_ERR(priv->rstc)) {
		ret = reset_control_deassert(priv->rstc);
		if (ret) {
			dev_err(dev, "reset deassert err %d\n", ret);
			return ret;
		}
	}

	if (!IS_ERR(priv->gate)) {
		ret = clk_prepare_enable(priv->gate);
		if (ret)
			goto err_gate;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		goto err_enable;
	if (priv->soc_type == SOC_A83T)
		clk_set_rate(priv->clk, DE_CLK_RATE_A83T);
	else
		clk_set_rate(priv->clk, DE_CLK_RATE_H3);

	/* set the A83T clock divider = 500 / 250 */
	if (priv->soc_type == SOC_A83T)
		de_write(priv, DE_DIV_REG,
				0x00000011);	/* div = 2 for both LCDs */

	return 0;

err_enable:
	clk_disable_unprepare(priv->gate);
err_gate:
	if (!IS_ERR(priv->rstc))
		reset_control_assert(priv->rstc);
	return ret;
}

void de2_de_cleanup(struct priv *priv)
{
	clk_disable_unprepare(priv->clk);
	clk_disable_unprepare(priv->gate);
	if (!IS_ERR(priv->rstc))
		reset_control_assert(priv->rstc);
}
