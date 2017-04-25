/*
 * Display driver for sunxi Allwinner SoCs with DE2.
 *
 * Copyright (C) 2016 Jernej Skrabec <jernej.skrabec@siol.net>
 *
 * Based on sunxi_display.c:
 * (C) Copyright 2013-2014 Luc Verhaegen <libv@skynet.be>
 * (C) Copyright 2014-2015 Hans de Goede <hdegoede@redhat.com>
 *
 * Based on Linux DRM driver:
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Copyright (c) 2016 Allwinnertech Co., Ltd.
 *
 * Based on rk_hdmi.c:
 * Copyright (c) 2015 Google, Inc
 * Copyright 2014 Rockchip Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>

#include <asm/arch/clock.h>
#include <asm/arch/display2.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <errno.h>
#include <malloc.h>
#include <fdt_support.h>
#include <video_fb.h>
#include "videomodes.h"

DECLARE_GLOBAL_DATA_PTR;

enum sunxi_monitor {
	sunxi_monitor_none,
	sunxi_monitor_dvi,
	sunxi_monitor_hdmi,
	sunxi_monitor_composite_pal,
	sunxi_monitor_composite_ntsc,
};
#define SUNXI_MONITOR_LAST sunxi_monitor_hdmi

struct sunxi_display {
	GraphicDevice graphic_device;
	enum sunxi_monitor monitor;
	unsigned int depth;
	unsigned int fb_addr;
	unsigned int fb_size;
} sunxi_display;

const struct ctfb_res_modes composite_video_modes[2] = {
	/*  x     y  hz  pixclk ps/kHz   le   ri  up  lo   hs vs  s  vmode */
	{ 720,  576, 50, 37037,  27000, 137,   5, 20, 27,   2, 2, 0, FB_VMODE_INTERLACED },
	{ 720,  480, 60, 37037,  27000, 116,  20, 16, 27,   2, 2, 0, FB_VMODE_INTERLACED },
};

static bool sunxi_is_composite(void)
{
	switch (sunxi_display.monitor) {
	case sunxi_monitor_none:
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
		return false;
	case sunxi_monitor_composite_pal:
	case sunxi_monitor_composite_ntsc:
		return true;
	}

	return false; /* Never reached */
}

static bool sunxi_is_hdmi(void)
{
	switch (sunxi_display.monitor) {
	case sunxi_monitor_none:
	case sunxi_monitor_composite_pal:
	case sunxi_monitor_composite_ntsc:
		return false;
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
		return true;
	}

	return false; /* Never reached */
}

#ifdef CONFIG_VIDEO_HDMI

static void sunxi_hdmi_phy_init(void)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	unsigned long tmo;
	u32 tmp;

	/*
	 * HDMI PHY settings are taken as-is from Allwinner BSP code.
	 * There is no documentation.
	 */
	writel(0, &hdmi->phy_ctrl);
	setbits_le32(&hdmi->phy_ctrl, BIT(0));
	udelay(5);
	setbits_le32(&hdmi->phy_ctrl, BIT(16));
	setbits_le32(&hdmi->phy_ctrl, BIT(1));
	udelay(10);
	setbits_le32(&hdmi->phy_ctrl, BIT(2));
	udelay(5);
	setbits_le32(&hdmi->phy_ctrl, BIT(3));
	udelay(40);
	setbits_le32(&hdmi->phy_ctrl, BIT(19));
	udelay(100);
	setbits_le32(&hdmi->phy_ctrl, BIT(18));
	setbits_le32(&hdmi->phy_ctrl, 7 << 4);

	/* Note that Allwinner code doesn't fail in case of timeout */
	tmo = timer_get_us() + 2000;
	while ((readl(&hdmi->phy_status) & 0x80) == 0) {
		if (timer_get_us() > tmo) {
			printf("Warning: HDMI PHY init timeout!\n");
			break;
		}
	}

	setbits_le32(&hdmi->phy_ctrl, 0xf << 8);
	setbits_le32(&hdmi->phy_ctrl, BIT(7));

	writel(0x39dc5040, &hdmi->phy_pll);
	writel(0x80084343, &hdmi->phy_clk);
	udelay(10000);
	writel(1, &hdmi->phy_unk3);
	setbits_le32(&hdmi->phy_pll, BIT(25));
	udelay(100000);
	tmp = (readl(&hdmi->phy_status) & 0x1f800) >> 11;
	setbits_le32(&hdmi->phy_pll, BIT(31) | BIT(30));
	setbits_le32(&hdmi->phy_pll, tmp);
	writel(0x01FF0F7F, &hdmi->phy_ctrl);
	writel(0x80639000, &hdmi->phy_unk1);
	writel(0x0F81C405, &hdmi->phy_unk2);

	/* enable read access to HDMI controller */
	writel(0x54524545, &hdmi->phy_read_en);
	/* descramble register offsets */
	writel(0x42494E47, &hdmi->phy_unscramble);
}

static void sunxi_hdmi_ctrl_init(void)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;

	/* soft reset HDMI controller */
	writeb(0x00, &hdmi->mc_swrstz);

	udelay(1);

	writeb(HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT |
	       HDMI_IH_MUTE_MUTE_ALL_INTERRUPT,
	       &hdmi->ih_mute);
	writeb(HDMI_I2CM_CTLINT_ADDR_NACK_POL |
	       HDMI_I2CM_CTLINT_ADDR_NACK_MSK |
	       HDMI_I2CM_CTLINT_ADDR_ARB_POL |
	       HDMI_I2CM_CTLINT_ADDR_ARB_MSK,
	       &hdmi->i2cm_ctlint);
	writeb(0xff & ~HDMI_MC_CLKDIS_TMDSCLK_DISABLE,
	       &hdmi->mc_clkdis);
}

