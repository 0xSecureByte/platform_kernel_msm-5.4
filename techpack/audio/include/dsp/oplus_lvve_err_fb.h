/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __OPLUS_LVVE_ERR_FB_H__
#define __OPLUS_LVVE_ERR_FB_H__
#include <sound/soc-component.h>
#include <dsp/q6voice.h>

#define LVVEFQ_TX_MODULE_ID            (0x1000B500)  /* 268481792 */
#define LVVEFQ_RX_MODULE_ID            (0x1000B501)  /* 268481793 */
/* LVVEFQ Tx 1-mic Topology ID */
#define VOICE_TOPOLOGY_LVVEFQ_TX_SM    (0x1000BFF0)  /* 268484592 */
/* LVVEFQ Tx 2-mic Topology ID */
#define VOICE_TOPOLOGY_LVVEFQ_TX_DM    (0x1000BFF1)  /* 268484593 */
/* LVVEFQ Tx 3-mic Topology ID */
#define VOICE_TOPOLOGY_LVVEFQ_TX_QM    (0x1000BFF3)  /* 268484595 */
/* VOICE LVVEFQ Rx Topology ID */
#define VOICE_TOPOLOGY_LVVEFQ_RX       (0x1000BFF2)  /* 268484594 */
/* AUDIO LVVEFQ Rx Topology ID */
#define AUDIO_TOPOLOGY_LVVEFQ_RX       (0x1000BFF4)  /* 268484596 */

#define VOICE_PARAM_LVVEFQ_GET_ERR     (0x1000BE10)  /* 268484112 */

#define VOICE_OR_VOIP_APP_TYPE         (0x0001113A)  /* 69946 */

void oplus_lvve_err_fb_add_controls(struct snd_soc_component *component);
void oplus_copy_voice_err_result(int port_type, void *presult, uint32_t len);
int oplus_adm_get_lvve_err_fb(int port_id, int copp_idx);
int oplus_voice_get_lvve_err_fb(void *apr_cvp, struct voice_data *v);

#endif /* __OPLUS_LVVE_ERR_FB_H__ */
