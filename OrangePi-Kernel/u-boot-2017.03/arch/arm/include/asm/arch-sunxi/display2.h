/*
 * Sunxi platform display controller register and constant defines
 *
 * (C) Copyright 2016 Jernej Skrabec <jernej.skrabec@siol.net>
 *
 * Based on Linux DRM driver defines:
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Copyright (c) 2016 Allwinnertech Co., Ltd.
 *
 * Based on display.h:
 * (C) Copyright 2014 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _SUNXI_DISPLAY2_H
#define _SUNXI_DISPLAY2_H

struct sunxi_lcdc_reg {
	u32 ctrl;			/* 0x00 */
	u32 int0;			/* 0x04 */
	u32 int1;			/* 0x08 */
	u8 res0[0x04];			/* 0x0c */
	u32 tcon0_frm_ctrl;		/* 0x10 */
	u32 tcon0_frm_seed[6];		/* 0x14 */
	u32 tcon0_frm_table[4];		/* 0x2c */
	u8 res1[4];			/* 0x3c */
	u32 tcon0_ctrl;			/* 0x40 */
	u32 tcon0_dclk;			/* 0x44 */
	u32 tcon0_timing_active;	/* 0x48 */
	u32 tcon0_timing_h;		/* 0x4c */
	u32 tcon0_timing_v;		/* 0x50 */
	u32 tcon0_timing_sync;		/* 0x54 */
	u32 tcon0_hv_intf;		/* 0x58 */
	u8 res2[0x04];			/* 0x5c */
	u32 tcon0_cpu_intf;		/* 0x60 */
	u32 tcon0_cpu_wr_dat;		/* 0x64 */
	u32 tcon0_cpu_rd_dat0;		/* 0x68 */
	u32 tcon0_cpu_rd_dat1;		/* 0x6c */
	u32 tcon0_ttl_timing0;		/* 0x70 */
	u32 tcon0_ttl_timing1;		/* 0x74 */
	u32 tcon0_ttl_timing2;		/* 0x78 */
	u32 tcon0_ttl_timing3;		/* 0x7c */
	u32 tcon0_ttl_timing4;		/* 0x80 */
	u32 tcon0_lvds_intf;		/* 0x84 */
	u32 tcon0_io_polarity;		/* 0x88 */
	u32 tcon0_io_tristate;		/* 0x8c */
	u32 tcon1_ctrl;			/* 0x90 */
	u32 tcon1_timing_source;	/* 0x94 */
	u32 tcon1_timing_scale;		/* 0x98 */
	u32 tcon1_timing_out;		/* 0x9c */
	u32 tcon1_timing_h;		/* 0xa0 */
	u32 tcon1_timing_v;		/* 0xa4 */
	u32 tcon1_timing_sync;		/* 0xa8 */
	u8 res3[0x44];			/* 0xac */
	u32 tcon1_io_polarity;		/* 0xf0 */
	u32 tcon1_io_tristate;		/* 0xf4 */
	u8 res4[0x108];			/* 0xf8 */
	u32 mux_ctrl;			/* 0x200 */
};

/* internal clock settings */
struct de_clk {
	u32 gate_cfg;
	u32 bus_cfg;
	u32 rst_cfg;
	u32 div_cfg;
	u32 sel_cfg;
};

/* global control */
struct de_glb {
	u32 ctl;
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
		u32 size;
		u32 coord;
		u32 pitch[3];
		u32 top_laddr[3];
		u32 bot_laddr[3];
	} cfg[4];
	u32 fcolor[4];			/* c0 */
	u32 top_haddr[3];		/* d0 */
	u32 bot_haddr[3];		/* dc */
	u32 ovl_size[2];		/* e8 */
	u32 hori[2];			/* f0 */
	u32 vert[2];			/* f8 */
};