static int sunxi_hdmi_hpd_detect(int hpd_delay)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	unsigned long tmo = timer_get_us() + hpd_delay * 1000;
	int status = 0;

	/* Set pll3 to 297 MHz */
	clock_set_pll3(297000000);

	/* Set hdmi parent to pll3 */
	clrsetbits_le32(&ccm->hdmi_clk_cfg, CCM_HDMI_CTRL_PLL_MASK,
			CCM_HDMI_CTRL_PLL3);

	/* Set ahb gating to pass */
	setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_HDMI);
	setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_HDMI2);
	setbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_HDMI);
	setbits_le32(&ccm->hdmi_slow_clk_cfg, CCM_HDMI_SLOW_CTRL_DDC_GATE);

	/* Clock on */
	setbits_le32(&ccm->hdmi_clk_cfg, CCM_HDMI_CTRL_GATE);

	sunxi_hdmi_phy_init();
	sunxi_hdmi_ctrl_init();

	while (timer_get_us() < tmo) {
		if (readl(&hdmi->phy_status) & SUNXI_HDMI_HPD_DETECT) {
			status = 1;
			break;
		}
	}

	return status;
}

static void sunxi_hdmi_shutdown(void)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;

	writel(0, &hdmi->phy_ctrl);
	clrbits_le32(&ccm->hdmi_clk_cfg, CCM_HDMI_CTRL_GATE);
	clrbits_le32(&ccm->hdmi_slow_clk_cfg, CCM_HDMI_SLOW_CTRL_DDC_GATE);
	clrbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_HDMI);
	clrbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_HDMI);
	clrbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_HDMI2);
	clock_set_pll3(0);
}

static int sunxi_hdmi_ddc_wait_i2c_done(int msec)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	u32 val;
	ulong start;

	start = get_timer(0);
	do {
		val = readb(&hdmi->ih_i2cm_stat0);
		writeb(val, &hdmi->ih_i2cm_stat0);

		if (val & 0x2)
			return 0;
		if (val & 0x1)
			return -EIO;

		udelay(100);
	} while (get_timer(start) < msec);

	return 1;
}

static int sunxi_hdmi_ddc_read(int block, u8 *buf)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	int shift = (block % 2) * 0x80;
	int trytime = 5;
	int edid_read_err = 0;
	u32 op = (block == 0) ? 1 : 2;
	int n;

	writeb(block >> 1, &hdmi->i2cm_segptr);

	while (trytime--) {
		edid_read_err = 0;

		for (n = 0; n < 128; n++) {
			writeb(shift + n, &hdmi->i2c_address);
			writeb(op, &hdmi->i2cm_operation);

			if (sunxi_hdmi_ddc_wait_i2c_done(10)) {
				edid_read_err = 1;
				break;
			}

			*buf++ = readb(&hdmi->i2cm_datai);
		}

		if (!edid_read_err)
			break;
	}

	return edid_read_err;
}

static int sunxi_hdmi_edid_get_block(int block, u8 *buf)
{
	int r, retries = 2;

	do {
		r = sunxi_hdmi_ddc_read(block, buf);
		if (r)
			continue;
		r = edid_check_checksum(buf);
		if (r) {
			printf("EDID block %d: checksum error%s\n",
			       block, retries ? ", retrying" : "");
		}
	} while (r && retries--);

	return r;
}

static int sunxi_hdmi_edid_get_mode(struct ctfb_res_modes *mode)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	struct edid1_info edid1;
	struct edid_cea861_info cea681[4];
	struct edid_detailed_timing *t =
		(struct edid_detailed_timing *)edid1.monitor_details.timing;
	int i, r, ext_blocks = 0;

	/* Reset i2c controller */
	writeb(0, &hdmi->i2cm_softrstz);

	writeb(0x05, &hdmi->i2cm_div);
	writeb(0x08, &hdmi->i2cm_int);

	/* set DDC timing*/
	writeb(0xd8, &hdmi->i2cm_ss_scl_hcnt_0_addr);
	writeb(0xfe, &hdmi->i2cm_ss_scl_lcnt_0_addr);

	writeb(HMDI_DDC_ADDR_SLAVE_ADDR, &hdmi->i2cm_slave);
	writeb(HMDI_DDC_ADDR_SEG_ADDR, &hdmi->i2cm_segaddr);

	r = sunxi_hdmi_edid_get_block(0, (u8 *)&edid1);
	if (r == 0) {
		r = edid_check_info(&edid1);
		if (r) {
			printf("EDID: invalid EDID data\n");
			r = -EINVAL;
		}
	}
	if (r == 0) {
		ext_blocks = edid1.extension_flag;
		if (ext_blocks > 4)
			ext_blocks = 4;
		for (i = 0; i < ext_blocks; i++) {
			if (sunxi_hdmi_edid_get_block(1 + i,
						(u8 *)&cea681[i]) != 0) {
				ext_blocks = i;
				break;
			}
		}
	}

	if (r)
		return r;

	/* We want version 1.3 or 1.2 with detailed timing info */
	if (edid1.version != 1 || (edid1.revision < 3 &&
			!EDID1_INFO_FEATURE_PREFERRED_TIMING_MODE(edid1))) {
		printf("EDID: unsupported version %d.%d\n",
		       edid1.version, edid1.revision);
		return -EINVAL;
	}

	/* Take the first usable detailed timing */
	for (i = 0; i < 4; i++, t++) {
		r = video_edid_dtd_to_ctfb_res_modes(t, mode);
		if (r == 0)
			break;
	}
	if (i == 4) {
		printf("EDID: no usable detailed timing found\n");
		return -ENOENT;
	}

	/* Check for basic audio support, if found enable hdmi output */
	sunxi_display.monitor = sunxi_monitor_dvi;
	for (i = 0; i < ext_blocks; i++) {
		if (cea681[i].extension_tag != EDID_CEA861_EXTENSION_TAG ||
		    cea681[i].revision < 2)
			continue;

		if (EDID_CEA861_SUPPORTS_BASIC_AUDIO(cea681[i]))
			sunxi_display.monitor = sunxi_monitor_hdmi;
	}

	return 0;
}

#endif /* CONFIG_VIDEO_HDMI */

/*
 * This is the entity that mixes and matches the different layers and inputs.
 * Allwinner calls it display engine, but here is called composer.
 */
static void sunxi_composer_init(void)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
#if defined(CONFIG_MACH_SUN50I) && !defined(CONFIG_MACH_SUN50I_H5)
	u32 reg_value;
 
	/* set SRAM for video use (A64 only) */
	reg_value = readl(SUNXI_SRAMC_BASE + 0x04);
	reg_value &= ~(0x01 << 24);
	writel(reg_value, SUNXI_SRAMC_BASE + 0x04);
