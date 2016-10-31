#ifndef __SUNXI_HDMI_H__
#define __SUNXI_HDMI_H__
/*
 * Copyright (C) 2016 Jean-François Moine
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

struct sunxi_hdmi_codec {
	u8 *eld;
	int (*set_audio_input)(struct device *dev,
				int enable,
				unsigned sample_rate,
				unsigned sample_bit);
};

int sunxi_hdmi_codec_register(struct device *dev);
void sunxi_hdmi_codec_unregister(struct device *dev);

#endif /* __SUNXI_HDMI_H__ */
