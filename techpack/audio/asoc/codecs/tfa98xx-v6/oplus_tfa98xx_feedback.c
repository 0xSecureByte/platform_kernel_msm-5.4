/******************************************************************************
** File: - oplus_tfa98xx_feedback.cpp
**
** Copyright (C), 2022-2024, OPLUS Mobile Comm Corp., Ltd
**
** Description:
**     Implementation of tfa98xx reg error or speaker r0 or f0 error feedback.
**
** Version: 1.0
** --------------------------- Revision History: ------------------------------
**      <author>                                       <date>                  <desc>
*******************************************************************************/

#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "config.h"
#include "tfa98xx.h"
#include "tfa.h"
#include "tfa_internal.h"

#include <soc/oplus/system/oplus_mm_kevent_fb.h>

#define SMARTPA_ERR_FB_VERSION             "1.0.0"
#define SPK_ERR_FB_VERSION                 "1.0.2"

#define OPLUS_AUDIO_EVENTID_SMARTPA_ERR    10041
#define OPLUS_AUDIO_EVENTID_SPK_ERR        10042

/*bit0~3 check status registers; bit16:check speaker status*/
#define CHECK_STATUS_REGS_MASKS (0xF)
#define CHECK_SPEAKER_MASKS     (0x100)
#define IS_EXIT_CHECK_WORK(flag)    (!(flag & CHECK_SPEAKER_MASKS))

#define CHECK_DAMAGE_TIME            MM_FB_KEY_RATELIMIT_30MIN

/* SB35-->8.28*/
#define TFA_LIB_VER_SB35             0x81c0000
/* default threshold */
#define R0_MIN                       2000
#define R0_MAX                       14000
#define F0_MIN                       200
#define F0_MAX                       2000
#define F0_BLOCK_HOLE                1000

#define R0_BASE_RANGE                3000

enum {
	MONO = 0,
	LEFT = MONO,
	RIGHT,
	MAX_SPK_NUM
};

#define IS_DEV_COUNT_VALID(cnt) (((cnt) > MONO) && ((cnt) <= MAX_SPK_NUM))