#endif

	clock_set_pll10(432000000);

	/* Set DE parent to pll10 */
	clrsetbits_le32(&ccm->de_clk_cfg, CCM_DE2_CTRL_PLL_MASK,
			CCM_DE2_CTRL_PLL10);

	/* Set ahb gating to pass */
	setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_DE);
	setbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_DE);

	/* Clock on */
	setbits_le32(&ccm->de_clk_cfg, CCM_DE2_CTRL_GATE);
}

static void sunxi_composer_mode_set(int mux, const struct ctfb_res_modes *mode,
				    unsigned int address)
{
	void *de_mux_base = (mux == 0) ? SUNXI_DE2_MUX0_BASE : SUNXI_DE2_MUX1_BASE;
	struct de_clk * const de_clk_regs =
		(struct de_clk *)(SUNXI_DE2_BASE);
	struct de_glb * const de_glb_regs =
		(struct de_glb *)(de_mux_base +
				  SUNXI_DE2_MUX_GLB_REGS);
	struct de_bld * const de_bld_regs =
		(struct de_bld *)(de_mux_base +
				  SUNXI_DE2_MUX_BLD_REGS);
	struct de_ui * const de_ui_regs =
		(struct de_ui *)(de_mux_base +
				 SUNXI_DE2_MUX_CHAN_REGS +
				 SUNXI_DE2_MUX_CHAN_SZ * 1);
	struct de_csc * const de_csc_regs =
		(struct de_csc *)(de_mux_base +
				  SUNXI_DE2_MUX_DCSC_REGS);
	u32 size = SUNXI_DE2_WH(mode->xres, mode->yres);
	int channel;
	u32 data;

	/* enable clock */
#ifdef CONFIG_MACH_SUN8I_H3
	setbits_le32(&de_clk_regs->rst_cfg, (mux == 0) ? 1 : 4);
#else
	setbits_le32(&de_clk_regs->rst_cfg, (mux == 0) ? 1 : 2);
#endif
	setbits_le32(&de_clk_regs->gate_cfg, (mux == 0) ? 1 : 2);
	setbits_le32(&de_clk_regs->bus_cfg, (mux == 0) ? 1 : 2);

	clrbits_le32(&de_clk_regs->sel_cfg, 1);

	writel(SUNXI_DE2_MUX_GLB_CTL_RT_EN, &de_glb_regs->ctl);
	writel(0, &de_glb_regs->status);
	writel(1, &de_glb_regs->dbuff);
	writel(size, &de_glb_regs->size);

	for (channel = 0; channel < 4; channel++) {
		void *chan = de_mux_base + SUNXI_DE2_MUX_CHAN_REGS +
			SUNXI_DE2_MUX_CHAN_SZ * channel;
		memset(chan, 0, channel == 0 ?
			sizeof(struct de_vi) : sizeof(struct de_ui));
	}
	memset(de_bld_regs, 0, sizeof(struct de_bld));

	writel(0x00000101, &de_bld_regs->fcolor_ctl);

	writel(1, &de_bld_regs->route);

	writel(0, &de_bld_regs->premultiply);
	writel(0xff000000, &de_bld_regs->bkcolor);

	writel(0x03010301, &de_bld_regs->bld_mode[0]);

	writel(size, &de_bld_regs->output_size);
	writel(mode->vmode & FB_VMODE_INTERLACED ? 2 : 0,
		&de_bld_regs->out_ctl);
	writel(0, &de_bld_regs->ck_ctl);

	writel(0xff000000, &de_bld_regs->attr[0].fcolor);
	writel(size, &de_bld_regs->attr[0].insize);

	/* Disable all other units */
	writel(0, de_mux_base + SUNXI_DE2_MUX_VSU_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_GSU1_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_GSU2_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_GSU3_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_FCE_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_BWS_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_LTI_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_PEAK_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_ASE_REGS);
	writel(0, de_mux_base + SUNXI_DE2_MUX_FCC_REGS);

	if (sunxi_is_composite()) {
		writel(0x107, &de_csc_regs->coef11);
		writel(0x204, &de_csc_regs->coef12);
		writel(0x64, &de_csc_regs->coef13);
		writel(0x4200, &de_csc_regs->coef14);
		writel(0x1f68, &de_csc_regs->coef21);
		writel(0x1ed6, &de_csc_regs->coef22);
		writel(0x1c2, &de_csc_regs->coef23);
		writel(0x20200, &de_csc_regs->coef24);
		writel(0x1c2, &de_csc_regs->coef31);
		writel(0x1e87, &de_csc_regs->coef32);
		writel(0x1fb7, &de_csc_regs->coef33);
		writel(0x20200, &de_csc_regs->coef34);
		writel(0x20200, &de_csc_regs->alpha);

		writel(1, &de_csc_regs->csc_ctl);
	} else {
		writel(0, &de_csc_regs->csc_ctl);
	}

	data = SUNXI_DE2_UI_CFG_ATTR_EN |
	       SUNXI_DE2_UI_CFG_ATTR_FMT(SUNXI_DE2_FORMAT_XRGB_8888) |
	       SUNXI_DE2_UI_CFG_ATTR_ALPMOD(1) |
	       SUNXI_DE2_UI_CFG_ATTR_ALPHA(0xff);
	writel(data, &de_ui_regs->cfg[0].attr);
	writel(size, &de_ui_regs->cfg[0].size);
	writel(0, &de_ui_regs->cfg[0].coord);
	writel(4 * mode->xres, &de_ui_regs->cfg[0].pitch);
	writel(address, &de_ui_regs->cfg[0].top_laddr);
	writel(size, &de_ui_regs->ovl_size);
}

static void sunxi_composer_enable(int mux)
{
	struct de_glb *de_glb_regs;

	if (mux == 0)
		de_glb_regs = (struct de_glb *)(SUNXI_DE2_MUX0_BASE +
				  SUNXI_DE2_MUX_GLB_REGS);
	else
		de_glb_regs = (struct de_glb *)(SUNXI_DE2_MUX1_BASE +
				  SUNXI_DE2_MUX_GLB_REGS);

	writel(1, &de_glb_regs->dbuff);
}

/*
 * LCDC, what allwinner calls a CRTC, so timing controller and serializer.
 */