struct de_ui {
	struct {
		u32 attr;
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

struct de_csc {
	u32 csc_ctl;
	u32 rsv[3];
	u32 coef11;
	u32 coef12;
	u32 coef13;
	u32 coef14;
	u32 coef21;
	u32 coef22;
	u32 coef23;
	u32 coef24;
	u32 coef31;
	u32 coef32;
	u32 coef33;
	u32 coef34;
	u32 alpha;
};

struct sunxi_dwc_hdmi {
	u8 reserved0[0x100];
	u8 ih_fc_stat0;
	u8 ih_fc_stat1;
	u8 ih_fc_stat2;
	u8 ih_as_stat0;
	u8 ih_phy_stat0;
	u8 ih_i2cm_stat0;
	u8 ih_cec_stat0;
	u8 ih_vp_stat0;
	u8 ih_i2cmphy_stat0;
	u8 ih_ahbdmaaud_stat0;
	u8 reserved1[0x17f-0x109];
	u8 ih_mute_fc_stat0;
	u8 ih_mute_fc_stat1;
	u8 ih_mute_fc_stat2;
	u8 ih_mute_as_stat0;
	u8 ih_mute_phy_stat0;
	u8 ih_mute_i2cm_stat0;
	u8 ih_mute_cec_stat0;
	u8 ih_mute_vp_stat0;
	u8 ih_mute_i2cmphy_stat0;
	u8 ih_mute_ahbdmaaud_stat0;
	u8 reserved2[0x1fe - 0x189];
	u8 ih_mute;
	u8 tx_invid0;
	u8 tx_instuffing;
	u8 tx_gydata0;
	u8 tx_gydata1;
	u8 tx_rcrdata0;
	u8 tx_rcrdata1;
	u8 tx_bcbdata0;
	u8 tx_bcbdata1;
	u8 reserved3[0x7ff-0x207];
	u8 vp_status;
	u8 vp_pr_cd;
	u8 vp_stuff;
	u8 vp_remap;
	u8 vp_conf;
	u8 vp_stat;
	u8 vp_int;
	u8 vp_mask;
	u8 vp_pol;
	u8 reserved4[0xfff-0x808];
	u8 fc_invidconf;
	u8 fc_inhactv0;
	u8 fc_inhactv1;
	u8 fc_inhblank0;
	u8 fc_inhblank1;
	u8 fc_invactv0;
	u8 fc_invactv1;
	u8 fc_invblank;
	u8 fc_hsyncindelay0;
	u8 fc_hsyncindelay1;
	u8 fc_hsyncinwidth0;
	u8 fc_hsyncinwidth1;
	u8 fc_vsyncindelay;
	u8 fc_vsyncinwidth;
	u8 fc_infreq0;
	u8 fc_infreq1;
	u8 fc_infreq2;
	u8 fc_ctrldur;
	u8 fc_exctrldur;
	u8 fc_exctrlspac;
	u8 fc_ch0pream;
	u8 fc_ch1pream;
	u8 fc_ch2pream;
	u8 fc_aviconf3;
	u8 fc_gcp;
	u8 fc_aviconf0;
	u8 fc_aviconf1;
	u8 fc_aviconf2;
	u8 fc_avivid;
	u8 fc_avietb0;
	u8 fc_avietb1;
	u8 fc_avisbb0;
	u8 fc_avisbb1;
	u8 fc_avielb0;
	u8 fc_avielb1;
	u8 fc_avisrb0;
	u8 fc_avisrb1;
	u8 fc_audiconf0;
	u8 fc_audiconf1;
	u8 fc_audiconf2;
	u8 fc_audiconf3;
	u8 fc_vsdieeeid0;
	u8 fc_vsdsize;
	u8 reserved5[0x30ff-0x102a];
	u8 aud_conf0;
	u8 aud_conf1;
	u8 aud_int;
	u8 aud_conf2;
	u8 aud_int1;
	u8 reserved6[0x31ff-0x3104];
	u8 aud_n1;
	u8 aud_n2;
	u8 aud_n3;
	u8 aud_cts1;
	u8 aud_cts2;
	u8 aud_cts3;
	u8 aud_inputclkfs;
	u8 reserved7[0x3fff-0x3206];
	u8 mc_sfrdiv;
	u8 mc_clkdis;
	u8 mc_swrstz;
	u8 mc_opctrl;
	u8 mc_flowctrl;
	u8 mc_phyrstz;
	u8 mc_lockonclock;
	u8 mc_heacphy_rst;
	u8 reserved8[0x40ff-0x4007];
	u8 csc_cfg;
	u8 csc_scale;
	struct {
		u8 msb;
		u8 lsb;
	} csc_coef[3][4];
	u8 reserved9[0x7dff-0x4119];
	u8 i2cm_slave;
	u8 i2c_address;
	u8 i2cm_datao;
	u8 i2cm_datai;
	u8 i2cm_operation;
	u8 i2cm_int;
	u8 i2cm_ctlint;
	u8 i2cm_div;
	u8 i2cm_segaddr;
	u8 i2cm_softrstz;
	u8 i2cm_segptr;
	u8 i2cm_ss_scl_hcnt_1_addr;
	u8 i2cm_ss_scl_hcnt_0_addr;
	u8 i2cm_ss_scl_lcnt_1_addr;
	u8 i2cm_ss_scl_lcnt_0_addr;
	u8 i2cm_fs_scl_hcnt_1_addr;
	u8 i2cm_fs_scl_hcnt_0_addr;
	u8 i2cm_fs_scl_lcnt_1_addr;
	u8 i2cm_fs_scl_lcnt_0_addr;
	u8 reserved10[0xffff-0x7e12];
	u32 phy_pol;
	u32 phy_reserved11[3];
	u32 phy_read_en;
	u32 phy_unscramble;
	u32 reserved12[2];
	u32 phy_ctrl;
	u32 phy_unk1;
	u32 phy_unk2;
	u32 phy_pll;
	u32 phy_clk;
	u32 phy_unk3;
	u32 phy_status;
};

struct sunxi_tve_reg {
	u32 gctrl;			/* 0x000 */
	u32 cfg0;			/* 0x004 */
	u32 dac_cfg0;			/* 0x008 */
	u32 filter;			/* 0x00c */
	u32 chroma_freq;		/* 0x010 */
	u32 porch_num;			/* 0x014 */
	u32 unknown0;			/* 0x018 */
	u32 line_num;			/* 0x01c */
	u32 blank_black_level;		/* 0x020 */
	u32 unknown1;			/* 0x024, seems to be 1 byte per dac */
	u8 res0[8];			/* 0x028 */
	u32 auto_detect_en;		/* 0x030 */
	u32 auto_detect_int_status;	/* 0x034 */
	u32 auto_detect_status;		/* 0x038 */
	u32 auto_detect_debounce;	/* 0x03c */
	u32 csc_reg0;			/* 0x040 */
	u32 csc_reg1;			/* 0x044 */
	u32 csc_reg2;			/* 0x048 */
	u32 csc_reg3;			/* 0x04c */
	u8 res1[0xb0];			/* 0x050 */
	u32 color_burst;		/* 0x100 */
	u32 vsync_num;			/* 0x104 */
	u32 notch_freq;			/* 0x108 */
	u32 cbr_level;			/* 0x10c */
	u32 burst_phase;		/* 0x110 */
	u32 burst_width;		/* 0x114 */
	u32 unknown2;			/* 0x118 */
	u32 sync_vbi_level;		/* 0x11c */
	u32 white_level;		/* 0x120 */
	u32 active_num;			/* 0x124 */
	u32 chroma_bw_gain;		/* 0x128 */
	u32 notch_width;		/* 0x12c */
	u32 resync_num;			/* 0x130 */
	u32 slave_para;			/* 0x134 */
	u32 cfg1;			/* 0x138 */
	u32 cfg2;			/* 0x13c */
};

/*
 * DE register constants.
 */
#define SUNXI_DE2_MUX0_BASE			(u8 *)(SUNXI_DE2_BASE + 0x100000)
#define SUNXI_DE2_MUX1_BASE			(u8 *)(SUNXI_DE2_BASE + 0x200000)

#define SUNXI_DE2_MUX_GLB_REGS			0x00000
#define SUNXI_DE2_MUX_BLD_REGS			0x01000
#define SUNXI_DE2_MUX_CHAN_REGS			0x02000
#define	SUNXI_DE2_MUX_CHAN_SZ			0x1000
#define SUNXI_DE2_MUX_VSU_REGS			0x20000
#define SUNXI_DE2_MUX_GSU1_REGS			0x30000
#define SUNXI_DE2_MUX_GSU2_REGS			0x40000
#define SUNXI_DE2_MUX_GSU3_REGS			0x50000
#define SUNXI_DE2_MUX_FCE_REGS			0xa0000
#define SUNXI_DE2_MUX_BWS_REGS			0xa2000
#define SUNXI_DE2_MUX_LTI_REGS			0xa4000
#define SUNXI_DE2_MUX_PEAK_REGS			0xa6000
#define SUNXI_DE2_MUX_ASE_REGS			0xa8000
#define SUNXI_DE2_MUX_FCC_REGS			0xaa000
#define SUNXI_DE2_MUX_DCSC_REGS			0xb0000

#define SUNXI_DE2_FORMAT_ARGB_8888		0
#define SUNXI_DE2_FORMAT_BGRA_8888		3
#define SUNXI_DE2_FORMAT_XRGB_8888		4
#define SUNXI_DE2_FORMAT_RGB_888		8
#define SUNXI_DE2_FORMAT_BGR_888		9

#define SUNXI_DE2_MUX_GLB_CTL_RT_EN		(1 << 0)

#define SUNXI_DE2_UI_CFG_ATTR_EN		(1 << 0)
#define SUNXI_DE2_UI_CFG_ATTR_ALPMOD(m)		((m & 3) << 1)
#define SUNXI_DE2_UI_CFG_ATTR_FMT(f)		((f & 0xf) << 8)
#define SUNXI_DE2_UI_CFG_ATTR_ALPHA(a)		((a & 0xff) << 24)

#define SUNXI_DE2_WH(w, h)			(((h - 1) << 16) | (w - 1))

/*
 * LCDC register constants.
 */
#define SUNXI_LCDC_X(x)				(((x) - 1) << 16)
#define SUNXI_LCDC_Y(y)				(((y) - 1) << 0)
#define SUNXI_LCDC_CTRL_TCON_ENABLE		(1 << 31)
#define SUNXI_LCDC_TCON0_CTRL_ENABLE		(1 << 31)
#define SUNXI_LCDC_TCON1_CTRL_CLK_DELAY(n)	(((n) & 0x1f) << 4)
#define SUNXI_LCDC_TCON1_CTRL_INTERLACE_ENABLE	(1 << 20)
#define SUNXI_LCDC_TCON1_CTRL_ENABLE		(1 << 31)
#define SUNXI_LCDC_TCON1_TIMING_H_BP(n)		(((n) - 1) << 0)
#define SUNXI_LCDC_TCON1_TIMING_H_TOTAL(n)	(((n) - 1) << 16)
#define SUNXI_LCDC_TCON1_TIMING_V_BP(n)		(((n) - 1) << 0)
#define SUNXI_LCDC_TCON1_TIMING_V_TOTAL(n)	((n) << 16)

/*
 * HDMI register constants.
 */
#define HDMI_IH_MUTE_MUTE_WAKEUP_INTERRUPT	0x02
#define HDMI_IH_MUTE_MUTE_ALL_INTERRUPT		0x01

#define HDMI_TX_INSTUFFING_BDBDATA_STUFFING_EN	0x04
#define HDMI_TX_INSTUFFING_RCRDATA_STUFFING_EN	0x02
#define HDMI_TX_INSTUFFING_GYDATA_STUFFING_EN	0x01

#define HDMI_FC_INVIDCONF_DVI_MODE_HDMI		0x08
#define HDMI_FC_INVIDCONF_DE_IN_POL_ACTIVE_HIGH 0x10

#define HDMI_FC_AVICONF0_ACTIVE_FORMAT		0x40
#define HDMI_FC_AVICONF0_SCAN_INFO_UNDERSCAN	0x20

#define HDMI_FC_AVICONF2_RGB_QUANT_FULL_RANGE	0x08
#define HDMI_FC_AVICONF2_IT_CONTENT_VALID	0x80

#define HDMI_MC_CLKDIS_TMDSCLK_DISABLE		0x02

#define HDMI_MC_FLOWCTRL_CSC_BYPASS		0x00

#define HDMI_I2CM_CTLINT_ADDR_NACK_POL		0x80
#define HDMI_I2CM_CTLINT_ADDR_NACK_MSK		0x40
#define HDMI_I2CM_CTLINT_ADDR_ARB_POL		0x08
#define HDMI_I2CM_CTLINT_ADDR_ARB_MSK		0x04

#define HMDI_DDC_CTRL_RESET			(1 << 0)
#define HMDI_DDC_ADDR_SLAVE_ADDR		(0x50 << 0)
#define HMDI_DDC_ADDR_SEG_ADDR			(0x30 << 0)

#define SUNXI_HDMI_HPD_DETECT			(1 << 19)

/*
 * TVE register constants.
 */
#define SUNXI_TVE_GCTRL_ENABLE			(1 << 0)
/*
 * Select input 0 to disable dac, 1 - 4 to feed dac from tve0, 5 - 8 to feed
 * dac from tve1. When using tve1 the mux value must be written to both tve0's
 * and tve1's gctrl reg.
 */
#define SUNXI_TVE_GCTRL_DAC_INPUT_MASK(dac)	(0xf << (((dac) + 1) * 4))
#define SUNXI_TVE_GCTRL_DAC_INPUT(dac, sel)	((sel) << (((dac) + 1) * 4))

#define SUNXI_TVE_CFG0_PAL			0x07070001
#define SUNXI_TVE_CFG0_NTSC			0x07070000
#define SUNXI_TVE_DAC_CFG0_COMPOSITE		0x433f0009
#define SUNXI_TVE_FILTER_COMPOSITE		0x00000120
#define SUNXI_TVE_CHROMA_FREQ_PAL_M		0x21e6efe3
#define SUNXI_TVE_CHROMA_FREQ_PAL_NC		0x21f69446
#define SUNXI_TVE_PORCH_NUM_PAL			0x008a0018
#define SUNXI_TVE_PORCH_NUM_NTSC		0x00760020
#define SUNXI_TVE_LINE_NUM_PAL			0x00160271
#define SUNXI_TVE_LINE_NUM_NTSC			0x0016020d
#define SUNXI_TVE_BLANK_BLACK_LEVEL_PAL		0x00fc00fc
#define SUNXI_TVE_BLANK_BLACK_LEVEL_NTSC	0x00f0011a
#define SUNXI_TVE_UNKNOWN1_VGA			0x00000000
#define SUNXI_TVE_UNKNOWN1_COMPOSITE		0x18181818
#define SUNXI_TVE_AUTO_DETECT_EN_DET_EN(dac)	(1 << ((dac) + 0))
#define SUNXI_TVE_AUTO_DETECT_EN_INT_EN(dac)	(1 << ((dac) + 16))
#define SUNXI_TVE_AUTO_DETECT_INT_STATUS(dac)	(1 << ((dac) + 0))
#define SUNXI_TVE_AUTO_DETECT_STATUS_SHIFT(dac)	((dac) * 8)
#define SUNXI_TVE_AUTO_DETECT_STATUS_MASK(dac)	(3 << ((dac) * 8))
#define SUNXI_TVE_AUTO_DETECT_STATUS_NONE	0
#define SUNXI_TVE_AUTO_DETECT_STATUS_CONNECTED	1
#define SUNXI_TVE_AUTO_DETECT_STATUS_SHORT_GND	3
#define SUNXI_TVE_AUTO_DETECT_DEBOUNCE_SHIFT(d)	((d) * 8)
#define SUNXI_TVE_AUTO_DETECT_DEBOUNCE_MASK(d)	(0xf << ((d) * 8))
#define SUNXI_TVE_CSC_REG0_ENABLE		(1 << 31)
#define SUNXI_TVE_CSC_REG0			0x08440832
#define SUNXI_TVE_CSC_REG1			0x3b6dace1
#define SUNXI_TVE_CSC_REG2			0x0e1d13dc
#define SUNXI_TVE_CSC_REG3			0x00108080
#define SUNXI_TVE_COLOR_BURST_PAL_M		0x00000000
#define SUNXI_TVE_CBR_LEVEL_PAL			0x00002828
#define SUNXI_TVE_CBR_LEVEL_NTSC		0x0000004f
#define SUNXI_TVE_BURST_PHASE_NTSC		0x00000000
#define SUNXI_TVE_BURST_WIDTH_COMPOSITE		0x0016447e
#define SUNXI_TVE_UNKNOWN2_PAL			0x0000e0e0
#define SUNXI_TVE_UNKNOWN2_NTSC			0x0000a0a0
#define SUNXI_TVE_SYNC_VBI_LEVEL_NTSC		0x001000f0
#define SUNXI_TVE_ACTIVE_NUM_COMPOSITE		0x000005a0
#define SUNXI_TVE_CHROMA_BW_GAIN_COMP		0x00000002
#define SUNXI_TVE_NOTCH_WIDTH_COMPOSITE		0x00000101
#define SUNXI_TVE_RESYNC_NUM_PAL		0x800d000c
#define SUNXI_TVE_RESYNC_NUM_NTSC		0x000e000c
#define SUNXI_TVE_SLAVE_PARA_COMPOSITE		0x00000000

#endif /* _SUNXI_DISPLAY_H */