#define SetFdBuf(buf, arg, ...) \
	do { \
		int len = strlen(buf); \
		snprintf(buf + len, sizeof(buf) - len - 1, arg, ##__VA_ARGS__); \
	} while (0)

struct oplus_tfa98xx_feedback {
	uint32_t chk_flag;
	uint32_t queue_work_flag;
	int pa_cnt;
	int cmd_step;
	uint32_t lib_version;
	uint32_t lib_new;
	uint32_t r0_cal[MAX_SPK_NUM];
	uint32_t r0_min[MAX_SPK_NUM];
	uint32_t r0_max[MAX_SPK_NUM];
	uint32_t f0_min[MAX_SPK_NUM];
	uint32_t f0_max[MAX_SPK_NUM];
	uint32_t damage_flag;
	uint32_t last_damage_r0[MAX_SPK_NUM];
	uint32_t last_damage_f0[MAX_SPK_NUM];
	ktime_t start_damage_tm[MAX_SPK_NUM];
	struct mutex *lock;
	ktime_t last_chk_reg;
	ktime_t last_chk_spk;
};

static struct oplus_tfa98xx_feedback tfa_fb = {
	.chk_flag = 0,
	.queue_work_flag = 0,
	.pa_cnt = 0,
	.cmd_step = 0,
	.lib_version = 0,
	.lib_new = 0xff,
	.r0_cal = {0, 0},
	.r0_min = {R0_MIN, R0_MIN},
	.r0_max = {R0_MAX, R0_MAX},
	.f0_min = {F0_MIN, F0_MIN},
	.f0_max = {F0_MAX, F0_MAX},
	.damage_flag = 0,
	.last_damage_r0 = {0, 0},
	.last_damage_f0 = {0, 0},
	.start_damage_tm = {0, 0},
	.lock = NULL,
	.last_chk_reg = 0,
	.last_chk_spk = 0
};

#define ERROR_INFO_MAX_LEN                 32
#define REG_BITS  16
#define TFA9874_STATUS_NORMAL_VALUE    ((0x850F << REG_BITS) + 0x16)/*reg 0x13 high 16 bits and 0x10 low 16 bits*/
#define TFA9874_STATUS_CHECK_MASK      ((0x300 << REG_BITS) + 0x9C)/*reg 0x10 mask bit2~4, bit7, reg 0x13 mask bit8 , bit9 */
#define TFA9873_STATUS_NORMAL_VALUE    ((0x850F << REG_BITS) + 0x56) /*reg 0x13 high 16 bits and 0x10 low 16 bits*/
#define TFA9873_STATUS_CHECK_MASK      ((0x300 << REG_BITS) + 0x15C)/*reg 0x10 mask bit2~4, bit6, bit8, reg 0x13 mask bit8 , bit9*/

struct check_status_err {
	int bit;
	uint32_t err_val;
	char info[ERROR_INFO_MAX_LEN];
};

static const struct check_status_err check_err_tfa9874[] = {
	/*register 0x10 check bits*/
	{2,             0, "OverTemperature"},
	{3,             1, "CurrentHigh"},
	{4,             0, "VbatLow"},
	{7,             1, "NoClock"},
	/*register 0x13 check bits*/
	{8 + REG_BITS,  0, "VbatHigh"},
	{9 + REG_BITS,  1, "Clipping"},
};

static const struct check_status_err check_err_tfa9873[] = {
	/*register 0x10 check bits*/
	{2,             0, "OverTemperature"},
	{3,             1, "CurrentHigh"},
	{4,             0, "VbatLow"},
	{6,             0, "UnstableClk"},
	{8,             1, "NoClock"},
	/*register 0x13 check bits*/
	{8 + REG_BITS,  0, "VbatHigh"},
	{9 + REG_BITS,  1, "Clipping"},
};

static const unsigned char fb_regs[] = {0x00, 0x01, 0x02, 0x04, 0x05, 0x11, 0x14, 0x15, 0x16};


#define OPLUS_CHECK_LIMIT_TIME  (5*MM_FB_KEY_RATELIMIT_1H)
#define CHECK_SPK_DELAY_TIME    (6)/* seconds */

/* this enum order must consistent with ready command data, such as: tfaCmdStereoReady_LP */
enum {
	POS_R0 = 0,
	POS_F0,
	POS_NUM
};

extern enum Tfa98xx_Error
tfa98xx_write_dsp(struct tfa_device *tfa,  int num_bytes, const char *command_buffer);
extern enum Tfa98xx_Error
tfa98xx_read_dsp(struct tfa_device *tfa,  int num_bytes, unsigned char *result_buffer);


#define TFA_DATA_BYTES               3
#define TFA_OFFSET_BASE              3
#define PARAM_OFFSET(pos, id)        (TFA_OFFSET_BASE + ((id) * POS_NUM + (pos)) * TFA_DATA_BYTES)
#define GET_VALUE(pdata, offset)     ((pdata[offset] << 16) + (pdata[offset+1] << 8) + pdata[offset+2])
#define TFA_GET_R0(pdata, id)        (GET_VALUE(pdata, PARAM_OFFSET(POS_R0, id)) / 65) /* 65 = 0x10000 / 1000 */
#define TFA_GET_F0(pdata, id)        (GET_VALUE(pdata, PARAM_OFFSET(POS_F0, id)))
#define TFA_MAX_RESULT_LEN           (MAX_SPK_NUM * POS_NUM * TFA_DATA_BYTES + TFA_OFFSET_BASE)
#define TFA_RESULT_BUF_LEN           ((TFA_MAX_RESULT_LEN + 3) & (~3))
#define TFA_ONE_ALGO_MAX_RESULT_LEN  (2 * POS_NUM * TFA_DATA_BYTES + TFA_OFFSET_BASE)
#define TFA_CMD_READY_LEN(spkNum)    (((spkNum) * POS_NUM + 2) * TFA_DATA_BYTES)
#define TFA_DATA_NUM_OFFSET          5
#define TFA_CMD_HEAD_OFFSET          0


static int tfa98xx_set_check_feedback(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int need_chk = ucontrol->value.integer.value[0];

	/* only record true status, tfa_fb.chk_flag will set false in tfa98xx_mute*/
	if (need_chk) {
		tfa_fb.chk_flag = CHECK_SPEAKER_MASKS + CHECK_STATUS_REGS_MASKS;
	}
	pr_info("need_chk = %d, tfa_fb.chk_flag = 0x%x\n", need_chk, tfa_fb.chk_flag);

	return 1;
}

static int tfa98xx_get_check_feedback(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tfa_fb.chk_flag;
	pr_info("tfa_fb.chk_flag = 0x%x\n", tfa_fb.chk_flag);

	return 0;
}

static char const *tfa98xx_check_feedback_text[] = {"Off", "On"};
static const struct soc_enum tfa98xx_check_feedback_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tfa98xx_check_feedback_text), tfa98xx_check_feedback_text);
const struct snd_kcontrol_new tfa98xx_check_feedback[] = {
	SOC_ENUM_EXT("TFA_CHECK_FEEDBACK", tfa98xx_check_feedback_enum,
		       tfa98xx_get_check_feedback, tfa98xx_set_check_feedback),
};