static void sunxi_lcdc_pll_set(int dotclock, int *clk_div)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	int value, n, m, x = 0, diff;
	int best_n = 0, best_m = 0, best_diff = 0x0FFFFFFF;

	/*
	 * Due to unknown registers in HDMI PHY, we know correct settings
	 * only for following four PHY dividers. Select one based on
	 * clock speed.
	 */
	if (dotclock <= 27000)
		x = 11;
	else if (dotclock <= 74250)
		x = 4;
	else if (dotclock <= 148500)
		x = 2;
	else
		x = 1;

	/*
	 * Find the lowest divider resulting in a matching clock. If there
	 * is no match, pick the closest lower clock, as monitors tend to
	 * not sync to higher frequencies.
	 */
	for (m = 1; m <= 16; m++) {
		n = (m * x * dotclock) / 24000;

		if ((n >= 1) && (n <= 128)) {
			value = (24000 * n) / m / x;
			diff = dotclock - value;
			if (diff < best_diff) {
				best_diff = diff;
				best_m = m;
				best_n = n;
			}
		}
	}

	clock_set_pll3_factors(best_m, best_n);
	debug("dotclock: %dkHz = %dkHz: (24MHz * %d) / %d / %d\n",
	      dotclock, (clock_get_pll3() / 1000) / x,
	      best_n, best_m, x);

#if defined(CONFIG_MACH_SUN8I_H3) || defined(CONFIG_MACH_SUN50I_H5)
	writel(CCM_TCON0_CTRL_GATE | CCM_TCON0_CTRL_M(x),
	       &ccm->tcon0_clk_cfg);
#else
	writel(CCM_TCON1_CTRL_GATE | CCM_TCON1_CTRL_M(x),
	       &ccm->tcon1_clk_cfg);
#endif

	*clk_div = x;
}

static void sunxi_lcdc_init(int num)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_lcdc_reg *lcdc;

	if (num == 0) {
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD0_BASE;

		/* Reset off */
		setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_TCON0);

		/* Clock on */
		setbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_TCON0);
		setbits_le32(&ccm->tcon0_clk_cfg, CCM_TCON0_CTRL_GATE);
	} else {
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD1_BASE;

		/* Reset off */
		setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_TCON1);

		/* Clock on */
		setbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_TCON1);
#if !defined(CONFIG_MACH_SUN50I_H5) && defined(CONFIG_MACH_SUN50I)
		setbits_le32(&ccm->tcon1_clk_cfg, CCM_TCON1_CTRL_GATE);
#endif
	}

	/* Init lcdc */
	writel(0, &lcdc->ctrl); /* Disable tcon */
	writel(0, &lcdc->int0); /* Disable all interrupts */

	/* Set all io lines to tristate */
	writel(0x0fffffff, &lcdc->tcon1_io_tristate);
}

static void sunxi_lcdc_enable(int num)
{
	struct sunxi_lcdc_reg *lcdc;

	if (num == 0)
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD0_BASE;
	else
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD1_BASE;

	setbits_le32(&lcdc->ctrl, SUNXI_LCDC_CTRL_TCON_ENABLE);
}

static int sunxi_lcdc_get_clk_delay(const struct ctfb_res_modes *mode)
{
	int delay;

	delay = mode->lower_margin + mode->vsync_len + mode->upper_margin;
	if (mode->vmode == FB_VMODE_INTERLACED)
		delay /= 2;
	delay -= 2;

	return (delay > 31) ? 31 : delay;
}

#if defined CONFIG_VIDEO_HDMI || defined CONFIG_VIDEO_COMPOSITE
static void sunxi_lcdc_tcon1_mode_set(int num,
				      const struct ctfb_res_modes *mode,
				      int *clk_div)
{
	struct sunxi_lcdc_reg *lcdc;
	int bp, clk_delay, total, yres;

	if (num == 0)
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD0_BASE;
	else
		lcdc = (struct sunxi_lcdc_reg *)SUNXI_LCD1_BASE;

	clk_delay = sunxi_lcdc_get_clk_delay(mode);
	writel(SUNXI_LCDC_TCON1_CTRL_ENABLE |
	       ((mode->vmode == FB_VMODE_INTERLACED) ?
			SUNXI_LCDC_TCON1_CTRL_INTERLACE_ENABLE : 0) |
	       SUNXI_LCDC_TCON1_CTRL_CLK_DELAY(clk_delay), &lcdc->tcon1_ctrl);

	yres = mode->yres;
	if (mode->vmode == FB_VMODE_INTERLACED)
		yres /= 2;
	writel(SUNXI_LCDC_X(mode->xres) | SUNXI_LCDC_Y(yres),
	       &lcdc->tcon1_timing_source);
	writel(SUNXI_LCDC_X(mode->xres) | SUNXI_LCDC_Y(yres),
	       &lcdc->tcon1_timing_scale);
	writel(SUNXI_LCDC_X(mode->xres) | SUNXI_LCDC_Y(yres),
	       &lcdc->tcon1_timing_out);

	bp = mode->hsync_len + mode->left_margin;
	total = mode->xres + mode->right_margin + bp;
	writel(SUNXI_LCDC_TCON1_TIMING_H_TOTAL(total) |
	       SUNXI_LCDC_TCON1_TIMING_H_BP(bp), &lcdc->tcon1_timing_h);

	bp = mode->vsync_len + mode->upper_margin;
	total = mode->yres + mode->lower_margin + bp;
	if (mode->vmode == FB_VMODE_NONINTERLACED)
		total *= 2;
	writel(SUNXI_LCDC_TCON1_TIMING_V_TOTAL(total) |
	       SUNXI_LCDC_TCON1_TIMING_V_BP(bp), &lcdc->tcon1_timing_v);

	writel(SUNXI_LCDC_X(mode->hsync_len) | SUNXI_LCDC_Y(mode->vsync_len),
	       &lcdc->tcon1_timing_sync);

	if (!sunxi_is_composite())
		sunxi_lcdc_pll_set(mode->pixclock_khz, clk_div);
}
#endif /* CONFIG_VIDEO_HDMI || CONFIG_VIDEO_COMPOSITE */

#ifdef CONFIG_VIDEO_HDMI

