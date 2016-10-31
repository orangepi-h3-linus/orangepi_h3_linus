/*
 * Allwinner DRM driver - HDMI
 *
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/component.h>
#include <linux/clk.h>
#include <linux/hdmi.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>

#include "de2_hdmi.h"
#include "de2_hdmi_io.h"

static const struct of_device_id de2_hdmi_dt_ids[] = {
	{ .compatible = "allwinner,sun8i-a83t-hdmi",
					.data = (void *) SOC_A83T },
	{ .compatible = "allwinner,sun8i-h3-hdmi",
					.data = (void *) SOC_H3 },
	{ }
};
MODULE_DEVICE_TABLE(of, de2_hdmi_dt_ids);

#define conn_to_priv(x) \
	container_of(x, struct de2_hdmi_priv, connector)

#define enc_to_priv(x) \
	container_of(x, struct de2_hdmi_priv, encoder)

#define codec_to_priv(x) \
	container_of(x, struct de2_hdmi_priv, codec)

#if IS_ENABLED(CONFIG_SND_SOC_SUNXI_HDMI)
/* --- audio functions --- */
static int de2_hdmi_set_audio_input(struct device *dev,
				    int enable,
				    unsigned sample_rate,
				    unsigned sample_bit)
{
	struct de2_hdmi_priv *priv = codec_to_priv(dev_get_drvdata(dev));
	int ret;

	if (!enable)
		return 0;

	mutex_lock(&priv->mutex);
	ret = hdmi_io_audio(priv, sample_rate, sample_bit);
	mutex_unlock(&priv->mutex);

	return ret;
}
#endif

/* --- encoder functions --- */

static void de2_hdmi_encoder_mode_set(struct drm_encoder *encoder,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	struct de2_hdmi_priv *priv = enc_to_priv(encoder);

	priv->cea_mode = drm_match_cea_mode(mode);

	DRM_DEBUG_DRIVER("cea_mode %d\n", priv->cea_mode);

	clk_set_rate(priv->clk, mode->clock * 1000);

	mutex_lock(&priv->mutex);
	hdmi_io_video_mode(priv);
	mutex_unlock(&priv->mutex);
}

static void de2_hdmi_encoder_enable(struct drm_encoder *encoder)
{ 
	struct de2_hdmi_priv *priv = enc_to_priv(encoder);

	mutex_lock(&priv->mutex);
	hdmi_io_video_on(priv);
	mutex_unlock(&priv->mutex);
}

static void de2_hdmi_encoder_disable(struct drm_encoder *encoder)
{ 
	struct de2_hdmi_priv *priv = enc_to_priv(encoder);

	mutex_lock(&priv->mutex);
	hdmi_io_video_off(priv);
	mutex_unlock(&priv->mutex);
}

static const struct drm_encoder_helper_funcs de2_hdmi_encoder_helper_funcs = {
	.mode_set = de2_hdmi_encoder_mode_set,
	.enable = de2_hdmi_encoder_enable,
	.disable = de2_hdmi_encoder_disable,
};

static const struct drm_encoder_funcs de2_hdmi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

/* --- connector functions --- */
static int de2_hdmi_connector_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode)
{
	struct de2_hdmi_priv *priv = conn_to_priv(connector);
	int cea_mode = drm_match_cea_mode(mode);

	if (hdmi_io_mode_valid(cea_mode) < 0)
		return MODE_NOMODE;

	if (connector->eld[0])
		priv->codec.eld = connector->eld;	/* audio parameters */

	return MODE_OK;
}

static enum drm_connector_status de2_hdmi_connector_detect(
				struct drm_connector *connector, bool force)
{
	struct de2_hdmi_priv *priv = conn_to_priv(connector);
	int ret;

	mutex_lock(&priv->mutex);
	ret = hdmi_io_get_hpd(priv);
	mutex_unlock(&priv->mutex);

	return ret ? connector_status_connected :
			connector_status_disconnected;
}

static int read_edid_block(void *data, u8 *buf,
			   unsigned int blk, size_t length)
{
	struct de2_hdmi_priv *priv = data;
	int ret;

	mutex_lock(&priv->mutex);
	ret = hdmi_io_ddc_read(priv,
				blk / 2, (blk & 1) ? 128 : 0,
				length, buf);
	mutex_unlock(&priv->mutex);

	return ret;
}

static int de2_hdmi_connector_get_modes(struct drm_connector *connector)
{
	struct de2_hdmi_priv *priv = conn_to_priv(connector);
	struct edid *edid;
	int n;

	DRM_DEBUG_DRIVER("\n");

	priv->codec.eld = NULL;

	edid = drm_do_get_edid(connector, read_edid_block, priv);

	if (!edid) {
		dev_warn(priv->dev, "failed to read EDID\n");
		if (!connector->cmdline_mode.specified)
			return 0;

		return drm_add_modes_noedid(connector,
					connector->cmdline_mode.xres,
					connector->cmdline_mode.yres);
	}

	drm_mode_connector_update_edid_property(connector, edid);
	n = drm_add_edid_modes(connector, edid);

	drm_edid_to_eld(connector, edid);

	kfree(edid);

	DRM_DEBUG_DRIVER("%s EDID ok %d modes\n",
		connector->eld[0] ? "HDMI" : "DVI", n);

	return n;
}

static const
struct drm_connector_helper_funcs de2_hdmi_connector_helper_funcs = {
	.get_modes = de2_hdmi_connector_get_modes,
	.mode_valid = de2_hdmi_connector_mode_valid,
};