inline bool is_param_valid(struct tfa98xx *tfa98xx)
{
	if ((tfa98xx == NULL) || (tfa98xx->tfa == NULL) || (tfa98xx->tfa98xx_wq == NULL)) {
		pr_err("input parameter is not available\n");
		return false;
	}

	if ((tfa98xx->pa_type != PA_TFA9874) && (tfa98xx->pa_type != PA_TFA9873)) {
		return false;
	}

	if (tfa98xx->tfa->dev_idx >= MAX_SPK_NUM) {
		pr_err("dev_idx = %d error\n", tfa98xx->tfa->dev_idx);
		return false;
	}

	return true;
}

static int tfa98xx_check_status_reg(struct tfa98xx *tfa98xx)
{
	uint32_t reg_val;
	uint16_t reg10 = 0;
	uint16_t reg13 = 0;
	uint16_t reg_tmp = 0;
	int flag = 0;
	char fd_buf[MAX_PAYLOAD_DATASIZE] = {0};
	char info[MAX_PAYLOAD_DATASIZE] = {0};
	int offset = 0;
	enum Tfa98xx_Error err;
	int i;

	mutex_lock(tfa_fb.lock);
	/* check status register 0x10 value */
	err = tfa98xx_read_register16_v6(tfa98xx->tfa, 0x10, &reg10);
	if (Tfa98xx_Error_Ok == err) {
		err = tfa98xx_read_register16_v6(tfa98xx->tfa, 0x13, &reg13);
	}
	pr_info("read SPK%d status regs ret=%d, reg[0x10]=0x%x, reg[0x13]=0x%x", \
			tfa98xx->tfa->dev_idx + 1, err, reg10, reg13);

	if (Tfa98xx_Error_Ok == err) {
		reg_val = (reg13 << REG_BITS) + reg10;
		flag = 0;
		if ((tfa98xx->pa_type == PA_TFA9874) &&
				((TFA9874_STATUS_NORMAL_VALUE&TFA9874_STATUS_CHECK_MASK) != (reg_val&TFA9874_STATUS_CHECK_MASK))) {
			SetFdBuf(info, "TFA9874 SPK%x:reg[0x10]=0x%x,reg[0x13]=0x%x,", tfa98xx->tfa->dev_idx + 1, reg10, reg13);
			for (i = 0; i < ARRAY_SIZE(check_err_tfa9874); i++) {
				if (check_err_tfa9874[i].err_val == (1 & (reg_val >> check_err_tfa9874[i].bit))) {
					SetFdBuf(info, "%s,", check_err_tfa9874[i].info);
				}
			}
			flag = 1;
		} else if ((tfa98xx->pa_type == PA_TFA9873) &&
				((TFA9873_STATUS_NORMAL_VALUE&TFA9873_STATUS_CHECK_MASK) != (reg_val&TFA9873_STATUS_CHECK_MASK))) {
			SetFdBuf(info, "TFA9873 SPK%x:reg[0x10]=0x%x,reg[0x13]=0x%x,", tfa98xx->tfa->dev_idx + 1, reg10, reg13);
			for (i = 0; i < ARRAY_SIZE(check_err_tfa9873); i++) {
				if (check_err_tfa9873[i].err_val == (1 & (reg_val >> check_err_tfa9873[i].bit))) {
					SetFdBuf(info, "%s,", check_err_tfa9873[i].info);
				}
			}
			flag = 1;
		}

		/* read other registers */
		if (flag == 1) {
			SetFdBuf(info, "dump regs(");
			for (i = 0; i < sizeof(fb_regs); i++) {
				err = tfa98xx_read_register16_v6(tfa98xx->tfa, fb_regs[i], &reg_tmp);
				if (Tfa98xx_Error_Ok == err) {
					SetFdBuf(info, "%x=%x,", fb_regs[i], reg_tmp);
				} else {
					break;
				}
			}
			SetFdBuf(info, "),");
		}
	} else {
		SetFdBuf(info, "%s SPK%d: failed to read regs 0x10 and 0x13, error=%d,", \
				(tfa98xx->pa_type == PA_TFA9873) ? "TFA9873" : "TFA9874", tfa98xx->tfa->dev_idx + 1, err);
		tfa_fb.last_chk_reg = ktime_get();
	}
	mutex_unlock(tfa_fb.lock);

	/* feedback the check error */
	offset = strlen(info);
	if ((offset > 0) && (offset < MM_KEVENT_MAX_PAYLOAD_SIZE)) {
		scnprintf(fd_buf, sizeof(fd_buf) - 1, "payload@@%s", info);
		pr_err("fd_buf=%s\n", fd_buf);
		mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SMARTPA_ERR,
				MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
	}

	return 0;
}

