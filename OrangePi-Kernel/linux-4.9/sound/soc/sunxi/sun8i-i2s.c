/*
 * Allwinner sun8i I2S sound card
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

/* --- hardware --- */

#define I2S_CTL 	  	0x00
	/* common */
	#define I2S_CTL_SDO3EN		BIT(11)
	#define I2S_CTL_SDO2EN		BIT(10)
	#define I2S_CTL_SDO1EN		BIT(9)
	#define I2S_CTL_SDO0EN		BIT(8)
	#define I2S_CTL_TXEN		BIT(2)
	#define I2S_CTL_RXEN		BIT(1)
	#define I2S_CTL_GEN		BIT(0)
	/* a83t */
	#define I2S_CTL_A83T_MS		BIT(5)
	#define I2S_CTL_A83T_PCM	BIT(4)
	/* h3 */
	#define I2S_CTL_H3_BCLKOUT	BIT(18)
	#define I2S_CTL_H3_LRCKOUT	BIT(17)
	#define I2S_CTL_H3_MODE_MSK	(3 << 4)
		#define I2S_CTL_H3_MODE_I2S	(1 << 4)

#define I2S_FAT0 		0x04
	/* common */
	/* a83t */
	#define I2S_FAT0_A83T_LRCP		BIT(7)
	#define I2S_FAT0_A83T_BCP		BIT(6)
	#define I2S_FAT0_A83T_SR_16BIT		(0 << 4)
	#define I2S_FAT0_A83T_SR_24BIT		(2 << 4)
	#define I2S_FAT0_A83T_SR_MSK		(3 << 4)
	#define I2S_FAT0_A83T_WSS_32BCLK	(3 << 2)
	#define I2S_FAT0_A83T_FMT_I2S1		(0 << 0)
	#define I2S_FAT0_A83T_FMT_LFT		(1 << 0)
	#define I2S_FAT0_A83T_FMT_RGT		(2 << 0)
	#define I2S_FAT0_A83T_FMT_MSK		(3 << 0)
	/* h3 */
	#define I2S_FAT0_H3_LRCKR_PERIOD(v) ((v) << 20)
	#define I2S_FAT0_H3_LRCKR_PERIOD_MSK (0x3ff << 20)
	#define I2S_FAT0_H3_LRCK_POLARITY	BIT(19)
	#define I2S_FAT0_H3_LRCK_PERIOD(v)	((v) << 8)
	#define I2S_FAT0_H3_LRCK_PERIOD_MSK (0x3ff << 8)
	#define I2S_FAT0_H3_BCLK_POLARITY	BIT(7)
	#define I2S_FAT0_H3_SR_16		(3 << 4)
	#define I2S_FAT0_H3_SR_24		(5 << 4)
	#define I2S_FAT0_H3_SR_MSK		(7 << 4)
	#define I2S_FAT0_H3_SW_16		(3 << 0)
	#define I2S_FAT0_H3_SW_32		(7 << 0)
	#define I2S_FAT0_H3_SW_MSK		(7 << 0)

#define I2S_FAT1		0x08

#define I2S_FCTL		0x14
	#define I2S_FCTL_FTX		BIT(25)
	#define I2S_FCTL_FRX		BIT(24)
	#define I2S_FCTL_TXTL(v)	((v) << 12)
	#define I2S_FCTL_TXIM		BIT(2)

#define I2S_INT    		0x1c
	#define I2S_INT_TXDRQEN		BIT(7)

#define I2S_TXFIFO		0x20
	
#define I2S_CLKD   		0x24
	/* common */
	#define I2S_CLKD_BCLKDIV(v)	((v) << 4)
	#define I2S_CLKD_MCLKDIV(v)	((v) << 0)
	/* a83t */
	#define I2S_CLKD_A83T_MCLKOEN	BIT(7)
	/* h3 */
	#define I2S_CLKD_H3_MCLKOEN	BIT(8)

#define I2S_TXCNT  		0x28

#define I2S_RXCNT  		0x2c

