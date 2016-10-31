/*
 * Allwinner DRM driver - DE2 CRTC
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/component.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic_helper.h>
#include <asm/io.h>
#include <linux/of_irq.h>

#include "de2_drm.h"
#include "de2_crtc.h"

/* I/O map */

struct tcon {
	u32 gctl;
#define		TCON_GCTL_TCON_En BIT(31)
	u32 gint0;
#define		TCON_GINT0_TCON1_Vb_Int_En BIT(30)
#define		TCON_GINT0_TCON1_Vb_Int_Flag BIT(14)
	u32 gint1;
	u32 dum0[13];
	u32 tcon0_ctl;				/* 0x40 */
#define		TCON0_CTL_TCON_En BIT(31)
	u32 dum1[19];
	u32 tcon1_ctl;				/* 0x90 */
#define		TCON1_CTL_TCON_En BIT(31)
#define		TCON1_CTL_Interlace_En BIT(20)
#define		TCON1_CTL_Start_Delay_SHIFT 4
#define		TCON1_CTL_Start_Delay_MASK GENMASK(8, 4)
	u32 basic0;			/* XI/YI */
	u32 basic1;			/* LS_XO/LS_YO */
	u32 basic2;			/* XO/YO */
	u32 basic3;			/* HT/HBP */
	u32 basic4;			/* VT/VBP */
	u32 basic5;			/* HSPW/VSPW */
	u32 dum2;
	u32 ps_sync;				/* 0xb0 */
	u32 dum3[15];
	u32 io_pol;				/* 0xf0 */
#define		TCON1_IO_POL_IO0_inv BIT(24)
#define		TCON1_IO_POL_IO1_inv BIT(25)
#define		TCON1_IO_POL_IO2_inv BIT(26)
	u32 io_tri;
	u32 dum4[2];

	u32 ceu_ctl;				/* 0x100 */
#define     TCON_CEU_CTL_ceu_en BIT(31)
	u32 dum5[3];
	u32 ceu_rr;
	u32 ceu_rg;
	u32 ceu_rb;
	u32 ceu_rc;
	u32 ceu_gr;
	u32 ceu_gg;
	u32 ceu_gb;
	u32 ceu_gc;
	u32 ceu_br;
	u32 ceu_bg;
	u32 ceu_bb;
	u32 ceu_bc;
	u32 ceu_rv;
	u32 ceu_gv;
	u32 ceu_bv;
	u32 dum6[45];

	u32 mux_ctl;				/* 0x200 */
	u32 dum7[63];

	u32 fill_ctl;				/* 0x300 */
	u32 fill_start0;
	u32 fill_end0;
	u32 fill_data0;
};

#define XY(x, y) (((x) << 16) | (y))

#define tcon_read(base, member) \
	readl_relaxed(base + offsetof(struct tcon, member))
#define tcon_write(base, member, data) \
	writel_relaxed(data, base + offsetof(struct tcon, member))

/* vertical blank functions */
static void de2_atomic_flush(struct drm_crtc *crtc,
			struct drm_crtc_state *old_state)
{
	struct drm_pending_vblank_event *event = crtc->state->event;

	if (event) {
		crtc->state->event = NULL;
		spin_lock_irq(&crtc->dev->event_lock);
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irq(&crtc->dev->event_lock);
	}
}

static irqreturn_t de2_lcd_irq(int irq, void *dev_id)
{
	struct lcd *lcd = (struct lcd *) dev_id;
	u32 isr;

	isr = tcon_read(lcd->mmio, gint0);

	drm_crtc_handle_vblank(&lcd->crtc);

	tcon_write(lcd->mmio, gint0, isr & ~TCON_GINT0_TCON1_Vb_Int_Flag);

	return IRQ_HANDLED;
}

int de2_enable_vblank(struct drm_device *drm, unsigned crtc)
{
	struct priv *priv = drm->dev_private;
	struct lcd *lcd = priv->lcds[crtc];

	tcon_write(lcd->mmio, gint0,
			tcon_read(lcd->mmio, gint0) |
					TCON_GINT0_TCON1_Vb_Int_En);
	return 0;
}

void de2_disable_vblank(struct drm_device *drm, unsigned crtc)
{
	struct priv *priv = drm->dev_private;
	struct lcd *lcd = priv->lcds[crtc];

	tcon_write(lcd->mmio, gint0,
			 tcon_read(lcd->mmio, gint0) &
					~TCON_GINT0_TCON1_Vb_Int_En);
}

