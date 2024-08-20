// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_COMMON_H_
#define _MSM_COMMON_H_

#include <sound/soc.h>
#include <sound/pcm.h>
#include <dsp/apr_audio-v2.h>

enum {
	PRIM_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	QUIN_MI2S,
	SEN_MI2S,
	MI2S_MAX,
};

struct msm_asoc_mach_data {
	struct snd_info_entry *codec_root;
	int usbc_en2_gpio; /* used by gpio driver API */
	int lito_v2_enabled;
	struct device_node *dmic01_gpio_p; /* used by pinctrl API */
	struct device_node *dmic23_gpio_p; /* used by pinctrl API */
	struct device_node *dmic45_gpio_p; /* used by pinctrl API */
	struct device_node *mi2s_gpio_p[MI2S_MAX]; /* used by pinctrl API */
	atomic_t mi2s_gpio_ref_count[MI2S_MAX]; /* used by pinctrl API */
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct pinctrl *usbc_en2_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	bool supports_ext_mclk;
	struct ext_mclk_src_info *ext_mclk_srcs;
	u32 num_ext_mclk_srcs;
	bool is_afe_config_done;
	struct device_node *fsa_handle;
	struct clk *lpass_audio_hw_vote;
	int core_audio_vote_count;
	u32 wsa_max_devs;
	u32 tdm_max_slots; /* Max TDM slots used */
	int wcd_disabled;
	int (*get_wsa_dev_num)(struct snd_soc_component*);
	struct afe_cps_hw_intf_cfg cps_config;
};

int msm_lpass_audio_hw_vote_req(struct snd_pcm_substream *substream, bool enable);

#endif
