// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <sound/control.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/info.h>

#include "msm-common.h"

int msm_lpass_audio_hw_vote_req(struct snd_pcm_substream *substream, bool enable)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int rc = 0;

	if (enable) {
		if (pdata->lpass_audio_hw_vote == NULL) {
			dev_err(rtd->card->dev, "%s: Invalid lpass audio hw node\n",
				__func__);
			rc = -EINVAL;
			goto rtn;
		}
		if (pdata->core_audio_vote_count == 0) {
			rc = clk_prepare_enable(pdata->lpass_audio_hw_vote);
			if (rc < 0) {
				dev_err(rtd->card->dev, "%s: audio vote error\n",
						__func__);
				goto rtn;
			}
		}

		pdata->core_audio_vote_count++;
	} else {
		if (pdata->lpass_audio_hw_vote != NULL) {
			if (--pdata->core_audio_vote_count == 0) {
				clk_disable_unprepare(
					pdata->lpass_audio_hw_vote);
			} else if (pdata->core_audio_vote_count < 0) {
				pr_err("%s: audio vote mismatch\n", __func__);
				pdata->core_audio_vote_count = 0;
			}
		} else {
			pr_err("%s: Invalid lpass audio hw node\n", __func__);
		}
	}

rtn:
	return rc;
}
EXPORT_SYMBOL(msm_lpass_audio_hw_vote_req);