static int tfa98xx_cmd_set(struct tfa98xx *tfa98xx, int8_t *pbuf, int16_t size)
{
	int err = -1;

	if ((pbuf == NULL) || (size <= 0)) {
		pr_err("input parameter is not available\n");
		return -ENODEV;
	}

	if (tfa98xx && tfa98xx->tfa->is_probus_device) {
		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_dsp(tfa98xx->tfa, size, pbuf);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (err != Tfa98xx_Error_Ok) {
		pr_err("send data to adsp error, ret = %d\n", err);
	}

	return err;
}

static int tfa98xx_get_lib_version(struct tfa98xx *tfa98xx, unsigned int *pversion)
{
	int err = -1;
	unsigned char result[TFA_ONE_ALGO_MAX_RESULT_LEN] = {0};
	int8_t tfaCmdLibVer[] = {0x00, 0x80, 0xfe};
	unsigned char lib_ver[4]= {0};

	if (pversion == NULL) {
		pr_err("input parameter is not available\n");
		return -ENODEV;
	}

	if (tfa98xx && tfa98xx->tfa->is_probus_device) {
		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_dsp(tfa98xx->tfa, sizeof(tfaCmdLibVer), tfaCmdLibVer);
		if (err == Tfa98xx_Error_Ok) {
			err = tfa98xx_read_dsp(tfa98xx->tfa, sizeof(result), result);
		}
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (err != Tfa98xx_Error_Ok) {
		pr_err("send data to adsp error, ret = %d\n", err);
		return err;
	}

	/* Split 3rd byte into two seperate ITF version fields (3rd field and 4th field) */
	lib_ver[0] = (result[0]);
	lib_ver[1] = (result[1]);
	if ((lib_ver[0] != 2) && (lib_ver[1] >= 33)) {
		lib_ver[3] = (result[2]) & 0x07;
		lib_ver[2] = (result[2] >> 3) & 0x1F;
	} else {
		lib_ver[3] = (result[2]) & 0x3f;
		lib_ver[2] = (result[2] >> 6) & 0x03;
	}
	*pversion = (lib_ver[0] << 24) + (lib_ver[1] << 16) + (lib_ver[2] << 8) + lib_ver[3];
	pr_info("tfa lib version is %d.%d.%d.%d, version=0x%x", \
			lib_ver[0], lib_ver[1], lib_ver[2], lib_ver[3], *pversion);

	return err;
}

static int tfa98xx_check_new_lib(struct tfa98xx *tfa98xx)
{
	unsigned int lib_ver = 0;

	if (Tfa98xx_Error_Ok == tfa98xx_get_lib_version(tfa98xx, &lib_ver)) {
		if (lib_ver != 0) {
			tfa_fb.lib_version = lib_ver;
			tfa_fb.lib_new = (lib_ver > TFA_LIB_VER_SB35) ? 1 : 0;
			pr_info("lib_new=%d", tfa_fb.lib_new);
			return Tfa98xx_Error_Ok;
		}
	}

	return -1;
}

static bool tfa98xx_is_algorithm_working(struct tfa98xx *tfa98xx)
{
	unsigned int lib_ver = 0;

	if (Tfa98xx_Error_Ok == tfa98xx_get_lib_version(tfa98xx, &lib_ver)) {
		pr_info("get lib_ver=0x%x, record lib_version = 0x%x", lib_ver, tfa_fb.lib_version);
		if ((lib_ver != 0) && (tfa_fb.lib_version == lib_ver)) {
			return true;
		}
	}

	return false;
}

static int tfa98xx_get_speaker_status(struct tfa98xx *tfa98xx)
{
	char fd_buf[MAX_PAYLOAD_DATASIZE] = {0};
	enum Tfa98xx_Error err = Tfa98xx_Error_Ok;
	char buffer[6] = {0};

	if ((CHECK_SPEAKER_MASKS & tfa_fb.chk_flag) == 0) {
		return -1;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	/*Get the GetStatusChange results*/
	err = tfa_dsp_cmd_id_write_read_v6(tfa98xx->tfa, MODULE_FRAMEWORK,
			FW_PAR_ID_GET_STATUS_CHANGE, 6, (unsigned char *)buffer);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_info("ret=%d, get value=%d\n", err, buffer[2]);

	if (err == Tfa98xx_Error_Ok) {
		if (buffer[2] & 0x6) {
			if (buffer[2] & 0x2) {
				tfa_fb.damage_flag |= (1 << LEFT);
			}
			if ((tfa_fb.pa_cnt > 1) && (buffer[2] & 0x4)) {
				tfa_fb.damage_flag |= (1 << RIGHT);
			}
		}
	} else {
		pr_err("tfa_dsp_cmd_id_write_read_v6 err = %d\n", err);
		SetFdBuf(fd_buf, "payload@@tfa_dsp_cmd_id_write_read_v6 err = %u", (unsigned int)err);
		mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SMARTPA_ERR,
				MM_FB_KEY_RATELIMIT_5MIN, fd_buf);
	}
	pr_info("ret=%d, damage_flag=%d\n", err, tfa_fb.damage_flag);

	return err;
}

/* set cmd to protection algorithm to calcurate r0, f0 */
static int tfa98xx_set_ready_cmd(struct tfa98xx *tfa98xx)
{
	int16_t spk_num = 0;
	int16_t cmdlen = 0;
	int8_t *pcmd = NULL;

	/*cmd BYTE0: first algorithm: 0x00, second algorithm: 0x10*/
	/*cmd BYTE5: need to change as accroding to get param num */
	int8_t tfaCmdReady[] = {
		0x00, 0x80, 0x0b, 0x00, 0x00, 0x04,
		/*first speaker: R0, F0*/
		0x22, 0x00, 0x00,
		0x22, 0x00, 0x13,
		/*second speaker*/
		0x22, 0x00, 0x01,
		0x22, 0x00, 0x14
	};

	/* for new version SB4.0 */
	int8_t tfaCmdReady_SB40[] = {
		0x00, 0x80, 0x0b, 0x00, 0x00, 0x04,
		/*first speaker: R0, F0*/
		0x22, 0x00, 0x00,
		0x22, 0x00, 0x1b,
		/*second speaker*/
		0x22, 0x00, 0x01,
		0x22, 0x00, 0x1c
	};

	pcmd = tfa_fb.lib_new ? tfaCmdReady_SB40 : tfaCmdReady;
	*(pcmd + TFA_CMD_HEAD_OFFSET) = 0x00;
	*(pcmd + TFA_DATA_NUM_OFFSET) = (tfa_fb.pa_cnt == 1) ? POS_NUM : POS_NUM*2;
	spk_num = (tfa_fb.pa_cnt == 1) ? 1 : 2;
	cmdlen = TFA_CMD_READY_LEN(spk_num);

	return tfa98xx_cmd_set(tfa98xx, pcmd, cmdlen);
}

static int tfa98xx_get_result(struct tfa98xx *tfa98xx, uint8_t *pbuf, int64_t len, uint32_t algo_flag)
{
	uint8_t tfaCmdGet[] = {0x00, 0x80, 0x8b, 0x00};
	int err = Tfa98xx_Error_Ok;
	int idx = 0;

	if (!pbuf || (0 == len)) {
		err = -EINVAL;
		pr_err("input param error");
		goto exit;
	}

	if (1 == algo_flag) {
		tfaCmdGet[TFA_CMD_HEAD_OFFSET] = 0x00;
	} else if (2 == algo_flag) {
		tfaCmdGet[TFA_CMD_HEAD_OFFSET] = 0x10;
	} else {
		err = -EINVAL;
		pr_err("not support algo_flag = %u", algo_flag);
		goto exit;
	}

	if (tfa98xx && tfa98xx->tfa->is_probus_device) {
		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_dsp(tfa98xx->tfa, sizeof(tfaCmdGet), tfaCmdGet);
		if (err == Tfa98xx_Error_Ok) {
			err = tfa98xx_read_dsp(tfa98xx->tfa, len, pbuf);
		}
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (err != Tfa98xx_Error_Ok) {
		pr_err("set or get adsp data error, ret = %d\n", err);
		goto exit;
	}

	for (idx = 0; idx < len; idx += 4) {
		pr_debug("get data: 0x%x  0x%x  0x%x  0x%x\n", \
			*(pbuf + idx), *(pbuf + idx + 1), *(pbuf + idx + 2), *(pbuf + idx + 3));
	}

	if (0x00 == *(pbuf + 2)) {
		pr_err("get adsp data error\n");
		err = Tfa98xx_Error_DSP_not_running;
		goto exit;
	}

exit:
	return err;
}

static int tfa98xx_check_result(struct tfa98xx *tfa98xx, uint8_t *pdata)
{
	int err = Tfa98xx_Error_Ok;
	uint32_t re = 0;
	uint32_t fr = 0;
	char fb_buf[MAX_PAYLOAD_DATASIZE] = {0};
	bool spk_err = false;
	int index = 0;

	if (!pdata) {
		err = -EINVAL;
		pr_err("pdata is null");
		goto exit;
	}
	if (!IS_DEV_COUNT_VALID(tfa_fb.pa_cnt)) {
		err = -EINVAL;
		pr_err("pa_cnt %d invalid", tfa_fb.pa_cnt);
		goto exit;
	}

	SetFdBuf(fb_buf, "payload@@");
	for (index = 0; index < tfa_fb.pa_cnt; index++) {
		if (tfa_fb.damage_flag & (1 << index)) {
			SetFdBuf(fb_buf, "TFA98xx SPK%d-detected-damaged;", (index + 1));
		} else {
			tfa_fb.last_damage_r0[index] = 0;
			tfa_fb.last_damage_f0[index] = 0;
			tfa_fb.start_damage_tm[index] = 0;
			pr_info("speaker%d, R0 = %u, F0 = %u", \
					index+1, TFA_GET_R0(pdata, index), TFA_GET_F0(pdata, index));
			continue;
		}

		/* check r0 out of range */
		re = TFA_GET_R0(pdata, index);
		if ((re < tfa_fb.r0_min[index]) || (re > tfa_fb.r0_max[index])) {
			SetFdBuf(fb_buf, "TFA98xx SPK%u:R0=%u,out of range(%u, %u),R0_cal=%u;", \
					index+1, re, tfa_fb.r0_min[index], tfa_fb.r0_max[index], tfa_fb.r0_cal[index]);
			spk_err = true;
		}
		pr_info("speaker%d, R0 = %u", index+1, re);

		/* check f0 out of range */
		fr = TFA_GET_F0(pdata, index);
		if ((fr < tfa_fb.f0_min[index]) || (fr > tfa_fb.f0_max[index])) {
			SetFdBuf(fb_buf, "TFA98xx SPK%u:F0=%u,out of range(%u, %u);", \
				   index+1, fr, tfa_fb.f0_min[index], tfa_fb.f0_max[index]);
			spk_err = true;
		}
		pr_info("speaker%d, F0 = %u", index+1, fr);

		/* When short circuit or open circuit, read r0 will return same value */
		/* damaged but f0 and r0 is ok, record and check whether r0 is same . */
		if ((false == spk_err) && (tfa_fb.damage_flag & (1 << index)) && (fr < F0_BLOCK_HOLE)) {
			if ((re == tfa_fb.last_damage_r0[index]) && (fr == tfa_fb.last_damage_f0[index])) {
				if (tfa_fb.start_damage_tm[index] == 0) {
					tfa_fb.start_damage_tm[index] = ktime_get();
				} else if (ktime_after(ktime_get(), ktime_add_ms(tfa_fb.start_damage_tm[index], CHECK_DAMAGE_TIME))) {
					SetFdBuf(fb_buf, "TFA98xx SPK%u:speaker short circuit or open circuit.R0=%u,R0_cal=%u,F0=%u;", \
							index+1, re, tfa_fb.r0_cal[index], fr);
					spk_err = true;
					tfa_fb.last_damage_r0[index] = 0;
					tfa_fb.last_damage_f0[index] = 0;
					tfa_fb.start_damage_tm[index] = 0;
				}
				pr_info("speaker%d, maybe short circuit or open circuit", index + 1);
			} else {
				tfa_fb.last_damage_r0[index] = re;
				tfa_fb.last_damage_f0[index] = fr;
				tfa_fb.start_damage_tm[index] = ktime_get();
			}
		}
	}

	if (spk_err) {
		if (tfa98xx_is_algorithm_working(tfa98xx)) {
			if (spk_err) {
				mm_fb_audio_kevent_named(OPLUS_AUDIO_EVENTID_SPK_ERR,
						MM_FB_KEY_RATELIMIT_1H, fb_buf);
			}
		}
	}

exit:
	return err;
}

static void tfa98xx_check_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx = container_of(work, struct tfa98xx, check_work.work);
	uint8_t rebuf[TFA_RESULT_BUF_LEN] = {0};
	int ret = -1;

	if ((CHECK_SPEAKER_MASKS & tfa_fb.chk_flag) == 0) {
		return;
	}

	if (!is_param_valid(tfa98xx)) {
		pr_err("parameter is not available\n");
		return;
	}

	pr_info("chk_flag = 0x%x, cmd_step = %d, pa_cnt = %d, lib_new = %d\n", \
			tfa_fb.chk_flag, tfa_fb.cmd_step, tfa_fb.pa_cnt, tfa_fb.lib_new);

	switch (tfa_fb.cmd_step) {
	case 0:
		/*get algorithm library version if not get before*/
		if ((tfa_fb.lib_new != 0) && (tfa_fb.lib_new != 1)) {
			ret = tfa98xx_check_new_lib(tfa98xx);
			if (Tfa98xx_Error_Ok != ret) {
				goto exit;
			}
		}

		if(IS_EXIT_CHECK_WORK(tfa_fb.chk_flag)) {
			goto exit;
		}

		/*get speaker hole blocked or damaged status */
		ret = tfa98xx_get_speaker_status(tfa98xx);
		if (Tfa98xx_Error_Ok != ret) {
			goto exit;
		}

		/* not to check R0, F0 value if:
			1. not detected speaker damage
			2. has checked since boot
			3. limit time do not pass since last check */
		if (tfa_fb.damage_flag == 0 && (tfa_fb.last_chk_spk != 0) && \
				ktime_before(ktime_get(), ktime_add_ms(tfa_fb.last_chk_spk, OPLUS_CHECK_LIMIT_TIME))) {
			tfa_fb.chk_flag = (~CHECK_SPEAKER_MASKS) & tfa_fb.chk_flag;
			tfa_fb.cmd_step = 0;
			goto exit;
		} else {
			queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->check_work, \
				msecs_to_jiffies(20));
			tfa_fb.cmd_step = 1;
		}
		break;
	case 1:
		/* set cmd to protection algorithm to calcurate r0, f0 */
		ret = tfa98xx_set_ready_cmd(tfa98xx);
		if (Tfa98xx_Error_Ok == ret) {
			queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->check_work, \
					msecs_to_jiffies(100));
			tfa_fb.cmd_step = 2;
		}
		break;
	case 2:
		/* set cmd to get r0, f0 */
		ret = tfa98xx_get_result(tfa98xx, rebuf, sizeof(rebuf), 1);
		if (ret == 0) {
			if(IS_EXIT_CHECK_WORK(tfa_fb.chk_flag)) {
				goto exit;
			}

			tfa98xx_check_result(tfa98xx, rebuf);
			tfa_fb.last_chk_spk = ktime_get();
		}
		tfa_fb.chk_flag = (~CHECK_SPEAKER_MASKS) & tfa_fb.chk_flag;
		tfa_fb.cmd_step = 0;
		break;
	default:
		break;
	}

exit:
	if (Tfa98xx_Error_Ok != ret) {
		tfa_fb.chk_flag = (~CHECK_SPEAKER_MASKS) & tfa_fb.chk_flag;
		tfa_fb.cmd_step = 0;
		tfa_fb.last_chk_spk = ktime_get();
		pr_err("error: ret = %d\n", ret);
	}

	return;
}