/* --- A83T --- */
#define I2S_TXCHSEL_A83T	0x30
	#define I2S_TXCHSEL_A83T_CHNUM(v)	(((v) - 1) << 0)
	#define I2S_TXCHSEL_A83T_CHNUM_MSK	(7 << 0)

#define I2S_TXCHMAP_A83T	0x34

/* --- H3 --- */
#define I2S_TXCHCFG_H3		0x30
	#define I2S_TXCHCFG_H3_TX_SLOT_NUM_MSK (7 << 0)
	#define I2S_TXCHCFG_H3_TX_SLOT_NUM(v) ((v) << 0)

#define I2S_TX0CHSEL_H3		0x34		/* 0..3 */
	#define I2S_TXn_H3_OFFSET_MSK	(3 << 12)
	#define I2S_TXn_H3_OFFSET(v)	((v) << 12)
	#define I2S_TXn_H3_CHEN_MSK	(0xff << 4)
	#define I2S_TXn_H3_CHEN(v)	((v) << 4)
	#define I2S_TXn_H3_CHSEL_MSK	(7 << 0)
	#define I2S_TXn_H3_CHSEL(v)	((v) << 0)

#define I2S_TX0CHMAP_H3		0x44		/* 0..3 */

/* --- driver --- */

#define DRV_NAME "sun8i-audio"

#define I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S20_3LE | \
	 SNDRV_PCM_FMTBIT_S24_LE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

#define PCM_LRCK_PERIOD 32
#define PCM_LRCKR_PERIOD 1

struct priv {
	void __iomem *mmio;
	struct clk *clk;
	struct clk *gate;
	struct reset_control *rstc;
	int type;
#define SOC_A83T 0
#define SOC_H3 1
	struct snd_dmaengine_dai_dma_data dma_data;
};

static const struct of_device_id sun8i_i2s_of_match[] = {
	{ .compatible = "allwinner,sun8i-a83t-i2s",
				.data = (void *) SOC_A83T },
	{ .compatible = "allwinner,sun8i-h3-i2s",
				.data = (void *) SOC_H3 },
	{ }
};
MODULE_DEVICE_TABLE(of, sun8i_i2s_of_match);

/* --- CPU DAI --- */

static void sun8i_i2s_init(struct priv *priv)
{
	u32 reg;

	/* disable global */
	reg = readl(priv->mmio + I2S_CTL);
	reg &= ~(I2S_CTL_GEN |
		 I2S_CTL_RXEN |
		 I2S_CTL_TXEN);
	writel(reg, priv->mmio + I2S_CTL);

	/* A83T */
	if (priv->type == SOC_A83T) {
		reg &= ~(I2S_CTL_A83T_MS |	/* codec clk & FRM slave */
			 I2S_CTL_A83T_PCM);	/* I2S mode */
		writel(reg, priv->mmio + I2S_CTL);

		reg = readl(priv->mmio + I2S_FAT0);
		reg &= ~I2S_FAT0_A83T_FMT_MSK;
		reg |= I2S_FAT0_A83T_FMT_I2S1;

		reg &= ~(I2S_FAT0_A83T_LRCP | I2S_FAT0_A83T_BCP);
		writel(reg, priv->mmio + I2S_FAT0);

		reg = I2S_FCTL_TXIM |			/* fifo */
			 I2S_FCTL_TXTL(0x40);
		writel(reg, priv->mmio + I2S_FCTL);

		reg = readl(priv->mmio + I2S_FAT0);
		reg &= ~(I2S_FAT0_A83T_LRCP |	/* normal bit clock + frame */
			 I2S_FAT0_A83T_BCP);
		writel(reg, priv->mmio + I2S_FAT0);

	/* H3 */
	} else {
		reg = readl(priv->mmio + I2S_FCTL);
		reg &= ~(I2S_FCTL_FRX | I2S_FCTL_FTX);	/* clear the FIFOs */
		writel(reg, priv->mmio + I2S_FCTL);

		writel(0, priv->mmio + I2S_TXCNT);	/* FIFO counters */
		writel(0, priv->mmio + I2S_RXCNT);

		reg = readl(priv->mmio + I2S_CTL);
		reg |= I2S_CTL_H3_LRCKOUT | I2S_CTL_H3_BCLKOUT;
						/* codec clk & FRM slave */

		reg &= ~I2S_CTL_H3_MODE_MSK;
		reg |= I2S_CTL_H3_MODE_I2S;		/* I2S mode */
		writel(reg, priv->mmio + I2S_CTL);

		reg = readl(priv->mmio + I2S_TX0CHSEL_H3) &
					~I2S_TXn_H3_OFFSET_MSK;
		reg |= I2S_TXn_H3_OFFSET(1);
		writel(reg, priv->mmio + I2S_TX0CHSEL_H3);

		reg = readl(priv->mmio + I2S_FAT0);
		reg &= ~(I2S_FAT0_H3_BCLK_POLARITY | /* normal bclk & frame */
			 I2S_FAT0_H3_LRCK_POLARITY);
		writel(reg, priv->mmio + I2S_FAT0);
	}
}