static void sunxi_hdmi_setup_info_frames(const struct ctfb_res_modes *mode)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	u8 tmp;

	if (mode->pixclock_khz <= 27000)
		tmp = 0x40; /* SD-modes, ITU601 colorspace */
	else
		tmp = 0x80; /* HD-modes, ITU709 colorspace */

	if (mode->xres * 100 / mode->yres < 156)
		tmp |= 0x18;  /* 4 : 3 */
	else
		tmp |= 0x28; /* 16 : 9 */

	setbits_8(&hdmi->fc_invidconf,
		  HDMI_FC_INVIDCONF_DVI_MODE_HDMI);
	writeb(HDMI_FC_AVICONF0_ACTIVE_FORMAT |
	       HDMI_FC_AVICONF0_SCAN_INFO_UNDERSCAN,
	       &hdmi->fc_aviconf0);
	writeb(tmp, &hdmi->fc_aviconf1);
	writeb(HDMI_FC_AVICONF2_RGB_QUANT_FULL_RANGE |
	       HDMI_FC_AVICONF2_IT_CONTENT_VALID,
	       &hdmi->fc_aviconf2);
}

static void sunxi_hdmi_phy_set(u32 divider)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	u32 tmp;

	/*
	 * Unfortunatelly, we don't know much about those magic
	 * numbers. They are taken from Allwinner BSP driver.
	 */
	switch (divider) {
	case 1:
		writel(0x30dc5fc0, &hdmi->phy_pll);
		writel(0x800863C0, &hdmi->phy_clk);
		mdelay(10);
		writel(0x00000001, &hdmi->phy_unk3);
		setbits_le32(&hdmi->phy_pll, BIT(25));
		mdelay(200);
		tmp = (readl(&hdmi->phy_status) & 0x1f800) >> 11;
		setbits_le32(&hdmi->phy_pll, BIT(31) | BIT(30));
		if (tmp < 0x3d)
			setbits_le32(&hdmi->phy_pll, tmp + 2);
		else
			setbits_le32(&hdmi->phy_pll, 0x3f);
		mdelay(100);
		writel(0x01FFFF7F, &hdmi->phy_ctrl);
		writel(0x8063b000, &hdmi->phy_unk1);
		writel(0x0F8246B5, &hdmi->phy_unk2);
		break;
	case 2:
		writel(0x39dc5040, &hdmi->phy_pll);
		writel(0x80084381, &hdmi->phy_clk);
		mdelay(10);
		writel(0x00000001, &hdmi->phy_unk3);
		setbits_le32(&hdmi->phy_pll, BIT(25));
		mdelay(100);
		tmp = (readl(&hdmi->phy_status) & 0x1f800) >> 11;
		setbits_le32(&hdmi->phy_pll, BIT(31) | BIT(30));
		setbits_le32(&hdmi->phy_pll, tmp);
		writel(0x01FFFF7F, &hdmi->phy_ctrl);
		writel(0x8063a800, &hdmi->phy_unk1);
		writel(0x0F81C485, &hdmi->phy_unk2);
		break;
	case 4:
		writel(0x39dc5040, &hdmi->phy_pll);
		writel(0x80084343, &hdmi->phy_clk);
		mdelay(10);
		writel(0x00000001, &hdmi->phy_unk3);
		setbits_le32(&hdmi->phy_pll, BIT(25));
		mdelay(100);
		tmp = (readl(&hdmi->phy_status) & 0x1f800) >> 11;
		setbits_le32(&hdmi->phy_pll, BIT(31) | BIT(30));
		setbits_le32(&hdmi->phy_pll, tmp);
		writel(0x01FFFF7F, &hdmi->phy_ctrl);
		writel(0x8063b000, &hdmi->phy_unk1);
		writel(0x0F81C405, &hdmi->phy_unk2);
		break;
	case 11:
		writel(0x39dc5040, &hdmi->phy_pll);
		writel(0x8008430a, &hdmi->phy_clk);
		mdelay(10);
		writel(0x00000001, &hdmi->phy_unk3);
		setbits_le32(&hdmi->phy_pll, BIT(25));
		mdelay(100);
		tmp = (readl(&hdmi->phy_status) & 0x1f800) >> 11;
		setbits_le32(&hdmi->phy_pll, BIT(31) | BIT(30));
		setbits_le32(&hdmi->phy_pll, tmp);
		writel(0x01FFFF7F, &hdmi->phy_ctrl);
		writel(0x8063b000, &hdmi->phy_unk1);
		writel(0x0F81C405, &hdmi->phy_unk2);
		break;
	}
}