void oplus_tfa98xx_record_r0_cal(int r0_cal, int dev_idx)
{
	if ((dev_idx >= MONO) && (dev_idx < MAX_SPK_NUM)) {
		if (r0_cal == tfa_fb.r0_cal[dev_idx]) {
			return;
		}

		if ((r0_cal > R0_MIN) && (r0_cal < R0_MAX)) {
			tfa_fb.r0_cal[dev_idx] = r0_cal;
			tfa_fb.r0_max[dev_idx] = r0_cal + R0_BASE_RANGE;
			tfa_fb.r0_min[dev_idx] = r0_cal - R0_BASE_RANGE;
			pr_info("new r0_cal = %d, dev_idx=%d, [%d, %d]\n", \
				r0_cal, dev_idx, tfa_fb.r0_min[dev_idx], tfa_fb.r0_max[dev_idx]);
		} else {
			pr_err("invalid r0_cal=%d and threshold[%d, %d], dev_idx=%d and max=%d\n", \
				r0_cal, R0_MIN, R0_MAX, dev_idx, MAX_SPK_NUM);
		}
	} else {
		pr_info("unsupport dev_idx=%d\n", dev_idx);
	}
}

void oplus_tfa98xx_check_reg(struct tfa98xx *tfa98xx)
{
	uint32_t channel = 0;

	if (!is_param_valid(tfa98xx)) {
		pr_err("parameter is not available\n");
		return;
	}

	if (NULL == tfa_fb.lock) {
		return;
	}

	channel = tfa98xx->tfa->channel;
	if (((1 << channel) & tfa_fb.chk_flag) == 0) {
		return;
	}
	tfa_fb.chk_flag = (~(1 << channel)) & tfa_fb.chk_flag;

	if ((tfa_fb.last_chk_reg != 0) && \
			ktime_before(ktime_get(), ktime_add_ms(tfa_fb.last_chk_reg, MM_FB_KEY_RATELIMIT_5MIN))) {
		return;
	}

	tfa98xx_check_status_reg(tfa98xx);
}