/* panel functions */
static void de2_set_frame_timings(struct lcd *lcd)
{
	struct drm_crtc *crtc = &lcd->crtc;
	const struct drm_display_mode *mode = &crtc->mode;
	int interlace = mode->flags & DRM_MODE_FLAG_INTERLACE ? 2 : 1;
	int start_delay;
	u32 data;

	data = XY(mode->hdisplay - 1, mode->vdisplay / interlace - 1);
	tcon_write(lcd->mmio, basic0, data);
	tcon_write(lcd->mmio, basic1, data);
	tcon_write(lcd->mmio, basic2, data);
	tcon_write(lcd->mmio, basic3,
			XY(mode->htotal - 1,
				mode->htotal - mode->hsync_start - 1));
	tcon_write(lcd->mmio, basic4,
			XY(mode->vtotal * (3 - interlace),
				mode->vtotal - mode->vsync_start - 1));
	tcon_write(lcd->mmio, basic5,
			 XY(mode->hsync_end - mode->hsync_start - 1,
				mode->vsync_end - mode->vsync_start - 1));

	tcon_write(lcd->mmio, ps_sync, XY(1, 1));

	data = TCON1_IO_POL_IO2_inv;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		data |= TCON1_IO_POL_IO0_inv;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		data |= TCON1_IO_POL_IO1_inv;
	tcon_write(lcd->mmio, io_pol, data);

	tcon_write(lcd->mmio, ceu_ctl,
		tcon_read(lcd->mmio, ceu_ctl) & ~TCON_CEU_CTL_ceu_en);

	data = tcon_read(lcd->mmio, tcon1_ctl);
	if (interlace == 2)
		data |= TCON1_CTL_Interlace_En;
	else
		data &= ~TCON1_CTL_Interlace_En;
	tcon_write(lcd->mmio, tcon1_ctl, data);

	tcon_write(lcd->mmio, fill_ctl, 0);
	tcon_write(lcd->mmio, fill_start0, mode->vtotal + 1);
	tcon_write(lcd->mmio, fill_end0, mode->vtotal);
	tcon_write(lcd->mmio, fill_data0, 0);

	start_delay = (mode->vtotal - mode->vdisplay) / interlace - 5;
	if (start_delay > 31)
		start_delay = 31;
	data = tcon_read(lcd->mmio, tcon1_ctl);
	data &= ~TCON1_CTL_Start_Delay_MASK;
	data |= start_delay << TCON1_CTL_Start_Delay_SHIFT;
	tcon_write(lcd->mmio, tcon1_ctl, data);

	tcon_write(lcd->mmio, io_tri, 0x0fffffff);
}

static void de2_crtc_enable(struct drm_crtc *crtc)
{
	struct lcd *lcd = crtc_to_lcd(crtc);
	struct drm_display_mode *mode = &crtc->mode;

	DRM_DEBUG_DRIVER("\n");

	clk_set_rate(lcd->clk, mode->clock * 1000);

	de2_set_frame_timings(lcd);

	tcon_write(lcd->mmio, tcon1_ctl,
		tcon_read(lcd->mmio, tcon1_ctl) | TCON1_CTL_TCON_En);

	de2_de_panel_init(lcd->priv, lcd->num, mode);

	drm_mode_debug_printmodeline(mode);
}

static void de2_crtc_disable(struct drm_crtc *crtc)
{
	struct lcd *lcd = crtc_to_lcd(crtc);
	unsigned long flags;

	DRM_DEBUG_DRIVER("\n");

	tcon_write(lcd->mmio, tcon1_ctl,
		tcon_read(lcd->mmio, tcon1_ctl) & ~TCON1_CTL_TCON_En);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		crtc->state->event = NULL;
	}
}

static const struct drm_crtc_funcs de2_crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_atomic_helper_set_config,
	.page_flip	= drm_atomic_helper_page_flip,
	.reset		= drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static const struct drm_crtc_helper_funcs de2_crtc_helper_funcs = {
	.atomic_flush	= de2_atomic_flush,
	.enable		= de2_crtc_enable,
	.disable	= de2_crtc_disable,
};

static void de2_tcon_init(struct lcd *lcd)
{
	tcon_write(lcd->mmio, tcon0_ctl,
		tcon_read(lcd->mmio, tcon0_ctl) & ~TCON0_CTL_TCON_En);
	tcon_write(lcd->mmio, tcon1_ctl,
		tcon_read(lcd->mmio, tcon1_ctl) & ~TCON1_CTL_TCON_En);
	tcon_write(lcd->mmio, gctl,
		tcon_read(lcd->mmio, gctl) & ~TCON_GCTL_TCON_En);

	/* disable/ack interrupts */
	tcon_write(lcd->mmio, gint0, 0);
}

static void de2_tcon_enable(struct lcd *lcd)
{
	tcon_write(lcd->mmio, gctl,
		tcon_read(lcd->mmio, gctl) | TCON_GCTL_TCON_En);
}

static int de2_crtc_init(struct drm_device *drm, struct lcd *lcd)
{
	struct drm_crtc *crtc = &lcd->crtc;
	int ret;

	ret = de2_plane_init(drm, lcd);
	if (ret < 0)
		return ret;

	drm_crtc_helper_add(crtc, &de2_crtc_helper_funcs);

	ret = drm_crtc_init_with_planes(drm, crtc,
					&lcd->planes[DE2_PRIMARY_PLANE],
					&lcd->planes[DE2_CURSOR_PLANE],
					&de2_crtc_funcs, NULL);
	if (ret < 0)
		return ret;

	de2_tcon_enable(lcd);

	de2_de_enable(lcd->priv, lcd->num);

	return 0;
}