static void sunxi_hdmi_mode_set(const struct ctfb_res_modes *mode,
				int clk_div)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;
	u8 invidconf, v_blanking;
	u32 h_blanking;

	sunxi_hdmi_phy_set(clk_div);

	invidconf = 0;
	if (mode->vmode & FB_VMODE_INTERLACED)
		invidconf |= 0x01;
	if (mode->sync & FB_SYNC_HOR_HIGH_ACT)
		invidconf |= 0x20;
	if (mode->sync & FB_SYNC_VERT_HIGH_ACT)
		invidconf |= 0x40;

	h_blanking = mode->left_margin + mode->right_margin + mode->hsync_len;
	v_blanking = mode->upper_margin + mode->lower_margin + mode->vsync_len;

	writeb(invidconf |
	       HDMI_FC_INVIDCONF_DE_IN_POL_ACTIVE_HIGH,
	       &hdmi->fc_invidconf);
	if (invidconf < 96)
		setbits_le32(&hdmi->phy_pol, 0x300);

	writeb(mode->xres, &hdmi->fc_inhactv0);
	writeb(mode->xres >> 8, &hdmi->fc_inhactv1);
	writeb(h_blanking, &hdmi->fc_inhblank0);
	writeb(h_blanking >> 8, &hdmi->fc_inhblank1);
	writeb(mode->yres, &hdmi->fc_invactv0);
	writeb(mode->yres >> 8, &hdmi->fc_invactv1);
	writeb(v_blanking, &hdmi->fc_invblank);
	writeb(mode->right_margin, &hdmi->fc_hsyncindelay0);
	writeb(mode->right_margin >> 8, &hdmi->fc_hsyncindelay1);
	writeb(mode->hsync_len, &hdmi->fc_hsyncinwidth0);
	writeb(mode->hsync_len >> 8, &hdmi->fc_hsyncinwidth1);
	writeb(mode->lower_margin, &hdmi->fc_vsyncindelay);
	writeb(mode->vsync_len, &hdmi->fc_vsyncinwidth);

	/* control period minimum duration */
	writeb(0x0c, &hdmi->fc_ctrldur);
	writeb(0x20, &hdmi->fc_exctrldur);
	writeb(0x01, &hdmi->fc_exctrlspac);

	/* set to fill tmds data channels */
	writeb(0x0b, &hdmi->fc_ch0pream);
	writeb(0x16, &hdmi->fc_ch1pream);
	writeb(0x21, &hdmi->fc_ch2pream);

	writeb(0x40, &hdmi->vp_pr_cd);
	writeb(0x07, &hdmi->vp_stuff);
	writeb(0x00, &hdmi->vp_remap);
	writeb(0x47, &hdmi->vp_conf);

	writeb(0x01, &hdmi->tx_invid0);

	/* enable tx stuffing: when de is inactive, fix the output data to 0 */
	writeb(HDMI_TX_INSTUFFING_BDBDATA_STUFFING_EN |
	       HDMI_TX_INSTUFFING_RCRDATA_STUFFING_EN |
	       HDMI_TX_INSTUFFING_GYDATA_STUFFING_EN,
	       &hdmi->tx_instuffing);
	writeb(0x00, &hdmi->tx_gydata0);
	writeb(0x00, &hdmi->tx_gydata1);
	writeb(0x00, &hdmi->tx_rcrdata0);
	writeb(0x00, &hdmi->tx_rcrdata1);
	writeb(0x00, &hdmi->tx_bcbdata0);
	writeb(0x00, &hdmi->tx_bcbdata1);

	if (sunxi_display.monitor == sunxi_monitor_hdmi)
		sunxi_hdmi_setup_info_frames(mode);

	writeb(HDMI_MC_FLOWCTRL_CSC_BYPASS, &hdmi->mc_flowctrl);
	/* enable audio, TMDS and pixel clock */
	writeb(0x74, &hdmi->mc_clkdis);

	/*
	 * This is last hdmi access before boot,
	 * so scramble addresses again. Othwerwise
	 * BSP or current DRM driver won't work.
	 * Dummy read is needed or otherwise last
	 * write doesn't get written correctly.
	 */
	(void)readb(&hdmi->reserved0[0]);
	writel(0, &hdmi->phy_unscramble);
}

static void sunxi_hdmi_enable(void)
{
	struct sunxi_dwc_hdmi * const hdmi =
		(struct sunxi_dwc_hdmi *)SUNXI_HDMI_BASE;

	setbits_le32(&hdmi->phy_ctrl, 0xf << 12);
	printf("hdmi enabled\n");
}

#endif /* CONFIG_VIDEO_HDMI */

#ifdef CONFIG_VIDEO_COMPOSITE

static void sunxi_tvencoder_mode_set(void)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	struct sunxi_tve_reg * const tve =
		(struct sunxi_tve_reg *)SUNXI_TVE0_BASE;

	/* Reset off */
	setbits_le32(&ccm->ahb_reset1_cfg, 1 << AHB_RESET_OFFSET_TVE);

	/* Clock on */
	setbits_le32(&ccm->ahb_gate1, 1 << AHB_GATE_OFFSET_TVE);
	writel(CCM_TVE_CTRL_GATE | CCM_TVE_CTRL_M(2), &ccm->tve_clk_cfg);

	switch (sunxi_display.monitor) {
	case sunxi_monitor_composite_pal:
		writel(SUNXI_TVE_GCTRL_DAC_INPUT(0, 1) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(1, 2) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(2, 3) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(3, 4), &tve->gctrl);
		writel(SUNXI_TVE_CFG0_PAL, &tve->cfg0);
		writel(SUNXI_TVE_DAC_CFG0_COMPOSITE, &tve->dac_cfg0);
		writel(SUNXI_TVE_FILTER_COMPOSITE, &tve->filter);
		writel(0x2a098acb, &tve->chroma_freq);
		writel(SUNXI_TVE_PORCH_NUM_PAL, &tve->porch_num);
		writel(SUNXI_TVE_LINE_NUM_PAL, &tve->line_num);
		writel(SUNXI_TVE_BLANK_BLACK_LEVEL_PAL, &tve->blank_black_level);
		writel(SUNXI_TVE_UNKNOWN1_COMPOSITE, &tve->unknown1);
		writel(SUNXI_TVE_CBR_LEVEL_PAL, &tve->cbr_level);
		writel(SUNXI_TVE_BURST_WIDTH_COMPOSITE, &tve->burst_width);
		writel(SUNXI_TVE_UNKNOWN2_PAL, &tve->unknown2);
		writel(SUNXI_TVE_ACTIVE_NUM_COMPOSITE, &tve->active_num);
		writel(SUNXI_TVE_CHROMA_BW_GAIN_COMP, &tve->chroma_bw_gain);
		writel(SUNXI_TVE_NOTCH_WIDTH_COMPOSITE, &tve->notch_width);
		writel(SUNXI_TVE_RESYNC_NUM_PAL, &tve->resync_num);
		writel(SUNXI_TVE_SLAVE_PARA_COMPOSITE, &tve->slave_para);
		break;
	case sunxi_monitor_composite_ntsc:
		writel(SUNXI_TVE_GCTRL_DAC_INPUT(0, 1) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(1, 2) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(2, 3) |
		       SUNXI_TVE_GCTRL_DAC_INPUT(3, 4), &tve->gctrl);
		writel(SUNXI_TVE_CFG0_NTSC, &tve->cfg0);
		writel(SUNXI_TVE_DAC_CFG0_COMPOSITE, &tve->dac_cfg0);
		writel(SUNXI_TVE_FILTER_COMPOSITE, &tve->filter);
		writel(SUNXI_TVE_PORCH_NUM_NTSC, &tve->porch_num);
		writel(SUNXI_TVE_LINE_NUM_NTSC, &tve->line_num);
		writel(SUNXI_TVE_BLANK_BLACK_LEVEL_NTSC, &tve->blank_black_level);
		writel(SUNXI_TVE_UNKNOWN1_COMPOSITE, &tve->unknown1);
		writel(SUNXI_TVE_CBR_LEVEL_NTSC, &tve->cbr_level);
		writel(SUNXI_TVE_BURST_PHASE_NTSC, &tve->burst_phase);
		writel(SUNXI_TVE_BURST_WIDTH_COMPOSITE, &tve->burst_width);
		writel(SUNXI_TVE_UNKNOWN2_NTSC, &tve->unknown2);
		writel(SUNXI_TVE_SYNC_VBI_LEVEL_NTSC, &tve->sync_vbi_level);
		writel(SUNXI_TVE_ACTIVE_NUM_COMPOSITE, &tve->active_num);
		writel(SUNXI_TVE_CHROMA_BW_GAIN_COMP, &tve->chroma_bw_gain);
		writel(SUNXI_TVE_NOTCH_WIDTH_COMPOSITE, &tve->notch_width);
		writel(SUNXI_TVE_RESYNC_NUM_NTSC, &tve->resync_num);
		writel(SUNXI_TVE_SLAVE_PARA_COMPOSITE, &tve->slave_para);
		break;
	case sunxi_monitor_none:
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
		break;
	}
}

