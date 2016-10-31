#ifndef __DE2_HDMI_IO_H__
#define __DE2_HDMI_IO_H__
/*
 * Copyright (C) 2016 Jean-François Moine
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */                             

void hdmi_io_video_on(struct de2_hdmi_priv *priv);
void hdmi_io_video_off(struct de2_hdmi_priv *priv);
int hdmi_io_audio(struct de2_hdmi_priv *priv,
		int sample_rate, int sample_bit);
int hdmi_io_video_mode(struct de2_hdmi_priv *priv);
int hdmi_io_ddc_read(struct de2_hdmi_priv *priv,
			char pointer, char offset,
			int nbyte, char *pbuf);
int hdmi_io_get_hpd(struct de2_hdmi_priv *priv);
void hdmi_io_init(struct de2_hdmi_priv *priv);
void hdmi_io_reset(struct de2_hdmi_priv *priv);
int hdmi_io_mode_valid(int cea_mode);

#endif /* __DE2_HDMI_IO_H__ */