/*
 * device init
 */
static int de2_lcd_bind(struct device *dev, struct device *master,
			void *data)
{
	struct drm_device *drm = data;
	struct priv *priv = drm->dev_private;
	struct lcd *lcd = dev_get_drvdata(dev);
	int ret;

	lcd->priv = priv;

	/* (only 2 LCDs) */
	lcd->crtc_idx = priv->lcds[0] ? 1 : 0;
	priv->lcds[lcd->crtc_idx] = lcd;

	ret = de2_crtc_init(drm, lcd);
	if (ret < 0) {
		dev_err(dev, "failed to init the crtc\n");
		return ret;
	}

	return 0;
}

static void de2_lcd_unbind(struct device *dev, struct device *master,
			void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lcd *lcd = platform_get_drvdata(pdev);

	if (lcd->mmio) {
		if (lcd->priv)
			de2_de_disable(lcd->priv, lcd->num);
		tcon_write(lcd->mmio, gctl,
			tcon_read(lcd->mmio, gctl) & ~TCON_GCTL_TCON_En);
	}
}

static const struct component_ops de2_lcd_ops = {
	.bind = de2_lcd_bind,
	.unbind = de2_lcd_unbind,
};

static int de2_lcd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *tmp, *parent, *port;
	struct lcd *lcd;
	struct resource *res;
	int id, irq, ret;

	id = of_alias_get_id(np, "lcd");
	if (id < 0) {
		dev_err(dev, "no alias for lcd\n");
		id = 0;
	}
	lcd = devm_kzalloc(dev, sizeof *lcd, GFP_KERNEL);
	if (!lcd) {
		dev_err(dev, "failed to allocate private data\n");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lcd);
	lcd->dev = dev;
	lcd->num = id;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get memory resource\n");
		return -EINVAL;
	}

	lcd->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(lcd->mmio)) {
		dev_err(dev, "failed to map registers\n");
		return PTR_ERR(lcd->mmio);
	}

	snprintf(lcd->name, sizeof(lcd->name), "sunxi-lcd%d", id);

	/* possible CRTCs */
	parent = np;
	tmp = of_get_child_by_name(np, "ports");
	if (tmp)
		parent = tmp;
	port = of_get_child_by_name(parent, "port");
	of_node_put(tmp);
	if (!port) {
		dev_err(dev, "no port node\n");
		return -ENXIO;
	}
	lcd->crtc.port = port;

	lcd->gate = devm_clk_get(dev, "gate");		/* optional */

	lcd->clk = devm_clk_get(dev, "clock");
	if (IS_ERR(lcd->clk)) {
		dev_err(dev, "video clock err %d\n", (int) PTR_ERR(lcd->clk));
		ret = PTR_ERR(lcd->clk);
		goto err;
	}

	lcd->rstc = devm_reset_control_get_optional(dev, NULL);

	irq = irq_of_parse_and_map(np, 0);
	if (irq <= 0 || irq == NO_IRQ) {
		dev_err(dev, "unable to get irq lcd %d\n", id);
		ret = -EINVAL;
		goto err;
	}

	if (!IS_ERR(lcd->rstc)) {
		ret = reset_control_deassert(lcd->rstc);
		if (ret) {
			dev_err(dev, "reset deassert err %d\n", ret);
			goto err;
		}
	}

	if (!IS_ERR(lcd->gate)) {
		ret = clk_prepare_enable(lcd->gate);
		if (ret)
			goto err2;
	}

	ret = clk_prepare_enable(lcd->clk);
	if (ret)
		goto err2;

	de2_tcon_init(lcd);

	ret = devm_request_irq(dev, irq, de2_lcd_irq, 0,
				lcd->name, lcd);
	if (ret < 0) {
		dev_err(dev, "unable to request irq %d\n", irq);
		goto err2;
	}

	return component_add(dev, &de2_lcd_ops);

err2:
	if (!IS_ERR_OR_NULL(lcd->rstc))
		reset_control_assert(lcd->rstc);
	clk_disable_unprepare(lcd->gate);
	clk_disable_unprepare(lcd->clk);
err:
	of_node_put(lcd->crtc.port);
	return ret;
}

static int de2_lcd_remove(struct platform_device *pdev)
{
	struct lcd *lcd = platform_get_drvdata(pdev);

	component_del(&pdev->dev, &de2_lcd_ops);

	if (!IS_ERR_OR_NULL(lcd->rstc))
		reset_control_assert(lcd->rstc);
	clk_disable_unprepare(lcd->gate);
	clk_disable_unprepare(lcd->clk);
	of_node_put(lcd->crtc.port);

	return 0;
}

static const struct of_device_id de2_lcd_ids[] = {
	{ .compatible = "allwinner,sun8i-a83t-lcd", },
	{ }
};

struct platform_driver de2_lcd_platform_driver = {
	.probe = de2_lcd_probe,
	.remove = de2_lcd_remove,
	.driver = {
		.name = "sunxi-de2-lcd",
		.of_match_table = of_match_ptr(de2_lcd_ids),
	},
};