static int sun8i_i2s_set_clock(struct priv *priv,
				unsigned long rate)
{
	unsigned long freq;
	int ret, i, div;
	u32 reg;
	static const u8 div_tb[] = {
		1, 2, 4, 6, 8, 12, 16, 24,
	};

	/* compute the sys clock rate and divide values */
	if (rate % 1000 == 0)
		freq = 24576000;
	else
		freq = 22579200;
	div = freq / 2 / PCM_LRCK_PERIOD / rate;
	if (priv->type == SOC_A83T)
		div /= 2;			/* bclk_div==0 => mclk/2 */
	for (i = 0; i < ARRAY_SIZE(div_tb) - 1; i++)
		if (div_tb[i] >= div)
			break;

	ret = clk_set_rate(priv->clk, freq);
	if (ret) {
		pr_info("Setting sysclk rate failed %d\n", ret);
		return ret;
	}

	/* set the mclk and bclk dividor register */
	if (priv->type == SOC_A83T) {
		reg = I2S_CLKD_A83T_MCLKOEN | I2S_CLKD_MCLKDIV(i);
	} else {
		reg = I2S_CLKD_H3_MCLKOEN | I2S_CLKD_MCLKDIV(1) |
				I2S_CLKD_BCLKDIV(i + 1);
	}
	writel(reg, priv->mmio + I2S_CLKD);

	/* format */
	reg = readl(priv->mmio + I2S_FAT0);
	if (priv->type == SOC_A83T) {
		reg |= I2S_FAT0_A83T_WSS_32BCLK;
		reg &= ~I2S_FAT0_A83T_SR_MSK;
		reg |= I2S_FAT0_A83T_SR_16BIT;
	} else {
		reg &= ~(I2S_FAT0_H3_LRCKR_PERIOD_MSK |
			 I2S_FAT0_H3_LRCK_PERIOD_MSK);
		reg |= I2S_FAT0_H3_LRCK_PERIOD(PCM_LRCK_PERIOD - 1) |
			I2S_FAT0_H3_LRCKR_PERIOD(PCM_LRCKR_PERIOD - 1);

		reg &= ~I2S_FAT0_H3_SW_MSK;
		reg |= I2S_FAT0_H3_SW_16;

		reg &= ~I2S_FAT0_H3_SR_MSK;
		reg |= I2S_FAT0_H3_SR_16;
	}
	writel(reg, priv->mmio + I2S_FAT0);

	writel(0, priv->mmio + I2S_FAT1);

	return 0;
}