static void sunxi_tvencoder_enable(void)
{
	struct sunxi_tve_reg * const tve =
		(struct sunxi_tve_reg *)SUNXI_TVE0_BASE;

	setbits_le32(&tve->gctrl, SUNXI_TVE_GCTRL_ENABLE);
}

#endif /* CONFIG_VIDEO_COMPOSITE */

static void sunxi_engines_init(void)
{
#if defined(CONFIG_MACH_SUN50I_H5) || defined(CONFIG_MACH_SUN8I_H3)
	int mux = sunxi_is_hdmi() ? 0 : 1;
#else
	int mux = sunxi_is_hdmi() ? 1 : 0;
#endif
	sunxi_composer_init();
	sunxi_lcdc_init(mux);
}

static void sunxi_mode_set(const struct ctfb_res_modes *mode,
			   unsigned int address)
{
	int __maybe_unused clk_div;
	int __maybe_unused mux;

	switch (sunxi_display.monitor) {
	case sunxi_monitor_none:
		break;
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
#ifdef CONFIG_VIDEO_HDMI
#if defined(CONFIG_MACH_SUN50I_H5) || defined(CONFIG_MACH_SUN8I_H3)
		mux = 0;
#else
		mux = 1;
#endif
		sunxi_composer_mode_set(mux, mode, address);
		sunxi_lcdc_tcon1_mode_set(mux, mode, &clk_div);
		sunxi_hdmi_mode_set(mode, clk_div);
		sunxi_composer_enable(mux);
		sunxi_lcdc_enable(mux);
		sunxi_hdmi_enable();
#endif
		break;
	case sunxi_monitor_composite_pal:
	case sunxi_monitor_composite_ntsc:
#ifdef CONFIG_VIDEO_COMPOSITE
		sunxi_composer_mode_set(1, mode, address);
		sunxi_lcdc_tcon1_mode_set(1, mode, &clk_div);
		sunxi_tvencoder_mode_set();
		sunxi_composer_enable(1);
		sunxi_lcdc_enable(1);
		sunxi_tvencoder_enable();
#endif
		break;
	}
}

static const char *sunxi_get_mon_desc(enum sunxi_monitor monitor)
{
	switch (monitor) {
	case sunxi_monitor_none:		return "none";
	case sunxi_monitor_dvi:			return "dvi";
	case sunxi_monitor_hdmi:		return "hdmi";
	case sunxi_monitor_composite_pal:	return "composite-pal";
	case sunxi_monitor_composite_ntsc:	return "composite-ntsc";
	}
	return NULL; /* never reached */
}

ulong board_get_usable_ram_top(ulong total_size)
{
	return gd->ram_top - CONFIG_SUNXI_MAX_FB_SIZE;
}

static bool sunxi_has_hdmi(void)
{
#ifdef CONFIG_VIDEO_HDMI
	return true;
#else
	return false;
#endif
}

static bool sunxi_has_composite(void)
{
#ifdef CONFIG_VIDEO_COMPOSITE
	return true;
#else
	return false;
#endif
}

static enum sunxi_monitor sunxi_get_default_mon(bool allow_hdmi)
{
	if (allow_hdmi && sunxi_has_hdmi())
		return sunxi_monitor_dvi;
	else if (sunxi_has_composite())
		return sunxi_monitor_composite_pal;
	else
		return sunxi_monitor_none;
}