void oplus_tfa98xx_queue_check_work(struct tfa98xx *tfa98xx)
{
	if (((tfa_fb.chk_flag & CHECK_SPEAKER_MASKS) != 0) && (0 == tfa_fb.queue_work_flag)) {
		if (!is_param_valid(tfa98xx)) {
			pr_err("parameter is not available\n");
			return;
		}

		if (tfa98xx && tfa98xx->tfa98xx_wq && tfa98xx->check_work.work.func && (0 == tfa_fb.queue_work_flag)) {
			pr_info("queue delay work for check speaker\n");
			queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->check_work, CHECK_SPK_DELAY_TIME * HZ);
			tfa_fb.queue_work_flag = 1;
			tfa_fb.cmd_step = 0;
		}
	}
}

void oplus_tfa98xx_exit_check_work(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && tfa98xx->check_work.wq && (1 == tfa_fb.queue_work_flag)) {
		pr_info("cancel delay work for check speaker\n");
		tfa_fb.chk_flag = (~CHECK_SPEAKER_MASKS) & tfa_fb.chk_flag;
		tfa_fb.cmd_step = 0;
		cancel_delayed_work_sync(&tfa98xx->check_work);
		tfa_fb.queue_work_flag = 0;
	}
}

void oplus_tfa98xx_get_dt(struct tfa98xx *tfa98xx, struct device_node *np)
{
	int ret = 0;
	uint32_t channel = 0;

	if (tfa98xx && tfa98xx->tfa && np) {
		if ((tfa98xx->tfa->channel < MAX_SPK_NUM) || (tfa98xx->tfa->channel == 0xff)) {
			channel = (tfa98xx->tfa->channel == 0xff) ? 0 : tfa98xx->tfa->channel;
			tfa_fb.r0_min[channel] = tfa98xx->tfa->min_mohms;
			tfa_fb.r0_max[channel] = tfa98xx->tfa->max_mohms;

			ret = of_property_read_u32(np, "tfa_min_f0", &tfa_fb.f0_min[channel]);
			if (ret) {
				pr_info("Failed to parse tfa_min_f0 node\n");
				tfa_fb.f0_min[channel] = F0_MIN;
			}

			ret = of_property_read_u32(np, "tfa_max_f0", &tfa_fb.f0_max[channel]);
			if (ret) {
				pr_info("Failed to parse tfa_max_f0 node\n");
				tfa_fb.f0_max[channel] = F0_MAX;
			}

			pr_info("spk%u r0 range (%u, %u), f0 min=%u\n", \
					channel, tfa_fb.r0_min[channel], tfa_fb.r0_max[channel], tfa_fb.f0_min[channel]);
		}
	}
}