static int sun8i_i2s_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct priv *priv = snd_soc_card_get_drvdata(card);
	int nchan = substream->runtime->channels;
	u32 reg;

	if (priv->type == SOC_A83T) {
		reg = readl(priv->mmio + I2S_TXCHSEL_A83T);
		reg &= ~I2S_TXCHSEL_A83T_CHNUM_MSK;
		reg |= I2S_TXCHSEL_A83T_CHNUM(substream->runtime->channels);
		writel(reg, priv->mmio + I2S_TXCHSEL_A83T);

		switch (substream->runtime->channels) {
		case 1:
			reg = 0x76543200;
			break;
		case 8:
			reg = 0x54762310;
			break;
		default:
/* left/right inversion of channels 0 and 1 */
			reg = 0x76543201;
			break;
		}
		writel(reg, priv->mmio + I2S_TXCHMAP_A83T);
	} else {
		reg = readl(priv->mmio + I2S_TXCHCFG_H3) &
					~I2S_TXCHCFG_H3_TX_SLOT_NUM_MSK;
		if (nchan != 1)
			reg |= I2S_TXCHCFG_H3_TX_SLOT_NUM(1);
		writel(reg, priv->mmio + I2S_TXCHCFG_H3);

		reg = readl(priv->mmio + I2S_TX0CHSEL_H3);
		reg &= ~(I2S_TXn_H3_CHEN_MSK |
			 I2S_TXn_H3_CHSEL_MSK);
		reg |= I2S_TXn_H3_CHEN(3) |
			I2S_TXn_H3_CHSEL(1);
		writel(reg, priv->mmio + I2S_TX0CHSEL_H3);

		reg = nchan == 1 ? 0 : 0x10;
		writel(reg, priv->mmio + I2S_TX0CHMAP_H3);
	}

	reg = readl(priv->mmio + I2S_CTL);
	reg &= ~(I2S_CTL_SDO3EN |
		 I2S_CTL_SDO2EN |
		 I2S_CTL_SDO1EN);
	if (nchan >= 7)
		reg |= I2S_CTL_SDO3EN;
	if (nchan >= 5)
		reg |= I2S_CTL_SDO2EN;
	if (nchan >= 3)
		reg |= I2S_CTL_SDO1EN;
	reg |= I2S_CTL_SDO0EN;
	writel(reg, priv->mmio + I2S_CTL);

	writel(0, priv->mmio + I2S_TXCNT);

	return 0;
}

static void sun8i_i2s_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct priv *priv = snd_soc_card_get_drvdata(card);
	u32 reg;

	reg = readl(priv->mmio + I2S_CTL);
	reg &= ~I2S_CTL_GEN;
	writel(reg, priv->mmio + I2S_CTL);
}

static int sun8i_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct priv *priv = snd_soc_card_get_drvdata(card);
	u32 reg, reg2;
	int sample_resolution;
	int ret;

	ret = sun8i_i2s_set_clock(priv, params_rate(params));
	if (ret)
		return ret;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		priv->dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		sample_resolution = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		priv->dma_data.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		sample_resolution = 24;
		break;
	default:
		return -EINVAL;
	}
	reg = readl(priv->mmio + I2S_FAT0);
	reg2 = readl(priv->mmio + I2S_FCTL);
	if (priv->type == SOC_A83T) {
		reg &= ~I2S_FAT0_A83T_SR_MSK;
		if (sample_resolution == 16) {
			reg |= I2S_FAT0_A83T_SR_16BIT;
			reg2 |= I2S_FCTL_TXIM;
		} else {
			reg |= I2S_FAT0_A83T_SR_24BIT;
			reg2 &= ~I2S_FCTL_TXIM;
		}
	} else {
		reg &= ~(I2S_FAT0_H3_SR_MSK | I2S_FAT0_H3_SW_MSK);
		if (sample_resolution == 16) {
			reg |= I2S_FAT0_H3_SR_16 |
					I2S_FAT0_H3_SW_16;
			reg2 |= I2S_FCTL_TXIM;
		} else {
			reg |= I2S_FAT0_H3_SR_24 |
					I2S_FAT0_H3_SW_32;
			reg2 &= ~I2S_FCTL_TXIM;
		}
	}
	writel(reg, priv->mmio + I2S_FAT0);
	writel(reg2, priv->mmio + I2S_FCTL);

	/* enable audio interface */
	reg = readl(priv->mmio + I2S_CTL);
	reg |= I2S_CTL_GEN;
	writel(reg, priv->mmio + I2S_CTL);
	msleep(10);

	/* flush TX FIFO */
	reg = readl(priv->mmio + I2S_FCTL);
	reg |= I2S_FCTL_FTX;
	writel(reg, priv->mmio + I2S_FCTL);

	return 0;
}