void *video_hw_init(void)
{
	static GraphicDevice *graphic_device = &sunxi_display.graphic_device;
	const struct ctfb_res_modes *mode;
	struct ctfb_res_modes custom;
	const char *options;
#ifdef CONFIG_VIDEO_HDMI
	int ret, hpd, hpd_delay, edid;
#endif
	int i, overscan_offset, overscan_x, overscan_y;
	unsigned int fb_dma_addr;
	char mon[16];

	memset(&sunxi_display, 0, sizeof(struct sunxi_display));

	video_get_ctfb_res_modes(RES_MODE_1024x768, 24, &mode,
				 &sunxi_display.depth, &options);
#ifdef CONFIG_VIDEO_HDMI
	hpd = video_get_option_int(options, "hpd", 1);
	hpd_delay = video_get_option_int(options, "hpd_delay", 500);
	edid = video_get_option_int(options, "edid", 1);
#endif
	overscan_x = video_get_option_int(options, "overscan_x", -1);
	overscan_y = video_get_option_int(options, "overscan_y", -1);
	sunxi_display.monitor = sunxi_get_default_mon(true);
	video_get_option_string(options, "monitor", mon, sizeof(mon),
				sunxi_get_mon_desc(sunxi_display.monitor));
	for (i = 0; i <= SUNXI_MONITOR_LAST; i++) {
		if (strcmp(mon, sunxi_get_mon_desc(i)) == 0) {
			sunxi_display.monitor = i;
			break;
		}
	}
	if (i > SUNXI_MONITOR_LAST)
		printf("Unknown monitor: '%s', falling back to '%s'\n",
		       mon, sunxi_get_mon_desc(sunxi_display.monitor));

#ifdef CONFIG_VIDEO_HDMI
	/* If HDMI/DVI is selected do HPD & EDID, and handle fallback */
	if (sunxi_display.monitor == sunxi_monitor_dvi ||
	    sunxi_display.monitor == sunxi_monitor_hdmi) {
		/* Always call hdp_detect, as it also enables clocks, etc. */
		ret = sunxi_hdmi_hpd_detect(hpd_delay);
		if (ret) {
			printf("HDMI connected: ");
			if (edid && sunxi_hdmi_edid_get_mode(&custom) == 0)
				mode = &custom;
		} else if (hpd) {
			sunxi_hdmi_shutdown();
			sunxi_display.monitor = sunxi_get_default_mon(false);
		} /* else continue with hdmi/dvi without a cable connected */
	}
#endif

	switch (sunxi_display.monitor) {
	case sunxi_monitor_none:
		return NULL;
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
		if (!sunxi_has_hdmi()) {
			printf("HDMI/DVI not supported on this board\n");
			sunxi_display.monitor = sunxi_monitor_none;
			return NULL;
		}
		break;
	case sunxi_monitor_composite_pal:
	case sunxi_monitor_composite_ntsc:
		if (!sunxi_has_composite()) {
			printf("Composite video not supported on this board\n");
			sunxi_display.monitor = sunxi_monitor_none;
			return NULL;
		}
		if (sunxi_display.monitor == sunxi_monitor_composite_pal)
			mode = &composite_video_modes[0];
		else
			mode = &composite_video_modes[1];
		sunxi_display.depth = 24;
		break;
	}

	/* Yes these defaults are quite high, overscan on composite sucks... */
	if (overscan_x == -1)
		overscan_x = sunxi_is_composite() ? 32 : 0;
	if (overscan_y == -1)
		overscan_y = sunxi_is_composite() ? 20 : 0;

	sunxi_display.fb_size =
		(mode->xres * mode->yres * 4 + 0xfff) & ~0xfff;
	overscan_offset = (overscan_y * mode->xres + overscan_x) * 4;
	/* We want to keep the fb_base for simplefb page aligned, where as
	 * the sunxi dma engines will happily accept an unaligned address. */
	if (overscan_offset)
		sunxi_display.fb_size += 0x1000;

	if (sunxi_display.fb_size > CONFIG_SUNXI_MAX_FB_SIZE) {
		printf("Error need %dkB for fb, but only %dkB is reserved\n",
		       sunxi_display.fb_size >> 10,
		       CONFIG_SUNXI_MAX_FB_SIZE >> 10);
		return NULL;
	}

	printf("Setting up a %dx%d%s %s console (overscan %dx%d)\n",
	       mode->xres, mode->yres,
	       (mode->vmode == FB_VMODE_INTERLACED) ? "i" : "",
	       sunxi_get_mon_desc(sunxi_display.monitor),
	       overscan_x, overscan_y);

	gd->fb_base = (unsigned long)memalign(0x1000, sunxi_display.fb_size);
	sunxi_engines_init();

	fb_dma_addr = gd->fb_base;
	sunxi_display.fb_addr = gd->fb_base;
	if (overscan_offset) {
		fb_dma_addr += 0x1000 - (overscan_offset & 0xfff);
		sunxi_display.fb_addr += (overscan_offset + 0xfff) & ~0xfff;
		memset((void *)gd->fb_base, 0, sunxi_display.fb_size);
		flush_cache(gd->fb_base, sunxi_display.fb_size);
	}
	sunxi_mode_set(mode, fb_dma_addr);

	/*
	 * These are the only members of this structure that are used. All the
	 * others are driver specific. The pitch is stored in plnSizeX.
	 */
	graphic_device->frameAdrs = sunxi_display.fb_addr;
	graphic_device->gdfIndex = GDF_32BIT_X888RGB;
	graphic_device->gdfBytesPP = 4;
	graphic_device->winSizeX = mode->xres - 2 * overscan_x;
	graphic_device->winSizeY = mode->yres - 2 * overscan_y;
	graphic_device->plnSizeX = mode->xres * graphic_device->gdfBytesPP;

	return graphic_device;
}

/*
 * Simplefb support.
 */
#if defined(CONFIG_OF_BOARD_SETUP) && defined(CONFIG_VIDEO_DT_SIMPLEFB)
int sunxi_simplefb_setup(void *blob)
{
	static GraphicDevice *graphic_device = &sunxi_display.graphic_device;
	int offset, ret;
	u64 start, size;
	const char *pipeline = NULL;

	switch (sunxi_display.monitor) {
	case sunxi_monitor_none:
		return 0;
	case sunxi_monitor_dvi:
	case sunxi_monitor_hdmi:
		pipeline = "de0-lcd0-hdmi";
		break;
	case sunxi_monitor_composite_pal:
	case sunxi_monitor_composite_ntsc:
		pipeline = "de0-lcd1-tve0";
		break;
	}

	/* Find a prefilled simpefb node, matching out pipeline config */
	offset = fdt_node_offset_by_compatible(blob, -1,
					       "allwinner,simple-framebuffer");
	while (offset >= 0) {
		ret = fdt_stringlist_search(blob, offset, "allwinner,pipeline",
					    pipeline);
		if (ret == 0)
			break;
		offset = fdt_node_offset_by_compatible(blob, offset,
					       "allwinner,simple-framebuffer");
	}
	if (offset < 0) {
		eprintf("Cannot setup simplefb: node not found\n");
		return 0; /* Keep older kernels working */
	}

	/*
	 * Do not report the framebuffer as free RAM to the OS, note we cannot
	 * use fdt_add_mem_rsv() here, because then it is still seen as RAM,
	 * and e.g. Linux refuses to iomap RAM on ARM, see:
	 * linux/arch/arm/mm/ioremap.c around line 301.
	 */
	start = gd->bd->bi_dram[0].start;
	size = gd->bd->bi_dram[0].size - sunxi_display.fb_size;
	ret = fdt_fixup_memory_banks(blob, &start, &size, 1);
	if (ret) {
		eprintf("Cannot setup simplefb: Error reserving memory\n");
		return ret;
	}

	ret = fdt_setup_simplefb_node(blob, offset, sunxi_display.fb_addr,
			graphic_device->winSizeX, graphic_device->winSizeY,
			graphic_device->plnSizeX, "x8r8g8b8");
	if (ret)
		eprintf("Cannot setup simplefb: Error setting properties\n");

	return ret;
}
#endif /* CONFIG_OF_BOARD_SETUP && CONFIG_VIDEO_DT_SIMPLEFB */