void oplus_tfa98xx_feedback_init(struct tfa98xx *tfa98xx, struct mutex *lock, int count)
{
	if (tfa98xx && tfa98xx->component && tfa98xx->tfa && lock) {
		if (NULL == tfa_fb.lock) {
			snd_soc_add_component_controls(tfa98xx->component,
					   tfa98xx_check_feedback,
					   ARRAY_SIZE(tfa98xx_check_feedback));

			INIT_DELAYED_WORK(&tfa98xx->check_work, tfa98xx_check_work);

			tfa_fb.lock = lock;
			if ((count > 0) && (count <= MAX_SPK_NUM)) {
				tfa_fb.pa_cnt = count;
			} else {
				tfa_fb.pa_cnt = 1;
			}
			pr_info("success, pa_cnt = %d\n", tfa_fb.pa_cnt);
			pr_info("event_id=%u, version:%s\n", OPLUS_AUDIO_EVENTID_SMARTPA_ERR, SMARTPA_ERR_FB_VERSION);
			pr_info("event_id=%u, version:%s\n", OPLUS_AUDIO_EVENTID_SPK_ERR, SPK_ERR_FB_VERSION);
		} else {
			tfa98xx->check_work.work.func = NULL;
			tfa98xx->check_work.wq = NULL;
		}
	}
}