static int sun8i_i2s_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct priv *priv = snd_soc_card_get_drvdata(card);
	u32 reg;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		reg = readl(priv->mmio + I2S_CTL);
		reg |= I2S_CTL_TXEN;
		writel(reg, priv->mmio + I2S_CTL);

		/* enable DMA DRQ mode */
		reg = readl(priv->mmio + I2S_INT);
		reg |= I2S_INT_TXDRQEN;
		writel(reg, priv->mmio + I2S_INT);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		reg = readl(priv->mmio + I2S_INT);
		reg &= ~I2S_INT_TXDRQEN;
		writel(reg, priv->mmio + I2S_INT);

		reg = readl(priv->mmio + I2S_CTL);
		reg &= ~I2S_CTL_TXEN;
		writel(reg, priv->mmio + I2S_CTL);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops sun8i_i2s_dai_ops = {
	.hw_params = sun8i_i2s_hw_params,
	.prepare = sun8i_i2s_prepare,
	.trigger = sun8i_i2s_trigger,
	.shutdown = sun8i_i2s_shutdown,
};

static int sun8i_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct snd_soc_card *card = snd_soc_dai_get_drvdata(dai);
	struct priv *priv = snd_soc_card_get_drvdata(card);

	snd_soc_dai_init_dma_data(dai, &priv->dma_data, NULL);

	return 0;
}

static struct snd_soc_dai_driver sun8i_i2s_dai = {
	.probe = sun8i_i2s_dai_probe,
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min = 32000,
		.rate_max = 192000,
		.formats = I2S_FORMATS,
	},
	.ops = &sun8i_i2s_dai_ops,
};

static const struct snd_soc_component_driver i2s_component = {
	.name = DRV_NAME,
};

/* --- dma --- */

static const struct snd_pcm_hardware sun8i_i2s_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats = I2S_FORMATS,
	.rates = SNDRV_PCM_RATE_CONTINUOUS,
	.rate_min = 32000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 8,
	.buffer_bytes_max = 1024 * 1024,
	.period_bytes_min = 156,
	.period_bytes_max = 1024 * 1024,
	.periods_min = 1,
	.periods_max = 8,
	.fifo_size = 128,
};

static const struct snd_dmaengine_pcm_config sun8i_i2s_config = {
	.prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config,
	.pcm_hardware = &sun8i_i2s_pcm_hardware,
	.prealloc_buffer_size = 1024 * 1024,
};

/* --- audio card --- */

static struct device_node *sun8i_get_codec(struct device *dev)
{
	struct device_node *ep, *remote;

	ep = of_graph_get_next_endpoint(dev->of_node, NULL);
	if (!ep)
		return NULL;
	remote = of_graph_get_remote_port_parent(ep);
	of_node_put(ep);

	return remote;
}