static const struct drm_connector_funcs de2_hdmi_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.reset = drm_atomic_helper_connector_reset,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = de2_hdmi_connector_detect,
	.destroy = drm_connector_cleanup,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static void de2_hdmi_cleanup(struct de2_hdmi_priv *priv)
{
	clk_disable_unprepare(priv->clk_ddc);
	clk_disable_unprepare(priv->clk);
	clk_disable_unprepare(priv->gate);
	if (!IS_ERR_OR_NULL(priv->rstc1))
		reset_control_assert(priv->rstc1);
	if (!IS_ERR_OR_NULL(priv->rstc0))
		reset_control_assert(priv->rstc0);
}

static int de2_hdmi_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct de2_hdmi_priv *priv = codec_to_priv(dev_get_drvdata(dev));
	struct drm_encoder *encoder = &priv->encoder;
	struct drm_connector *connector = &priv->connector;
	int ret;

	encoder->possible_crtcs =
			drm_of_find_possible_crtcs(drm, dev->of_node);

	/* if no CRTC, delay */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	/* HDMI init */
	ret = reset_control_deassert(priv->rstc0);
	if (ret)
		goto err;
	ret = reset_control_deassert(priv->rstc1);
	if (ret)
		goto err;
	if (!IS_ERR(priv->gate)) {
		ret = clk_prepare_enable(priv->gate);
		if (ret)
			goto err;
	}
	ret = clk_prepare_enable(priv->clk);
	if (ret)
		goto err;
	ret = clk_prepare_enable(priv->clk_ddc);
	if (ret)
		goto err;

	mutex_lock(&priv->mutex);
	hdmi_io_init(priv);
	hdmi_io_reset(priv);		/* hpd reset */
	mutex_unlock(&priv->mutex);

	/* encoder init */
	ret = drm_encoder_init(drm, encoder, &de2_hdmi_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, NULL);
	if (ret)
		goto err;

	drm_encoder_helper_add(encoder, &de2_hdmi_encoder_helper_funcs);

	/* connector init */
	ret = drm_connector_init(drm, connector,
				 &de2_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		goto err_connector;

	connector->interlace_allowed = 1;
	connector->polled = DRM_CONNECTOR_POLL_CONNECT |
				 DRM_CONNECTOR_POLL_DISCONNECT;
	drm_connector_helper_add(connector,
				 &de2_hdmi_connector_helper_funcs);

	drm_mode_connector_attach_encoder(connector, encoder);

	return 0;

err_connector:
	drm_encoder_cleanup(encoder);
err:
	de2_hdmi_cleanup(priv);
	DRM_DEBUG_DRIVER("err %d\n", ret);
	return ret;
}

static void de2_hdmi_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct de2_hdmi_priv *priv = codec_to_priv(dev_get_drvdata(dev));

	drm_connector_cleanup(&priv->connector);
	drm_encoder_cleanup(&priv->encoder);
	de2_hdmi_cleanup(priv);
}

static const struct component_ops de2_hdmi_ops = {
	.bind = de2_hdmi_bind,
	.unbind = de2_hdmi_unbind,
};

static int de2_hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct de2_hdmi_priv *priv;
	struct resource *res;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, &priv->codec);
	priv->dev = dev;

	mutex_init(&priv->mutex);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get memory resource\n");
		return -ENXIO;
	}
	priv->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(priv->mmio)) {
		dev_err(dev, "failed to map registers\n");
		return PTR_ERR(priv->mmio);
	}

	priv->gate = devm_clk_get(dev, "gate");	/* optional */

	priv->clk = devm_clk_get(dev, "clock");
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "hdmi clock err %d\n",
					(int) PTR_ERR(priv->clk));
		return PTR_ERR(priv->clk);
	}
	priv->clk_ddc = devm_clk_get(dev, "ddc-clock");
	if (IS_ERR(priv->clk_ddc)) {
		dev_err(dev, "hdmi-ddc clock err %d\n",
					(int) PTR_ERR(priv->clk_ddc));
		return PTR_ERR(priv->clk_ddc);
	}

	priv->rstc0 = devm_reset_control_get(dev, "hdmi0");
	if (IS_ERR(priv->rstc0)) {
		dev_err(dev, "reset controller err %d\n",
				(int) PTR_ERR(priv->rstc0));
		return PTR_ERR(priv->rstc0);
	}

	priv->rstc1 = devm_reset_control_get(dev, "hdmi1");
	if (IS_ERR(priv->rstc1)) {
		dev_err(dev, "reset controller err %d\n",
				(int) PTR_ERR(priv->rstc1));
		return PTR_ERR(priv->rstc1);
	}

	priv->soc_type = (int) of_match_device(de2_hdmi_dt_ids,
						&pdev->dev)->data;

	if (IS_ENABLED(CONFIG_SND_SOC_SUNXI_HDMI)) {
		priv->codec.set_audio_input = de2_hdmi_set_audio_input;
		sunxi_hdmi_codec_register(dev);
	}

	return component_add(dev, &de2_hdmi_ops);
}

static int de2_hdmi_remove(struct platform_device *pdev)
{
	if (IS_ENABLED(CONFIG_SND_SOC_SUNXI_HDMI))
		sunxi_hdmi_codec_unregister(&pdev->dev);
	component_del(&pdev->dev, &de2_hdmi_ops);

	return 0;
}

static struct platform_driver de2_hdmi_driver = {
	.probe = de2_hdmi_probe,
	.remove = de2_hdmi_remove,
	.driver = {
		.name = "sunxi-de2-hdmi",
		.of_match_table = of_match_ptr(de2_hdmi_dt_ids),
	},
};

module_platform_driver(de2_hdmi_driver);

MODULE_AUTHOR("Jean-Francois Moine <moinejf@free.fr>");
MODULE_DESCRIPTION("Allwinner DE2 HDMI encoder/connector");
MODULE_LICENSE("GPL");