static int sun8i_card_create(struct device *dev, struct priv *priv)
{
	struct snd_soc_card *card;
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_dai_link_component *codec;

	card = devm_kzalloc(dev, sizeof(*card), GFP_KERNEL);
	if (!card)
		return -ENOMEM;
	dai_link = devm_kzalloc(dev, sizeof(*dai_link), GFP_KERNEL);
	if (!dai_link)
		return -ENOMEM;
	codec = devm_kzalloc(dev, sizeof(*codec), GFP_KERNEL);
	if (!codec)
		return -ENOMEM;

	codec->of_node = sun8i_get_codec(dev);
	if (!codec->of_node) {
		dev_err(dev, "no port node\n");
		return -ENXIO;
	}

	card->name = codec->of_node->name;
	card->dai_link = dai_link;
	card->num_links = 1;
	dai_link->name = codec->of_node->name;
	dai_link->stream_name = codec->of_node->name;
	dai_link->platform_name = dev_name(dev);
	dai_link->cpu_name = dev_name(dev);

	dai_link->codecs = codec;
	dai_link->num_codecs = 1;

	/* the DAI name must be the name of codec node */
	codec->dai_name = codec->of_node->name;

	card->dev = dev;
	dev_set_drvdata(dev, card);
	snd_soc_card_set_drvdata(card, priv);

	return devm_snd_soc_register_card(dev, card);
}

/* --- module init --- */

static int sun8i_i2s_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct priv *priv;
	struct resource *mem;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "no DT!\n");
		return -EINVAL;
	}
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* get the resources */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->mmio = devm_ioremap_resource(dev, mem);
	if (IS_ERR(priv->mmio))
		return PTR_ERR(priv->mmio);

	/* get SoC type */
	priv->type = (int) of_match_device(sun8i_i2s_of_match,
						&pdev->dev)->data;

	/* get and enable the clocks */
	priv->gate = devm_clk_get(dev, "apb");	/* optional */
	priv->clk = devm_clk_get(dev, "mod");
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "no clock\n");
		return PTR_ERR(priv->clk);
	}
	ret = clk_set_rate(priv->clk, 24576000);
	if (ret) {
		dev_err(dev, "cannot set rate of i2s clock %d\n", ret);
		return ret;
	}

	priv->rstc = devm_reset_control_get_optional(dev, NULL);
	if (!IS_ERR(priv->rstc)) {
		ret = reset_control_deassert(priv->rstc);
		if (ret < 0)
			return ret;
	}

	if (!IS_ERR(priv->gate)) {
		ret = clk_prepare_enable(priv->gate);
		if (ret < 0)
			goto err_gate;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret < 0)
		goto err_enable;

	/* activate the audio subsystem */
	sun8i_i2s_init(priv);

	ret = devm_snd_soc_register_component(dev, &i2s_component,
						&sun8i_i2s_dai, 1);
	if (ret) {
		dev_err(dev, "snd_soc_register_component failed %d\n", ret);
		goto err_register;
	}

	ret = devm_snd_dmaengine_pcm_register(dev, &sun8i_i2s_config, 0);
	if (ret) {
		dev_err(dev, "pcm_register failed %d\n", ret);
		goto err_register;
	}

	priv->dma_data.maxburst = priv->type == SOC_A83T ? 8 : 4;
	priv->dma_data.addr = mem->start + I2S_TXFIFO;
	priv->dma_data.addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;

	ret = sun8i_card_create(dev, priv);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "register card failed %d\n", ret);
		goto err_register;
	}

	return 0;

err_register:
	clk_disable_unprepare(priv->clk);
err_enable:
	clk_disable_unprepare(priv->gate);
err_gate:
	if (!IS_ERR(priv->rstc))
		reset_control_assert(priv->rstc);

	return ret;
}

static int sun8i_i2s_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = dev_get_drvdata(&pdev->dev);
	struct priv *priv = snd_soc_card_get_drvdata(card); 

	clk_disable_unprepare(priv->clk);
	clk_disable_unprepare(priv->gate);
	if (!IS_ERR_OR_NULL(priv->rstc))
		reset_control_assert(priv->rstc);

	return 0;
}

static struct platform_driver sun8i_i2s_driver = {
	.probe  = sun8i_i2s_dev_probe,
	.remove = sun8i_i2s_dev_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = of_match_ptr(sun8i_i2s_of_match),
	},
};

module_platform_driver(sun8i_i2s_driver);

MODULE_AUTHOR("Jean-Francois Moine <moinejf@free.fr>");
MODULE_DESCRIPTION("Allwinner sun8i I2S ASoC Interface");
MODULE_LICENSE("GPL v2");
