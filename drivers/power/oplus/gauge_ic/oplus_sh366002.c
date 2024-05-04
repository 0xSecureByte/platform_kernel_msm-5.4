// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2023 Oplus. All rights reserved.
 */

#define pr_fmt(fmt) "[sh366002] %s(%d): " fmt, __func__, __LINE__

#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <asm/div64.h>
#include <stdbool.h>

#include "oplus_bq27541.h"
#include "oplus_sh366002.h"

#include "../oplus_charger.h"
#include "../oplus_vooc.h"
#include "../oplus_chg_track.h"

#define DUMP_SH366002_BLOCK BIT(0)
#define DUMP_SH366002_CURRENT_BLOCK BIT(1)

#define GAUGE_LOG_MIN_TIMESPAN 10

#define CMD_CNTLSTATUS_SEAL 0x6000
#define CMD_UNSEALKEY 0x19861115
#define CMD_FULLKEY 0xFFFFFFFF

#define AFI_DATE_BLOCKA_INDEX 24
#define AFI_MANU_BLOCKA_INDEX 28
#define AFI_MANU_SINOWEALTH 0x02
#define CHECK_VERSION_ERR -1
#define CHECK_VERSION_OK 0
#define CHECK_VERSION_FW BIT(0)
#define CHECK_VERSION_AFI BIT(1)
#define CHECK_VERSION_TS BIT(2)
#define CHECK_VERSION_WHOLE_CHIP (CHECK_VERSION_FW | CHECK_VERSION_AFI | CHECK_VERSION_TS)
#define FILE_DECODE_RETRY 2
#define FILE_DECODE_DELAY 100
#define CADC_OFFSET_RETRY 3

#define WRITE_BUF_MAX_LEN 32
#define GAUGE_LOG_MIN_TIMESPAN 10
#define MAX_BUF_LEN 1024
#define DF_PAGE_LEN 32
#define DF_PAGE_HALF_LEN 16
#define CMD_SBS_DELAY 3
#define CMD_E2ROM_DELAY 1500
#define CMDMASK_MASK 0xFF000000
#define CMDMASK_SINGLE 0x01000000
#define CMDMASK_WRITE 0x80000000
#define CMDMASK_CNTL_R 0x08000000
#define CMDMASK_CNTL_W (CMDMASK_WRITE | CMDMASK_CNTL_R)
#define CMDMASK_MANUBLOCK_R 0x04000000
#define CMDMASK_MANUBLOCK_W (CMDMASK_WRITE | CMDMASK_MANUBLOCK_R)
#define CMDMASK_RAMBLOCK_R 0x02000000

#define CMD_CNTLSTATUS (CMDMASK_CNTL_R | 0x0000)
#define CMD_FORCE_UPDATE_E2ROM 0x55FF
#define CMD_CNTL 0x00
#define CMD_SEAL 0x0020
#define CMD_DFSTART 0x61
#define CMD_DFCLASS 0x3E
#define CMD_DFPAGE 0x3F
#define CMD_BLOCK 0x40
#define CMD_CHECKSUM 0x60
#define CMD_BLOCKA (CMDMASK_MANUBLOCK_W | 0x01)
#define CMD_FWDATE1 (CMDMASK_CNTL_R | 0xD0)
#define CMD_FWDATE2 (CMDMASK_CNTL_R | 0xE2)

#define CMD_TEMPER 0x06
#define CMD_VOLTAGE 0x08
#define CMD_CURRENT 0x14
#define CMD_FILRC 0x10
#define CMD_FILFCC 0x12
#define CMD_CYCLECOUNT 0x2A
#define CMD_PASSEDC 0x34
#define CMD_DOD0 0x36
#define CMD_PACKCON 0x3A
#define CMD_GAUGE_ENABLE 0x21
#define CMD_GAUGE_DELAY 500
#define CMD_CELLMODEL (CMDMASK_CNTL_R | 0xE6)
#define CELL_MODEL_COUNT 15
#define BYTE_COUNT_CELL_MODEL 32

#define CMD_GAUGEBLOCK1 (CMDMASK_CNTL_R | 0x00E3)
#define CMD_GAUGEBLOCK2 (CMDMASK_CNTL_R | 0x00E4)
#define CMD_GAUGEBLOCK3 (CMDMASK_CNTL_R | 0x00E5)
#define CMD_GAUGEBLOCK4 (CMDMASK_CNTL_R | 0x00E6)
#define CMD_GAUGEBLOCK5 (CMDMASK_CNTL_R | 0x00E7)
#define CMD_GAUGEBLOCK6 (CMDMASK_RAMBLOCK_R | 0x002B)
#define CMD_CURRENTBLOCK1 (CMDMASK_CNTL_R | 0x00EA)
#define CMD_CURRENTBLOCK2 (CMDMASK_CNTL_R | 0x00EB)
#define CMD_CURRENTBLOCK3 (CMDMASK_CNTL_R | 0x00EC)
#define CMD_CURRENTBLOCK4 (CMDMASK_CNTL_R | 0x00ED)

#define BUF2U16_BG(p) ((u16)(((u16)(u8)((p)[0]) << 8) | (u8)((p)[1])))
#define BUF2U16_LT(p) ((u16)(((u16)(u8)((p)[1]) << 8) | (u8)((p)[0])))
#define BUF2U32_BG(p)                                                                                                  \
	((u32)(((u32)(u8)((p)[0]) << 24) | ((u32)(u8)((p)[1]) << 16) | ((u32)(u8)((p)[2]) << 8) | (u8)((p)[3])))
#define BUF2U32_LT(p)                                                                                                  \
	((u32)(((u32)(u8)((p)[3]) << 24) | ((u32)(u8)((p)[2]) << 16) | ((u32)(u8)((p)[1]) << 8) | (u8)((p)[0])))
#define GAUGEINFO_LEN 32
#define GAUGESTR_LEN 512
#define U64_MAXVALUE 0xFFFFFFFFFFFFFFFF
#define TEMPER_OFFSET 2731

#define IIC_ADDR_OF_2_KERNEL(addr) ((u8)((u8)addr >> 1))

/* file_decode_process */
#define OPERATE_READ 1
#define OPERATE_WRITE 2
#define OPERATE_COMPARE 3
#define OPERATE_WAIT 4

#define ERRORTYPE_NONE 0
#define ERRORTYPE_ALLOC 1
#define ERRORTYPE_LINE 2
#define ERRORTYPE_COMM 3
#define ERRORTYPE_COMPARE 4
#define ERRORTYPE_FINAL_COMPARE 5

/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: data */
#define INDEX_TYPE 0
#define INDEX_ADDR 1
#define INDEX_REG 2
#define INDEX_LENGTH 3
#define INDEX_DATA 4
#define INDEX_WAIT_LENGTH 1
#define INDEX_WAIT_HIGH 2
#define INDEX_WAIT_LOW 3
#define LINELEN_WAIT 4
#define LINELEN_READ 4
#define LINELEN_COMPARE 4
#define LINELEN_WRITE 4
#define FILEDECODE_STRLEN 128
#define COMPARE_RETRY_CNT 2
#define COMPARE_RETRY_WAIT 50
#define BUF_MAX_LENGTH 512

#define E2ROM_DELAY_MS          1500
#define ADDR_PACKCONFIG         0x4000
#define LENGTH_PACKCONFIG       2
#define PACKCONFIG_SLEEP_MASK   0x20
#define CMD_DFCONFIGVERSION     (CMDMASK_CNTL_R | 0x0C)
#define DFCONFIG_CALIED         0xAA
#define ADDR_DEADBAND          0x6B01
#define LENGTH_DEADBAND        1
#define DEFAULT_DEADBAND       3
#define ADDR_PACKLOTCODE        0x3800
#define ADDR_CADCOFFSET        0x6802
#define LENGTH_CADCOFFSET      2

/* fg_gauge_calibrate_board */
#define CMD_TEMPERATURE         0x06
#define MAXTEMPER               (2731 + 500)
#define MINTEMPER               (2731 + 100)
#define ADDR_DFCONFIGVERSION    0x380A
#define LEN_DFCONFIGVERSION      2
#define MAX_CADCOFFSET         120
#define ADDR_CADCRATIO         0x6800
#define LENGTH_CADCRATIO       4
#define INDEX_CADCRATIO        0
#define INDEX_CADCOFFSET       2
#define BASE_CADCRATIO         16384
#define DELAY_CURRENT          1000
#define CALI_MAX_CNT           6
#define CALI_DELAY_MS          1000
#define CALI_MAX_CURRENT       12
#define CALI_DELTA_CURRENT     4
#define CALI_DEADBAND          4

/* fg_gauge_check_cell_model */
#define MODELFLAG_NONE            0
#define MODELFLAG_NEW_SMALL       0x0010
#define MODELFLAG_NEW_LARGE       0x0020
#define MODELFLAG_NEW_EXTREME     0x0040
#define MODELFLAG_NEW_SLIGHT      0x0080
#define TOOLARGE_STARTGRID       0
#define TOOLARGE_ENDGRID         10
#define TOOLARGE_MAXVAULE        3000
#define TOOLARGE_RATIO           1000
#define TOOSMALL_STARTGRID_0     0
#define TOOSMALL_ENDGRID_0       4
#define TOOSMALL_MINVAULE_0      5
#define TOOSMALL_STARTGRID_1     5
#define TOOSMALL_ENDGRID_1       10
#define TOOSMALL_MINVAULE_1      10
#define TOOSMALL_STARTGRID_2     11
#define TOOSMALL_ENDGRID_2       14
#define TOOSMALL_MINVAULE_2      30
#define SLIGHT_STARTGRID       0
#define SLIGHT_ENDGRID         10
#define SLIGHT_GAP             5
#define SLIGHT_RATIO           1000
#define SLIGHT_MAXRATIO        3000
#define SLIGHT_MAXCNT          2
#define EXTREME_STARTGRID_0       0
#define EXTREME_ENDGRID_0         12
#define EXTREME_GAP_0             5
#define EXTREME_RATIO             1000
#define EXTREME_MAXRATIO_0        4500
#define EXTREME_MAXCNT_0          1
#define EXTREME_STARTGRID_1       12
#define EXTREME_ENDGRID_1         14
#define EXTREME_GAP_1             2
#define EXTREME_MAXRATIO_1        8000
#define EXTREME_MAXCNT_1          1
#define EXTREME_STARTGRID_2       12
#define EXTREME_ENDGRID_2         13
#define EXTREME_MAXRATIO_2        1250

/* fg_gauge_restore_cell_model */
#define ADDR_CELLMODEL 0x7B00
#define LENGTH_CELLMODEL 64
#define INDEX_CELLMODEL 0
#define INDEX_XCELLMODEL 32
#define FLAG_XCELLMODEL 0xFFFF
#define STRLEN 200
#define MODELRATIO_BASE    1000
#define MAX_MODEL  0x7FFF
#define MIN_MODEL  0x0F

/* fg_gauge_check_por_soc */
#define MAX_ABS_CURRENT 200
#define CMD_BLOCK3 (CMDMASK_MANUBLOCK_R | 0x03)
#define ADDR_BLOCK3 0x3A40
#define INDEX_POR_FLAG 0
#define LENGTH_POR_FLAG 1
#define POR_FLAG_TRIGGER 0xAA
#define POR_FLAG_DEFAULT 0
#define CMD_CYCLE_MODEL (CMDMASK_RAMBLOCK_R | 0x002B)
#define INDEX_CYCLE_MODEL 28
#define LENGTH_CYCLE_MODEL 2
#define CYCLE_MODEL_DIFFER 20
#define CMD_IAP_DEVICE  0xA0
#define IAP_DEVICE_ID   0x3602

#define MIN_VOLTAGE 2750


struct sh366002_model_track {
	struct mutex track_lock;
	bool uploading;
	oplus_chg_track_trigger *load_trigger;
	struct delayed_work load_trigger_work;
	bool track_init_done;
};

static struct sh366002_model_track fix_cadc_offset;
int sh366002_dbg = 0;
static int track_count;
module_param(sh366002_dbg, int, 0644);
MODULE_PARM_DESC(sh366002_dbg, "debug sh366002");
static s32 print_buffer(char *str, s32 strlen, u8 *buf, s32 buflen);
static int sh366002_track_upload_fix_cadc_info(int old_cadcoffset, int new_cadcoffset,
						int packlot, struct chip_bq27541 *bq_chip);

static s32 __fg_read_byte(struct chip_bq27541 *chip, u8 reg, u8 *val)
{
	s32 ret;

	if (!chip || !chip->client) {
		pr_err("chip or chip->client NULL,return\n");
		return 0;
	}

	if (oplus_is_rf_ftm_mode())
		return 0;

	mutex_lock(&chip->chip_mutex);
	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		pr_err("i2c read byte fail: can't read from reg 0x%02X\n", reg);
		mutex_unlock(&chip->chip_mutex);
		return ret;
	}
	mutex_unlock(&chip->chip_mutex);
	*val = (u8)ret;

	return 0;
}

static s32 __fg_write_byte(struct chip_bq27541 *chip, u8 reg, u8 val)
{
	s32 ret;

	if (!chip || !chip->client) {
		pr_err("chip or chip->client NULL,return\n");
		return 0;
	}

	if (oplus_is_rf_ftm_mode())
		return 0;

	mutex_lock(&chip->chip_mutex);
	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write byte fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		mutex_unlock(&chip->chip_mutex);
		return ret;
	}
	mutex_unlock(&chip->chip_mutex);

	return 0;
}
static s32 __fg_read_word(struct chip_bq27541 *chip, u8 reg, u16 *val)
{
	s32 ret;

	if (!chip || !chip->client) {
		pr_err("chip or chip->client NULL,return\n");
		return 0;
	}

	if (oplus_is_rf_ftm_mode())
		return 0;

	mutex_lock(&chip->chip_mutex);
	ret = i2c_smbus_read_word_data(chip->client, reg);
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
		mutex_unlock(&chip->chip_mutex);
		return ret;
	}
	mutex_unlock(&chip->chip_mutex);
	*val = (u16)ret;

	return 0;
}

static s32 __fg_write_word(struct chip_bq27541 *chip, u8 reg, u16 val)
{
	s32 ret;

	if (!chip || !chip->client) {
		pr_err("chip or chip->client NULL,return\n");
		return 0;
	}

	if (oplus_is_rf_ftm_mode())
		return 0;

	mutex_lock(&chip->chip_mutex);
	ret = i2c_smbus_write_word_data(chip->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X\n", val, reg);
		mutex_unlock(&chip->chip_mutex);
		return ret;
	}
	mutex_unlock(&chip->chip_mutex);

	return 0;
}

static s32 __fg_read_buffer(struct chip_bq27541 *chip, u8 reg, u8 length, u8 *val)
{
	static struct i2c_msg msg[2];
	s32 ret;

	if (!chip || !chip->client || !chip->client->adapter)
		return -ENODEV;

	if (oplus_is_rf_ftm_mode())
		return 0;

	msg[0].addr = chip->client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(u8);
	msg[1].addr = chip->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = val;
	msg[1].len = length;

	mutex_lock(&chip->chip_mutex);
	ret = (s32)i2c_transfer(chip->client->adapter, msg, ARRAY_SIZE(msg));
	mutex_unlock(&chip->chip_mutex);
	return ret;
}

static s32 __fg_write_buffer(struct chip_bq27541 *chip, u8 reg, u8 length, u8 *val)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	s32 ret;

	if (!chip || !chip->client || !chip->client->adapter)
		return -ENODEV;

	if (oplus_is_rf_ftm_mode())
		return 0;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!\n");
		return -1;
	}

	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = reg;
	memcpy(&write_buf[1], val, length);

	msg[0].addr = chip->client->addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	mutex_lock(&chip->chip_mutex);
	ret = i2c_transfer(chip->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", reg);
		mutex_unlock(&chip->chip_mutex);
		return (s32)ret;
	}
	mutex_unlock(&chip->chip_mutex);

	return 0;
}

static s32 fg_read_sbs_word(struct chip_bq27541 *chip, u32 reg, u16 *val)
{
	s32 ret = -1;

	if ((reg & CMDMASK_CNTL_R) == CMDMASK_CNTL_R) {
		mutex_lock(&chip->bq28z610_alt_manufacturer_access);
		ret = __fg_write_word(chip, CMD_CNTL, (u16)reg);
		if (ret < 0) {
			mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
			return ret;
		}

		mdelay(CMD_SBS_DELAY);
		ret = __fg_read_word(chip, CMD_CNTL, val);
		mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
	} else {
		ret = __fg_read_word(chip, (u8)reg, val);
	}

	return ret;
}

static int fg_write_sbs_word(struct chip_bq27541 *chip, u32 reg, u16 val)
{
	int ret;

	ret = __fg_write_word(chip, (u8)reg, val);

	return ret;
}

static s32 fg_read_sbs_word_then_check(struct chip_bq27541 *chip, u32 reg, u16* val)
{
	s32 ret = -1;
	s32 dat0, dat1, dat2;

	ret = fg_read_sbs_word(chip, reg, val);
	dat0 = (ret < 0) ? -1 : *val;
	msleep(CMD_SBS_DELAY);

	ret = fg_read_sbs_word(chip, reg, val);
	msleep(CMD_SBS_DELAY);
	dat1 = (ret < 0) ? -1 : *val;

	ret = fg_read_sbs_word(chip, reg, val);
	msleep(CMD_SBS_DELAY);
	dat2 = (ret < 0) ? -1 : *val;

	if ((dat0 == dat1) && (dat0 != -1)) {
		*val = (u16)dat0;
		ret = 0;
	} else if ((dat0 == dat2) && (dat0 != -1)) {
		*val = (u16)dat0;
		ret = 0;
	} else if ((dat1 == dat2) && (dat1 != -1)) {
		*val = (u16)dat1;
		ret = 0;
	} else {
		ret = -1;
	}

	return ret;
}

static int fg_block_checksum_calculate(u8 *buffer, u8 length)
{
	u8 sum = 0;
	if (length > 32)
		return -1;

	while (length--)
		sum += buffer[length];
	sum = ~sum;
	return (int)((u8)sum);
}

static int fg_read_block(struct chip_bq27541 *chip, u32 reg, u8 startIndex, u8 length, u8 *val)
{
	int ret = -1;
	int i;
	u16 temp16;
	int sum;
	u8 checksum;
	u8 readbuf[DF_PAGE_LEN];

	if ((startIndex >= DF_PAGE_LEN) || (length == 0))
		return -1;

	if (length > DF_PAGE_LEN)
		length = DF_PAGE_LEN;
	memset(val, 0, length);
	memset(readbuf, 0, DF_PAGE_LEN);

	if (startIndex + length >= DF_PAGE_LEN)
		length = DF_PAGE_LEN - startIndex;

	/* mutex_lock(&chip->bq28z610_alt_manufacturer_access); */
	if ((reg & CMDMASK_CNTL_R) == CMDMASK_CNTL_R) {
		ret = __fg_write_word(chip, CMD_CNTL, (u16)reg);
		if (ret < 0) {
			pr_err("TYPE CNTL, write 0x00 fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_buffer(chip, CMD_BLOCK, DF_PAGE_HALF_LEN, readbuf);
		msleep(CMD_SBS_DELAY);
		ret |= __fg_read_buffer(chip, (u8)(CMD_BLOCK + DF_PAGE_HALF_LEN), (u8)(DF_PAGE_LEN - DF_PAGE_HALF_LEN), &readbuf[DF_PAGE_HALF_LEN]);
		if (ret < 0) {
			pr_err("TYPE CNTL, read 0x40 fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		/* check buffer */
		ret = __fg_read_word(chip, CMD_CNTL, &temp16);
		if (ret < 0)
			goto fg_read_block_end;

		checksum = (u8)(temp16 >> 8);
		sum = fg_block_checksum_calculate(readbuf, 32);
		if (sum !=(int)((u8)checksum)) {
			ret = -1;
			pr_err("TYPE CNTL, verify checksum fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		else
			ret = 0;

		memcpy(val, &readbuf[startIndex], length);

	} else if ((reg & CMDMASK_MANUBLOCK_R) == CMDMASK_MANUBLOCK_R) {
		ret = __fg_write_byte(chip, CMD_DFSTART, 0x01);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, write 0x61 fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFPAGE, (u8)reg);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, write 0x3F fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(15);

		ret = __fg_read_buffer(chip, CMD_BLOCK, DF_PAGE_HALF_LEN, readbuf);
		msleep(CMD_SBS_DELAY);
		ret |= __fg_read_buffer(chip, (u8)(CMD_BLOCK + DF_PAGE_HALF_LEN), (u8)(DF_PAGE_LEN - DF_PAGE_HALF_LEN), &readbuf[DF_PAGE_HALF_LEN]);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, read 0x40 fail! command=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		/* check buffer */
		ret = __fg_read_byte(chip, CMD_CHECKSUM, &checksum);
		if (ret < 0)
			goto fg_read_block_end;

		sum = fg_block_checksum_calculate(readbuf, DF_PAGE_LEN);
		if (sum !=(int)((u8)checksum)) {
			ret = -1;
			pr_err("TYPE MANUBLOCK, verify checksum fail! command=0x%08X, cal_sum=0x%08X, read_sum=0x%02X\n", reg, sum, checksum);
			goto fg_read_block_end;
		}
		else
			ret = 0;
		memcpy(val, &readbuf[startIndex], length);

	} else if ((reg & CMDMASK_RAMBLOCK_R) == CMDMASK_RAMBLOCK_R) { /* donot support checksum */
		ret = fg_read_sbs_word(chip, (CMDMASK_CNTL_R | 0xA3), &temp16);
		if (ret < 0) {
			pr_err("fg_read_block write 0xA3 fail! cmd=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		sum = (temp16 >> 8);
		checksum = (u8)((0xFF & temp16) ^ sum ^ 0xA3);

		ret = __fg_write_byte(chip, CMD_DFSTART, 0x03);
		if (ret < 0)  {
			pr_err("write 0x61 fail! cmd=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFCLASS, (u8)reg);
		if (ret < 0)  {
			pr_err("write 0x3E fail! cmd=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFPAGE, checksum);
		if (ret < 0)  {
			pr_err("write 0x3F fail! cmd=0x%08X\n", reg);
			goto fg_read_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_buffer(chip, (u8)(CMD_BLOCK + startIndex), length, val); /* 20230823, Ethan */
		if (ret < 0)  {
			pr_err("read buffer fail! cmd=0x%08X\n", reg);
			goto fg_read_block_end;
		}

		for (i = 0; i < length; i++)
			val[i] = (u8)(val[i] ^ sum);
	} else {
		ret = __fg_read_buffer(chip, reg, length, val);
	}

fg_read_block_end:
	/* mutex_unlock(&chip->bq28z610_alt_manufacturer_access); */

	return ret;
}

static int fg_read_ram_block(struct chip_bq27541 *chip, u32 reg, u8 startIndex, u8 length, u8* val)
{
	int ret;
	int ret0, ret1, ret2;
	u8 str[200];

	u8 dat0[DF_PAGE_LEN], dat1[DF_PAGE_LEN], dat2[DF_PAGE_LEN];
	if ((length <= 0) || (startIndex + length > DF_PAGE_LEN)) /* 20231102, Sinowealth. Bug Fix */
		return -1;

	/* 20231102, Sinowealth. Support all-type block read */
	/*if ((reg & CMDMASK_RamBlock_R) != CMDMASK_RamBlock_R)
		return fg_read_block(client, reg, startIndex, length, val);*/

	ret0 = fg_read_block(chip, reg, startIndex, length, dat0);
	msleep(CMD_SBS_DELAY);
	ret1 = fg_read_block(chip, reg, startIndex, length, dat1);
	msleep(CMD_SBS_DELAY);
	ret2 = fg_read_block(chip, reg, startIndex, length, dat2);
	msleep(CMD_SBS_DELAY);

/* for debug */
	print_buffer(str, 200, dat0, length);
	pr_debug("fg_read_ram_block ret=%d, dat0=%s\r\n", ret0, str);
	print_buffer(str, 200, dat1, length);
	pr_debug("fg_read_ram_block ret=%d, dat1=%s\r\n", ret1, str);
	print_buffer(str, 200, dat2, length);
	pr_debug("fg_read_ram_block ret=%d, dat2=%s\r\n", ret2, str);

	if (!memcmp(dat0, dat1, length) && (ret0 >= 0)) {
		memcpy(val, dat0, length);
		ret = 0;
	} else if (!memcmp(dat0, dat2, length) && (ret0 >= 0)) {
		memcpy(val, dat0, length);
		ret = 0;
	} else if (!memcmp(dat1, dat2, length) && (ret1 >= 0)) {
		memcpy(val, dat1, length);
		ret = 0;
	} else {
		ret = -1;
	}

	return ret;
}

static __maybe_unused int fg_write_block(struct chip_bq27541 *chip, u32 reg, u8 length, u8 *val)
{
	int ret;
	int i;
	u8 sum;
	static u8 write_buffer[DF_PAGE_LEN];

	if ((length > DF_PAGE_LEN) || (length == 0))
		return -1;

	memcpy(val, write_buffer, length);
	i = DF_PAGE_LEN - length;
	if (i > 0)
		memset(write_buffer, 0, i);

	mutex_lock(&chip->bq28z610_alt_manufacturer_access);
	if ((reg & CMDMASK_MANUBLOCK_R) == CMDMASK_MANUBLOCK_R) {
		ret = __fg_write_byte(chip, CMD_DFSTART, 0x01);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, write 0x61 fail! command=0x%08X\n", reg);
			goto fg_write_block_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFPAGE, (u8)reg);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, write 0x3F fail! command=0x%08X\n", reg);
			goto fg_write_block_end;
		}
		msleep(15);

		ret = __fg_write_buffer(chip, CMD_BLOCK, DF_PAGE_LEN, write_buffer);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, read 0x40 fail! command=0x%08X\n", reg);
			goto fg_write_block_end;
		}
		msleep(CMD_SBS_DELAY);

		sum = fg_block_checksum_calculate(write_buffer, DF_PAGE_LEN);
		ret = __fg_write_byte(chip, CMD_CHECKSUM, (u8)sum);
		if (ret < 0) {
			pr_err("TYPE MANUBLOCK, write 0x60 fail! command=0x%08X\n", reg);
			goto fg_write_block_end;
		}
		msleep(15);
	} else {
		ret = __fg_write_buffer(chip, (u8)reg, length, val);
	}
fg_write_block_end:
	mutex_unlock(&chip->bq28z610_alt_manufacturer_access);

	return ret;
}

static __maybe_unused int fg_read_dataflash(struct chip_bq27541 *chip, s32 address, s32 length, u8 *val)
{
	int ret = -1;

	u8 sum_read;
	u8 buf[DF_PAGE_LEN];
	u8 classID = (u8)(address >> 8);
	u8 pageNo;
	s32 sum;
	s32 pageLen = 0;
	s32 valIndex = 0;
	address &= 0xFF;

	if (length <= 0)
		return -1;

	mutex_lock(&chip->bq28z610_alt_manufacturer_access);

	while (length > 0) {
		pageLen = DF_PAGE_LEN - (address % DF_PAGE_LEN);
		if (pageLen > length)
			pageLen = length;
		pr_debug("fg_read_dataflash: pageLen=%u, class=0x%02X, index=0x%02X\n", pageLen, classID, address);

		ret = __fg_write_byte(chip, CMD_DFSTART, 0x00);
		if (ret < 0) {
			pr_err("write 0x61 fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFCLASS, classID);
		if (ret < 0) {
			pr_err("write 0x3E fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		pageNo = (u8)(address / DF_PAGE_LEN); /* 20231102, Sinowealth. Read Check */
		ret = __fg_write_byte(chip, CMD_DFPAGE, pageNo);
		if (ret < 0) {
			pr_err("write 0x3F fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_read_dataflash_end;
		}
		msleep(15);

		/* 20231102, Sinowealth. Read Check */
		ret = __fg_read_byte(chip, CMD_DFSTART, &sum_read);
		if ((ret < 0) || (sum_read != 0)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x61 fail! target=0x%02X, read=0x%02X\n", 0, sum_read);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_byte(chip, CMD_DFCLASS, &sum_read);
		if ((ret < 0) || (sum_read != classID)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x3E fail! target=0x%02X, read=0x%02X\n", classID, sum_read);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_byte(chip, CMD_DFPAGE, &sum_read);
		if ((ret < 0) || (sum_read != pageNo)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x3F fail! target=0x%02X, read=0x%02X\n", pageNo, sum_read);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		/* for QualComm Host, cannot write over 16byte buffer!! */
		/* ret = __fg_read_buffer(chip, CMD_BLOCK, DF_PAGE_LEN, buf); */
		ret = __fg_read_buffer(chip, CMD_BLOCK, DF_PAGE_HALF_LEN, buf);
		msleep(CMD_SBS_DELAY);
		ret |= __fg_read_buffer(chip, (u8)(CMD_BLOCK + DF_PAGE_HALF_LEN), (u8)(DF_PAGE_LEN - DF_PAGE_HALF_LEN), &buf[DF_PAGE_HALF_LEN]);
		if (ret < 0) {
			pr_err("read 0x40 fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_read_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		sum = fg_block_checksum_calculate(buf, DF_PAGE_LEN);

		ret = __fg_read_byte(chip, CMD_CHECKSUM, &sum_read);
		if ((ret < 0) || (sum != (s32)((u8)sum_read))) {
			ret = -1;
			pr_err("read 0x60 fail! class=0x%02X, index=0x%02X, sum=0x%02X, readsum=0x%02X\n", classID,
			       address, sum, sum_read);
			goto fg_read_dataflash_end;
		}

		memcpy(&val[valIndex], &buf[address % DF_PAGE_LEN], pageLen);

		valIndex += pageLen;
		address += pageLen;
		length -= pageLen;
		ret = 0;
	}

fg_read_dataflash_end:
	mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
	return ret;
}

static __maybe_unused int fg_write_dataflash(struct chip_bq27541 *chip, s32 address, s32 length, u8 *val)
{
	int ret = -1;
	int i = 0;

	s32 sum;
	u8 buf[DF_PAGE_LEN];
	u8 classID = (u8)(address >> 8);
	u8 pageNo;
	u8 sum_read;
	s32 pageLen = 0;
	s32 valIndex = 0;
	address &= 0xFF;

	if (length <= 0)
		return -1;

	mutex_lock(&chip->bq28z610_alt_manufacturer_access);

	while (length > 0) {
		pageLen = DF_PAGE_LEN - (address % DF_PAGE_LEN);
		if (pageLen > length)
			pageLen = length;
		pr_debug("fg_write_dataflash: pageLen=%u, class=0x%02X, index=0x%02X\n", pageLen, classID, address);

		ret = __fg_write_byte(chip, CMD_DFSTART, 0x00);
		if (ret < 0) {
			pr_err("write 0x61 fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_write_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_write_byte(chip, CMD_DFCLASS, classID);
		if (ret < 0) {
			pr_err("write 0x3E fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_write_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		pageNo = (u8)(address / DF_PAGE_LEN); /* 20231102, Sinowealth. Read Check */
		ret = __fg_write_byte(chip, CMD_DFPAGE, pageNo);
		if (ret < 0) {
			pr_err("write 0x3F fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_write_dataflash_end;
		}
		msleep(15);

		/* 20231102, Sinowealth. Read Check */
		ret = __fg_read_byte(chip, CMD_DFSTART, &sum_read);
		if ((ret < 0) || (sum_read != 0)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x61 fail! target=0x%02X, read=0x%02X\n", 0, sum_read);
			goto fg_write_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_byte(chip, CMD_DFCLASS, &sum_read);
		if ((ret < 0) || (sum_read != classID)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x3E fail! target=0x%02X, read=0x%02X\n", classID, sum_read);
			goto fg_write_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = __fg_read_byte(chip, CMD_DFPAGE, &sum_read);
		if ((ret < 0) || (sum_read != pageNo)) {
			ret = -1;
			pr_err("fg_read_dataflash: chekc 0x3F fail! target=0x%02X, read=0x%02X\n", pageNo, sum_read);
			goto fg_write_dataflash_end;
		}
		msleep(CMD_SBS_DELAY);

		i = address % DF_PAGE_LEN;
		if ((i != 0) || (pageLen != DF_PAGE_LEN)) { /* fill buffer */
			ret = __fg_read_buffer(chip, CMD_BLOCK, DF_PAGE_HALF_LEN, buf);
			msleep(CMD_SBS_DELAY);
			ret |= __fg_read_buffer(chip, (u8)(CMD_BLOCK + DF_PAGE_HALF_LEN), (u8)(DF_PAGE_LEN - DF_PAGE_HALF_LEN), &buf[DF_PAGE_HALF_LEN]);
			if (ret < 0) {
				pr_err("read 0x40 fail! class=0x%02X, index=0x%02X\r\n", classID, address);
				goto fg_write_dataflash_end;
			}
		}
		memcpy(&buf[i], &val[valIndex], pageLen); /* fill buffer */

		/* for QualComm Host, cannot write over 16byte buffer!! */
		/* ret = __fg_write_buffer(chip, CMD_BLOCK, DF_PAGE_LEN, buf); */
		ret = __fg_write_buffer(chip, CMD_BLOCK, DF_PAGE_HALF_LEN, buf);
		for (i = DF_PAGE_HALF_LEN; i < DF_PAGE_LEN; i++) {
			msleep(CMD_SBS_DELAY);
			ret |= __fg_write_byte(chip, (u8)(CMD_BLOCK + i), buf[i]);
		}
		if (ret < 0) {
			pr_err("write 0x40 fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_write_dataflash_end;
		}

		sum = fg_block_checksum_calculate(buf, DF_PAGE_LEN);

		ret = __fg_write_byte(chip, CMD_CHECKSUM, (u8)sum);
		if (ret < 0) {
			pr_err("write 0x60 fail! class=0x%02X, index=0x%02X\n", classID, address);
			goto fg_write_dataflash_end;
		}
		msleep(100);

		valIndex += pageLen;
		address += pageLen;
		length -= pageLen;
		ret = 0;
	}

fg_write_dataflash_end:
	mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
	return ret;
}

static s32 print_buffer(char *str, s32 strlen, u8 *buf, s32 buflen)
{
#define PRINT_BUFFER_FORMAT_LEN 3
	s32 i, j;

	if ((strlen <= 0) || (buflen <= 0))
		return -1;

	memset(str, 0, strlen * sizeof(char));

	j = min(buflen, strlen / PRINT_BUFFER_FORMAT_LEN);
	if (j * PRINT_BUFFER_FORMAT_LEN >= strlen) {
		chg_err("buflen is more than max\n");
		return -1;
	}

	for (i = 0; i < j; i++) {
		sprintf(&str[i * PRINT_BUFFER_FORMAT_LEN], "%02X ", buf[i]);
	}

	return i * PRINT_BUFFER_FORMAT_LEN;
}

struct sh_decoder {
	u8 addr;
	u8 reg;
	u8 length;
	u8 buf_first_val;
};

static s32 fg_decode_iic_read(struct chip_bq27541 *chip, struct sh_decoder *decoder, u8 *pBuf)
{
	static struct i2c_msg msg[2];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	s32 ret;

	if (!chip || !chip->client || !chip->client->adapter)
		return -ENODEV;

	if (oplus_is_rf_ftm_mode())
		return 0;

	mutex_lock(&chip->chip_mutex);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = &(decoder->reg);
	msg[0].len = sizeof(u8);
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = pBuf;
	msg[1].len = decoder->length;
	ret = (s32)i2c_transfer(chip->client->adapter, msg, ARRAY_SIZE(msg));

	mutex_unlock(&chip->chip_mutex);
	return ret;
}

static s32 fg_decode_iic_write(struct chip_bq27541 *chip, struct sh_decoder *decoder)
{
	static struct i2c_msg msg[1];
	static u8 write_buf[WRITE_BUF_MAX_LEN];
	u8 addr = IIC_ADDR_OF_2_KERNEL(decoder->addr);
	u8 length = decoder->length;
	s32 ret;

	if (!chip || !chip->client || !chip->client->adapter)
		return -ENODEV;

	if (oplus_is_rf_ftm_mode())
		return 0;

	if ((length <= 0) || (length + 1 >= WRITE_BUF_MAX_LEN)) {
		pr_err("i2c write buffer fail: length invalid!\n");
		return -1;
	}

	mutex_lock(&chip->chip_mutex);
	memset(write_buf, 0, WRITE_BUF_MAX_LEN * sizeof(u8));
	write_buf[0] = decoder->reg;
	memcpy(&write_buf[1], &(decoder->buf_first_val), length);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].buf = write_buf;
	msg[0].len = sizeof(u8) * (length + 1);

	ret = i2c_transfer(chip->client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		pr_err("i2c write buffer fail: can't write reg 0x%02X\n", decoder->reg);
	}

	mutex_unlock(&chip->chip_mutex);
	return (ret < 0) ? ret : 0;
}

static s32 fg_soc_calculate(s32 remCap, s32 fullCap)
{
	s32 soc = 0;
	if ((remCap < 0) || (fullCap <= 0))
		return 0;
	remCap *= 100;
	soc = remCap / fullCap;
	if ((remCap % fullCap) != 0)
		soc++;
	if (soc > 100)
		soc = 100;
	return soc;
}

s32 sh366002_read_gaugeinfo_block(struct chip_bq27541 *chip)
{
	static u8 buf[GAUGEINFO_LEN];
	static char str[GAUGESTR_LEN];
	int i, j = 0;
	int k, current_index;
	int ret;
	s16 temp1, temp2;
	u16 data;
	u64 jiffies_now;

	if (chip->device_type != DEVICE_ZY0602)
		return 0;

	if (!chip->dump_sh366002_block && !(sh366002_dbg & DUMP_SH366002_BLOCK))
		return 0;

	if (oplus_is_rf_ftm_mode())
		return 0;

	jiffies_now = get_jiffies_64();
	chip->log_last_update_tick = jiffies_now;

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);

	ret = fg_read_sbs_word(chip, CMD_CNTLSTATUS, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_CNTLSTATUS, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "ControlStatus=0x%04X, ", data);

	ret = fg_read_sbs_word(chip, CMD_TEMPER, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_TEMPER, ret = %d\n", ret);
		return ret;
	}
	data -= TEMPER_OFFSET;
	i += sprintf(&str[i], "Temper=%d, ", (s16)data);

	ret = fg_read_sbs_word(chip, CMD_VOLTAGE, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_VOLTAGE, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "VCell=%d, ", (s16)data);

	ret = fg_read_sbs_word(chip, CMD_CURRENT, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_CURRENT, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "Current=%d, ", (s16)data);

	ret = fg_read_sbs_word(chip, CMD_CYCLECOUNT, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_CYCLECOUNT, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "CycleCount=%u, ", data);

	ret = fg_read_sbs_word(chip, CMD_PASSEDC, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_PASSEDC, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "PassedCharge=%d, ", (s16)data);

	ret = fg_read_sbs_word(chip, CMD_DOD0, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_DOD0, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "DOD0=%d, ", (s16)data);

	ret = fg_read_sbs_word(chip, CMD_PACKCON, &data);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_PACKCON, ret = %d\n", ret);
		return ret;
	}
	i += sprintf(&str[i], "PackConfig=0x%04X, ", data);
	j = max(i, j);
	pr_err("SH366002_GaugeLog: SBS_Info is %s", str);

	/* Gauge Block 1 */
	ret = fg_read_block(chip, CMD_GAUGEBLOCK1, 0, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK1, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
	i += sprintf(&str[i], "GaugeStatus=0x%02X, ", buf[0]);
	i += sprintf(&str[i], "QmaxFlag=0x%02X, ", buf[1]);
	i += sprintf(&str[i], "UpdateStatus=0x%02X, ", buf[2]);
	i += sprintf(&str[i], "GaugeUpdateIndex=0x%02X, ", buf[3]);
	i += sprintf(&str[i], "GaugeUpdateStatus=0x%04X, ", BUF2U16_LT(&buf[4]));
	i += sprintf(&str[i], "EODLoad=%d, ", (s16)BUF2U16_LT(&buf[6]));
	i += sprintf(&str[i], "CellRatio=%d, ", BUF2U16_LT(&buf[8]));
	i += sprintf(&str[i], "DODCAL=%d, ", (s16)BUF2U16_LT(&buf[10]));
	i += sprintf(&str[i], "DODEOC=%d, ", (s16)BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "DODEOD=%d, ", (s16)BUF2U16_LT(&buf[14]));
	i += sprintf(&str[i], "HighTimer=%u, ", BUF2U16_LT(&buf[16]));
	i += sprintf(&str[i], "LowTimer=%u, ", BUF2U16_LT(&buf[18]));
	i += sprintf(&str[i], "OCVFGTimer=%u, ", BUF2U16_LT(&buf[20]));
	i += sprintf(&str[i], "PrevRC=%u, ", BUF2U16_LT(&buf[22]));
	i += sprintf(&str[i], "CoulombOffset=%u, ", BUF2U16_LT(&buf[24]));
	i += sprintf(&str[i], "T_out=%d, ", (s16)BUF2U16_LT(&buf[26]));
	i += sprintf(&str[i], "EOCdelta_T=%d, ", (s16)BUF2U16_LT(&buf[28]));
	i += sprintf(&str[i], "ThermalT=%d, ", (s16)BUF2U16_LT(&buf[30]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: CMD_GAUGEINFO is %s", str);

	/* Gauge Block 2 */
	ret = fg_read_block(chip, CMD_GAUGEBLOCK2, 0, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK2, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
	i += sprintf(&str[i], "ChargingVoltage=%d, ", (s16)BUF2U16_LT(&buf[0]));
	i += sprintf(&str[i], "ChargingCurrent=%d, ", (s16)BUF2U16_LT(&buf[2]));
	i += sprintf(&str[i], "TaperCurrent=%d, ", (s16)BUF2U16_LT(&buf[4]));
	i += sprintf(&str[i], "VOLatEOC=%d, ", (s16)BUF2U16_LT(&buf[6]));
	i += sprintf(&str[i], "CURatEOC=%d, ", (s16)BUF2U16_LT(&buf[8]));
	i += sprintf(&str[i], "GaugeConfig=0x%04X, ", BUF2U16_LT(&buf[10]));
	i += sprintf(&str[i], "workstate2=0x%04X, ", BUF2U16_LT(&buf[12]));
	i += sprintf(&str[i], "UpdateStatus=0x%02X, ", buf[14]);
	i += sprintf(&str[i], "OCVRaUpdate=0x%02X, ", buf[15]);
	i += sprintf(&str[i], "Qmax1=%d, ", (s16)BUF2U16_LT(&buf[16]));
	i += sprintf(&str[i], "DODQmax=%d, ", (s16)BUF2U16_LT(&buf[18]));
	i += sprintf(&str[i], "QmaxPass=%d, ", (s16)BUF2U16_LT(&buf[20]));
	i += sprintf(&str[i], "PassedEnergy=%d, ", (s16)BUF2U16_LT(&buf[22]));
	i += sprintf(&str[i], "Qstart=%d, ", (s16)BUF2U16_LT(&buf[24]));
	i += sprintf(&str[i], "Estart=%d, ", (s16)BUF2U16_LT(&buf[26]));
	i += sprintf(&str[i], "DODGridIndex=0x%02X, ", buf[28]);
	i += sprintf(&str[i], "RCFilterFlag=0x%02X, ", buf[29]);
	i += sprintf(&str[i], "DeltaCap_Timer=%u, ", BUF2U16_LT(&buf[30]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: GAUGEBLOCK2 is %s", str);

	/* Gauge Block 3 */
	ret = fg_read_block(chip, CMD_GAUGEBLOCK3, 0, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK3, ret = %d\n", ret);
		return ret;
	}
	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
	temp1 = (s16)BUF2U16_LT(&buf[0]);
	temp2 = (s16)BUF2U16_LT(&buf[2]);
	i += sprintf(&str[i], "RemCap=%d, ", temp1);
	i += sprintf(&str[i], "FullCap=%d, ", temp2);
	temp1 = fg_soc_calculate(temp1, temp2);
	i += sprintf(&str[i], "RSOC=%d, ", temp1);
	temp1 = (s16)BUF2U16_LT(&buf[4]);
	temp2 = (s16)BUF2U16_LT(&buf[6]);
	i += sprintf(&str[i], "FilRC=%d, ", temp1);
	i += sprintf(&str[i], "FilFCC=%d, ", temp2);
	temp1 = fg_soc_calculate(temp1, temp2);
	i += sprintf(&str[i], "FSOC=%d, ", temp1);
	temp1 = (s16)BUF2U16_LT(&buf[8]);
	temp2 = (s16)BUF2U16_LT(&buf[10]);
	i += sprintf(&str[i], "RemEgy=%d, ", temp1);
	i += sprintf(&str[i], "FullEgy=%d, ", temp2);
	temp1 = fg_soc_calculate(temp1, temp2);
	i += sprintf(&str[i], "RSOCW=%d, ", temp1);
	temp1 = (s16)BUF2U16_LT(&buf[12]);
	temp2 = (s16)BUF2U16_LT(&buf[14]);
	i += sprintf(&str[i], "FilRE=%d, ", temp1);
	i += sprintf(&str[i], "FilFCE=%d, ", temp2);
	temp1 = fg_soc_calculate(temp1, temp2);
	i += sprintf(&str[i], "FSOCW=%d, ", temp1);

	i += sprintf(&str[i], "RemCapEQU=%d, ", (s16)BUF2U16_LT(&buf[16]));
	i += sprintf(&str[i], "FullCalEQU=%d, ", (s16)BUF2U16_LT(&buf[18]));
	i += sprintf(&str[i], "IdealFCC=%d, ", (s16)BUF2U16_LT(&buf[20]));
	i += sprintf(&str[i], "IIRCUR=%d, ", (s16)BUF2U16_LT(&buf[22]));
	i += sprintf(&str[i], "RemCapRAW=%d, ", (s16)BUF2U16_LT(&buf[24]));
	i += sprintf(&str[i], "FullCapRAW=%d, ", (s16)BUF2U16_LT(&buf[26]));
	i += sprintf(&str[i], "RemEgyRAW=%d, ", (s16)BUF2U16_LT(&buf[28]));
	i += sprintf(&str[i], "FullEgyRAW=%d, ", (s16)BUF2U16_LT(&buf[30]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: GAUGEBLOCK3 is %s", str);

	/* Gauge Block 4 */
	ret = fg_read_block(chip, CMD_GAUGEBLOCK4, 0, GAUGEINFO_LEN, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK4, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
	i += sprintf(&str[i], "FusionModel=");
	for (ret = 0; ret < 15; ret++)
		i += sprintf(&str[i], "0x%04X ", BUF2U16_LT(&buf[ret * 2]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: GAUGEBLOCK4 is %s", str);

	/* Gauge Block 5 */
	ret = fg_read_block(chip, CMD_GAUGEBLOCK5, 0, 15, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK5, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
	i += sprintf(&str[i], "FC_Lock_DSG_Timer=%u, ", BUF2U16_LT(&buf[10]));
	i += sprintf(&str[i], "FC_Lock_Relax_Timer=%u, ", buf[12]);
	i += sprintf(&str[i], "FlashUpdateVoltage=%d, ", (s16)BUF2U16_LT(&buf[13]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: GAUGEBLOCK5 is %s", str);

	/* Gauge Block 6 */
	ret = fg_read_ram_block(chip, CMD_GAUGEBLOCK6, 26, 6, buf);
	if (ret < 0) {
		pr_err("SH366002_GaugeLog: could not read CMD_GAUGEBLOCK6, ret = %d\n", ret);
		return ret;
	}

	memset(str, 0, GAUGESTR_LEN);
	i = 0;
	i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);  /* In case print twice */
	i += sprintf(&str[i], "CycleCount_Qmax=%u, ", BUF2U16_BG(&buf[0]));
	i += sprintf(&str[i], "CycleCount_FusionModel=%u, ", BUF2U16_BG(&buf[2]));
	i += sprintf(&str[i], "CycleCount_Relax=%u, ", BUF2U16_BG(&buf[4]));
	j = max(i, j);
	pr_err("SH366002_GaugeLog: GAUGEBLOCK6 is %s", str);

	if ((chip->dump_sh366002_block & DUMP_SH366002_CURRENT_BLOCK) || (sh366002_dbg & DUMP_SH366002_CURRENT_BLOCK)) {
		/* Current Block 1 */
		current_index = 0;
		ret = fg_read_block(chip, CMD_CURRENTBLOCK1, 0, 32, buf);
		if (ret < 0) {
			pr_err("SH366002_GaugeLog: could not read CMD_CURRENTBLOCK1, ret = %d\n", ret);
			return ret;
		}

		memset(str, 0, GAUGESTR_LEN);
		i = 0;
		i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
		for (k = 0; k < 16; k++) {
			i += sprintf(&str[i], "Current%d=%d, ", current_index++, (s16)BUF2U16_LT(&buf[k * 2]));
		}
		j = max(i, j);
		pr_err("SH366002_GaugeLog: CURRENTBLOCK1 is %s", str);

		/* Current Block 2 */
		ret = fg_read_block(chip, CMD_CURRENTBLOCK2, 0, 32, buf);
		if (ret < 0) {
			pr_err("SH366002_GaugeLog: could not read CMD_CURRENTBLOCK2, ret = %d\n", ret);
			return ret;
		}

		memset(str, 0, GAUGESTR_LEN);
		i = 0;
		i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
		for (k = 0; k < 16; k++) {
			i += sprintf(&str[i], "Current%d=%d, ", current_index++, (s16)BUF2U16_LT(&buf[k * 2]));
		}
		j = max(i, j);
		pr_err("SH366002_GaugeLog: CURRENTBLOCK2 is %s", str);

		/* Current Block 3 */
		ret = fg_read_block(chip, CMD_CURRENTBLOCK3, 0, 32, buf);
		if (ret < 0) {
			pr_err("SH366002_GaugeLog: could not read CMD_CURRENTBLOCK3, ret = %d\n", ret);
			return ret;
		}

		memset(str, 0, GAUGESTR_LEN);
		i = 0;
		i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
		for (k = 0; k < 16; k++) {
			i += sprintf(&str[i], "Current%d=%d, ", current_index++, (s16)BUF2U16_LT(&buf[k * 2]));
		}
		j = max(i, j);
		pr_err("SH366002_GaugeLog: CURRENTBLOCK3 is %s", str);

		/* Current Block 4 */
		ret = fg_read_block(chip, CMD_CURRENTBLOCK4, 0, 32, buf);
		if (ret < 0) {
			pr_err("SH366002_GaugeLog: could not read CMD_CURRENTBLOCK4, ret = %d\n", ret);
			return ret;
		}

		memset(str, 0, GAUGESTR_LEN);
		i = 0;
		i += sprintf(&str[i], "Tick=%d, ", (u32)jiffies_now);
		for (k = 0; k < 16; k++) {
			i += sprintf(&str[i], "Current%d=%d, ", current_index++, (s16)BUF2U16_LT(&buf[k * 2]));
		}
		j = max(i, j);
		pr_err("SH366002_GaugeLog: CURRENTBLOCK4 is %s", str);
	}
	ret = 0;

	return ret;
}

static s32 fg_gauge_unseal(struct chip_bq27541 *chip)
{
	s32 ret;
	s32 i;
	u16 cntl_status;

	for (i = 0; i < 5; i++) {
		ret = fg_write_sbs_word(chip, CMD_CNTL, (u16)CMD_UNSEALKEY);
		if (ret < 0)
			goto fg_gauge_unseal_End;
		msleep(CMD_SBS_DELAY);

		ret = fg_write_sbs_word(chip, CMD_CNTL, (u16)(CMD_UNSEALKEY >> 16));
		if (ret < 0)
			goto fg_gauge_unseal_End;
		msleep(CMD_SBS_DELAY);

		ret = fg_write_sbs_word(chip, CMD_CNTL, (u16)CMD_FULLKEY);
		if (ret < 0)
			goto fg_gauge_unseal_End;
		msleep(CMD_SBS_DELAY);

		ret = fg_write_sbs_word(chip, CMD_CNTL, (u16)(CMD_FULLKEY >> 16));
		if (ret < 0)
			goto fg_gauge_unseal_End;
		msleep(CMD_SBS_DELAY);

		ret = fg_read_sbs_word(chip, CMD_CNTLSTATUS, &cntl_status);
		if (ret < 0)
			goto fg_gauge_unseal_End;
		msleep(CMD_SBS_DELAY);

		if ((cntl_status & CMD_CNTLSTATUS_SEAL) == 0)
			break;
		msleep(CMD_SBS_DELAY);
	}

	ret = (i < 5) ? 0 : -1;
fg_gauge_unseal_End:
	return ret;
}

static s32 fg_gauge_seal(struct chip_bq27541 *chip)
{
	return fg_write_sbs_word(chip, CMD_CNTL, CMD_SEAL);
}

static s32 Check_Chip_Version(struct chip_bq27541 *chip)
{
	struct device *dev = &chip->client->dev;
	struct device_node *np = dev->of_node;
	s32 ret = CHECK_VERSION_ERR;
	u32 chip_afi, dtsi_afi;
	u32 chip_fw, dtsi_fw;
	u16 device_id, voltage;
	u8 chip_manu;
	u8 buffer[DF_PAGE_LEN];
	u16 temp16;

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("Check_Chip_Version: Cannot find child node \"battery_params\"\n");
		return CHECK_VERSION_ERR;
	}

	if (of_property_read_u32(np, "version_fw", &dtsi_fw) != 0)
		dtsi_fw = 0xFFFFFFFF;

	if (of_property_read_u32(np, "version_afi", &dtsi_afi) != 0)
		dtsi_afi = 0xFFFFFFFF;

	pr_err("Check_Chip_Version: fw_date=0x%08X, afi_date=0x%08X, afi_manu=0x%02X\n", dtsi_fw, dtsi_afi,
	       AFI_MANU_SINOWEALTH);

	ret = fg_read_sbs_word(chip, CMD_IAP_DEVICE, &device_id);
	if (ret < 0) {
		pr_err("Check_Chip_Version Error: cannot read IAP_DEVICE!");
		goto Check_Chip_Version_End;
	}
	if (device_id == IAP_DEVICE_ID) {
		pr_err("Check_Chip_Version: ic in iap mode! must update afi!!");
		ret = CHECK_VERSION_AFI;
		goto Check_Chip_Version_End;
	}

	fg_gauge_unseal(chip);
	if (fg_read_sbs_word(chip, CMD_FWDATE1, &temp16) < 0) {
		pr_err("Check_Chip_Version Error: cannot read CNTL 0xD0!");
		goto Check_Chip_Version_End;
	}
	chip_fw = temp16 * 0x10000;
	msleep(5);

	if (fg_read_sbs_word(chip, CMD_FWDATE2, &temp16) < 0) {
		pr_err("Check_Chip_Version Error: cannot read CNTL 0xE2!");
		goto Check_Chip_Version_End;
	}
	chip_fw += temp16;
	msleep(5);

	ret = fg_read_sbs_word(chip, CMD_VOLTAGE, &voltage);
	if (ret < 0) {
		pr_err("Check_Chip_Version Error: cannot read voltage!");
		goto Check_Chip_Version_End;
	}
	if (voltage <= MIN_VOLTAGE) {
		pr_err("Check_Chip_Version: voltage is %d, too low to update afi!", voltage);
		ret = CHECK_VERSION_OK;
		goto Check_Chip_Version_End;
	}

	ret = fg_read_block(chip, CMD_BLOCKA, 0, DF_PAGE_LEN, buffer);
	if (ret < 0) {
		pr_err("Check_Chip_Version Error: cannot read block a!");
		goto Check_Chip_Version_End;
	}

	ret = CHECK_VERSION_OK;
	chip_afi = BUF2U32_BG(&buffer[AFI_DATE_BLOCKA_INDEX]);
	chip_manu = buffer[AFI_MANU_BLOCKA_INDEX];
	if ((dtsi_fw != chip_fw) && (dtsi_fw != 0xFFFFFFFF))
		ret |= CHECK_VERSION_FW;

	if (dtsi_afi != 0xFFFFFFFF) {
		if ((chip_afi != dtsi_afi) || (chip_manu != AFI_MANU_SINOWEALTH))
			ret |= CHECK_VERSION_AFI;
	}

	pr_err("Check_Chip_Version: chip_fw=0x%08X, chip_afi_date=0x%08X, chip_afi_manu=0x%02X, ret=%d\n", chip_fw,
	       chip_afi, chip_manu, ret);

Check_Chip_Version_End:
	fg_gauge_seal(chip);
	return ret;
}

int file_decode_process(struct chip_bq27541 *chip, char *profile_name)
{
	struct device *dev = &chip->client->dev;
	struct device_node *np = dev->of_node;
	u8 *pBuf = NULL;
	u8 *pBuf_Read = NULL;
	char strDebug[FILEDECODE_STRLEN];
	int buflen;
	int wait_ms;
	int i, j;
	int line_length;
	int result = -1;
	int retry;
	struct sh_decoder *ptr_decoder;

	pr_err("file_decode_process: start\n");

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_err("file_decode_process: Cannot find child node \"battery_params\"");
		return -EINVAL;
	}

	buflen = of_property_count_u8_elems(np, profile_name);
	pr_err("file_decode_process: ele_len=%d, key=%s\n", buflen, profile_name);

	pBuf = (u8 *)devm_kzalloc(dev, buflen, 0);
	pBuf_Read = (u8 *)devm_kzalloc(dev, BUF_MAX_LENGTH, 0);

	if ((pBuf == NULL) || (pBuf_Read == NULL)) {
		result = ERRORTYPE_ALLOC;
		pr_err("file_decode_process: kzalloc error\n");
		goto main_process_error;
	}

	result = of_property_read_u8_array(np, profile_name, pBuf, buflen);
	if (result) {
		pr_err("file_decode_process: read dts fail %s\n", profile_name);
		goto main_process_error;
	}
	print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf, 32);
	pr_err("file_decode_process: first data=%s\n", strDebug);

	i = 0;
	j = 0;
	while (i < buflen) {
		/* delay: b0: operate, b1: 2, b2-b3: time, big-endian */
		/* other: b0: operate, b1: TWIADR, b2: reg, b3: data_length, b4...end: item */
		if (pBuf[i + INDEX_TYPE] == OPERATE_WAIT) {
			wait_ms = ((int)pBuf[i + INDEX_WAIT_HIGH] * 256) + pBuf[i + INDEX_WAIT_LOW];

			if (pBuf[i + INDEX_WAIT_LENGTH] == 2) {
				msleep(wait_ms);
				i += LINELEN_WAIT;
			} else {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process wait error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_READ) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			if (fg_decode_iic_read(chip, (struct sh_decoder *)&pBuf[i + INDEX_ADDR], pBuf_Read) < 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process read error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_READ;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_COMPARE) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			ptr_decoder = (struct sh_decoder *)&pBuf[i + INDEX_ADDR];
			for (retry = 0; retry < COMPARE_RETRY_CNT; retry++) {
				if (fg_decode_iic_read(chip, ptr_decoder, pBuf_Read) < 0) {
					print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE],
						     32);
					pr_err("file_decode_process compare_read error! index=%d, str=%s\n", i,
					       strDebug);
					result = ERRORTYPE_COMM;
					goto file_decode_process_compare_loop_end;
				}

				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, line_length);
				pr_debug("file_decode_process loop compare: IC read=%s\n", strDebug);

				result = 0;
				for (j = 0; j < line_length; j++) {
					if (pBuf[INDEX_DATA + i + j] != pBuf_Read[j]) {
						result = ERRORTYPE_COMPARE;
						break;
					}
				}

				if (result == 0)
					break;

				/* compare fail */
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process compare error! index=%d, retry=%d, host=%s\n", i, retry,
				       strDebug);
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, pBuf_Read, 32);
				pr_err("ic=%s\n", strDebug);

			file_decode_process_compare_loop_end:
				msleep(COMPARE_RETRY_WAIT);
			}

			if (retry >= COMPARE_RETRY_CNT) {
				result = ERRORTYPE_COMPARE;
				goto main_process_error;
			}

			i += LINELEN_COMPARE + line_length;
		} else if (pBuf[i + INDEX_TYPE] == OPERATE_WRITE) {
			line_length = pBuf[i + INDEX_LENGTH];
			if (line_length <= 0) {
				result = ERRORTYPE_LINE;
				goto main_process_error;
			}

			ptr_decoder = (struct sh_decoder *)&pBuf[i + INDEX_ADDR];
			if (fg_decode_iic_write(chip, ptr_decoder) != 0) {
				print_buffer(strDebug, sizeof(char) * FILEDECODE_STRLEN, &pBuf[i + INDEX_TYPE], 32);
				pr_err("file_decode_process write error! index=%d, str=%s\n", i, strDebug);
				result = ERRORTYPE_COMM;
				goto main_process_error;
			}

			i += LINELEN_WRITE + line_length;
		} else {
			result = ERRORTYPE_LINE;
			goto main_process_error;
		}
	}
	result = ERRORTYPE_NONE;

main_process_error:
	if (pBuf)
		devm_kfree(dev, pBuf);
	if (pBuf_Read)
		devm_kfree(dev, pBuf_Read);
	pr_err("file_decode_process end: result=%d\n", result);
	return result;
}

s32 fg_gauge_enable_sleep_mode(struct chip_bq27541 *chip) /* 20220625, Ethan */
{
	s32 ret;
	u16 temp16;
	u8 buf0[2], buf1[2];
	memset(buf0, 0, 2 * sizeof(u8));
	memset(buf1, 0, 2 * sizeof(u8));

	ret = fg_gauge_unseal(chip);
	if (ret < 0) {
		pr_err("end fail! cannot unseal ic! ret=%d\r", ret);
		goto fg_gauge_enable_sleep_mode_end;
	}
	msleep(CMD_SBS_DELAY);

	ret = fg_read_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf0);
	if (ret < 0) {
		pr_err("end: read PackConfig Fail! ret=%d\r", ret);
		goto fg_gauge_enable_sleep_mode_end;
	}

	temp16 = buf0[0] * 0x100 + buf0[1];
	temp16 |= PACKCONFIG_SLEEP_MASK;
	buf0[0] = (u8)(temp16 >> 8);
	buf0[1] = (u8)(temp16);
	ret = fg_write_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf0);
	msleep(CMD_SBS_DELAY);
	pr_err("end: write PackConfig 0x%04X, ret=%d\r", temp16, ret);

fg_gauge_enable_sleep_mode_end:
	fg_gauge_seal(chip);
	return ret;
}

static s32 fg_gauge_fix_cadcOffset_then_disable_sleep_mode(struct chip_bq27541 *chip) /* 20220625, Ethan */
{
	s32 ret;
	u16 temp16;
	s32 i;
	u8 buf0[2], buf1[2], buf_cadcoffet[2];
	memset(buf0, 0, 2 * sizeof(u8));
	memset(buf1, 0, 2 * sizeof(u8));

	if (!chip->gauge_fix_cadc)
		return 0;

	ret = fg_gauge_unseal(chip);
	if (ret < 0) {
		pr_err("fail! cannot unseal ic! ret=%d\r", ret);
		goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
	}
	msleep(CMD_SBS_DELAY);

	ret = fg_read_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf0);
	if (ret < 0) {
		pr_err("read PackConfig Fail! ret=%d\r", ret);
		goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
	}

	temp16 = buf0[0] * 0x100 + buf0[1];
	temp16 &= ~PACKCONFIG_SLEEP_MASK;
	buf0[0] = (u8)(temp16 >> 8);
	buf0[1] = (u8)(temp16);
	ret = fg_write_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf0);
	pr_err("write PackConfig 0x%04X, ret=%d\r", temp16, ret);

	/* must disable sleep mode first! in case cadc-auto-cali!! */
	ret = fg_read_sbs_word_then_check(chip, CMD_DFCONFIGVERSION, &temp16);
	if (ret < 0) {
		pr_err("fail! cannot read cmd 0x0C! ret=%d\r", ret);
		goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
	}
	msleep(CMD_SBS_DELAY);

	pr_err("DFConfigVersion is 0x%04X, ret=%d\r", temp16, !!(temp16 == DFCONFIG_CALIED));
	if (temp16 == DFCONFIG_CALIED) {
		ret = fg_read_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf_cadcoffet);
		if (ret < 0) {
			pr_err("fail! cannot read Origin Cadc Offset! ret=%d\r", ret);
			goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
		}

		ret = fg_read_dataflash(chip, ADDR_PACKLOTCODE, LENGTH_CADCOFFSET, buf0);
		if (ret < 0) {
			pr_err("fail! cannot read Pack Lot Code! ret=%d\r", ret);
			goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
		}
		msleep(CMD_SBS_DELAY);

		ret = 0;
		for (i = 0; i < LENGTH_CADCOFFSET; i++) {
			if (buf0[i] != buf_cadcoffet[i]) { /* 20231019, Sinowealth. Bug Fix */
				ret = -1;
				break;
			}
		}
		if (ret == 0) {
			pr_err("cadc offset same! quit re-write!\r");
		} else {
			ret = fg_write_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf0);
			if (ret < 0) {
				pr_err("fail! cannot write cadc offset! ret=%d\r", ret);
				goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
			}
			msleep(CMD_SBS_DELAY);

			oplus_vooc_set_allow_reading(false);
			ret = fg_write_sbs_word(chip, 0, CMD_FORCE_UPDATE_E2ROM);
			if (ret < 0) {
				pr_err("fail! cannot write CMD_FORCE_UPDATE_E2ROM! ret=%d\r", ret);
				oplus_vooc_set_allow_reading(true);
				goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
			}
			msleep(E2ROM_DELAY_MS);
			oplus_vooc_set_allow_reading(true);

			ret = fg_read_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf1);
			for (i = 0; i < LENGTH_CADCOFFSET; i++) {
				if (buf0[i] != buf1[i]) {
					ret = -1;
					break;
				}
			}

			if (ret < 0) {
				fg_write_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf_cadcoffet);
				pr_err("check cadc offset fail!\r");
				goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
			}
			sh366002_track_upload_fix_cadc_info(
				(buf_cadcoffet[1] | (buf_cadcoffet[0] << 8)), (buf0[1] | (buf0[0] << 8)), (buf1[1] | (buf1[0] << 8)), chip);
		}
	}

	buf0[0] = DEFAULT_DEADBAND;
	ret = fg_write_dataflash(chip, ADDR_DEADBAND, LENGTH_DEADBAND, buf0);
	if (ret < 0) {
		pr_err("write deadband Fail! ret=%d\r", ret);
		goto fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end;
	}

fg_gauge_fix_cadcOffset_then_disable_sleep_mode_end:
	fg_gauge_seal(chip);
	return ret;
}

s32 fg_gauge_calibrate_board(struct chip_bq27541 *chip) /* 20220625, Ethan */
{
	s32 ret;
	u8 buf[32];
	s32 i, error_cnt;
	u16 temp16, cadc_ratio, caliFlag;
	u8 deadband;
	s32 cadc_offset_origin, cadc_offset = 0;
	s32 tempS32;
	s32 cur_max, cur_min, cur_avg;
	u8 track_buf[64] = {0};

	if (!chip->gauge_cal_board)
		return -1;

	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$scene_%d@@calibrate_board", track_count);
	track_count += 1;
	bq27541_track_update_mode_buf(chip, track_buf);
	ret = fg_read_sbs_word_then_check(chip, CMD_TEMPERATURE, &temp16);
	if (ret < 0) {
		pr_err("cannot read Temperature! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}

	if ((temp16 > MAXTEMPER) || (temp16 < MINTEMPER)) {
		pr_err("Temperature Exceed 10 ~ 40deg! temper=%d\r\n", temp16);
		goto fg_gauge_calibrate_board_end;
	}

	ret = fg_read_sbs_word(chip, CMD_DFCONFIGVERSION, &temp16);
	if (ret < 0) {
		pr_err("cannot read DFConfigVersion! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}
	msleep(CMD_SBS_DELAY);

	if (temp16 == DFCONFIG_CALIED) {
		pr_err("Board is already calied! DFConfigVersion is 0x%04X\r\n", temp16);
		goto fg_gauge_calibrate_board_end;
	}

	ret = fg_gauge_unseal(chip);
	if (ret < 0) {
		pr_err("cannot unseal ic! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}
	msleep(CMD_SBS_DELAY);

	ret = fg_read_dataflash(chip, ADDR_DEADBAND, LENGTH_DEADBAND, buf);
	if (ret < 0) {
		pr_err("cannot read deadband! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}
	msleep(CMD_SBS_DELAY);
	deadband = buf[0];

	ret = fg_read_dataflash(chip, ADDR_CADCRATIO, LENGTH_CADCRATIO, buf);
	msleep(CMD_SBS_DELAY);
	cadc_ratio =  buf[0] * 0x100 + buf[1];
	cadc_offset_origin = (s32)((s16)(buf[INDEX_CADCOFFSET] * 0x100 + buf[INDEX_CADCOFFSET + 1]));
	if ((ret < 0) || (cadc_ratio == 0)) {
		ret = -1;
		pr_err("cannot read cali-ratio! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}

	ret = fg_read_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf);
	if (ret < 0) {
		pr_err("cannot read packconfig! ret=%d\r\n", ret);
		goto fg_gauge_calibrate_board_end;
	}

	temp16 = buf[0] * 0x100 + buf[1];
	temp16 &= ~PACKCONFIG_SLEEP_MASK;
	buf[0] = (u8)(temp16 >> 8);
	buf[1] = (u8)(temp16);
	ret = fg_write_dataflash(chip, ADDR_PACKCONFIG, LENGTH_PACKCONFIG, buf);
	msleep(CMD_SBS_DELAY);

	memset(buf, 0, LENGTH_DEADBAND * sizeof(u8));
	ret |= fg_write_dataflash(chip, ADDR_DEADBAND, LENGTH_DEADBAND, buf);
	msleep(CMD_SBS_DELAY);

	oplus_vooc_set_allow_reading(false);
	ret |= fg_write_sbs_word(chip, CMD_CNTL, CMD_FORCE_UPDATE_E2ROM);
	if (ret < 0) {
		pr_err("before cali: cannot write PackConfig | Deadband! ret=%d\r\n", ret);
		oplus_vooc_set_allow_reading(true);
		goto fg_gauge_calibrate_board_fail;
	}
	msleep(E2ROM_DELAY_MS);
	oplus_vooc_set_allow_reading(true);

	cur_max = 0x80000000;
	cur_min = 0x7FFFFFFF;
	cur_avg = 0;
	i = 0;
	error_cnt = 0;
	while (i < CALI_MAX_CNT) {
		msleep(CALI_DELAY_MS);
		ret = fg_read_sbs_word(chip, CMD_CURRENT, &temp16);
		if (ret < 0) {
			if (++error_cnt <= 3)
				continue;
			pr_err("cannot read current!\r\n");
			goto fg_gauge_calibrate_board_fail;
		}

		tempS32 = (s32)((s16)temp16);
		cur_max = max(cur_max, tempS32);
		cur_min = min(cur_min, tempS32);
		cur_avg += tempS32;
		pr_err("read curr before cali: index=%d, cur=%d, cur_sum=%d, cur_max=%d, cur_min=%d\r\n",
						i, tempS32, cur_avg, cur_max, cur_min);
		if (abs(tempS32) > CALI_MAX_CURRENT) {
			ret = -1;
			pr_err("read curr before cali fail! current too big!\r\n");
			goto fg_gauge_calibrate_board_fail;
		}

		error_cnt = 0;
		i++;
	}

	if ((abs(cur_max) < CALI_DEADBAND) && (abs(cur_min) < CALI_DEADBAND)) {
		pr_err("cali: all current is within deadband! no need to calibrate. cur_max=%d, cur_min=%d, deadband=%d\r\n",
						cur_max, cur_min, deadband);
		cadc_offset = cadc_offset_origin;
		ret = 1;
		goto fg_gauge_calibrate_board_ok;
	}
	tempS32 = abs(cur_max - cur_min);
	if (tempS32 > CALI_DELTA_CURRENT) {
		ret = -1;
		pr_err("read curr before cali fail! current differ exceed! cur_max=%d, cur_min=%d, differ=%d\r\n",
						cur_max, cur_min, tempS32);
		goto fg_gauge_calibrate_board_fail;
	}
	cur_avg = (cur_avg - cur_max - cur_min) / (CALI_MAX_CNT - 2);
	memset(&(track_buf[0]), 0, sizeof(track_buf));
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$old_curr@@%d,%d,%d", cur_avg, cur_max, cur_min);
	bq27541_track_update_mode_buf(chip, track_buf);
	/* cadcoffset_2 = current / cadc_ratio * base + cadc_offset_1 */
	cadc_offset = (cur_avg * BASE_CADCRATIO) / (s32)cadc_ratio + cadc_offset_origin;
	ret = (abs(cadc_offset) >= MAX_CADCOFFSET) ? -1 : 0;
	if (ret < 0) {
		pr_err("cali fail! cur_avg=%d, cadc_ratio=%d, cadc_offset_origin=%d, cadc_offset_new=%d, ret=%d\r\n",
						cur_avg, cadc_ratio, cadc_offset_origin, cadc_offset, ret);
		goto fg_gauge_calibrate_board_fail;
	}
	pr_err("cali ok! cur_avg=%d, cadc_ratio=%d, cadc_offset_origin=%d, cadc_offset_new=%d, ret=%d\r\n",
						cur_avg, cadc_ratio, cadc_offset_origin, cadc_offset, ret);
	buf[0] = (u8)(cadc_offset >> 8);
	buf[1] = (u8)cadc_offset;
	ret = fg_write_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf);
	msleep(CMD_SBS_DELAY);

	oplus_vooc_set_allow_reading(false);
	ret |= fg_write_sbs_word(chip, 0, CMD_FORCE_UPDATE_E2ROM);
	if (ret < 0) {
		pr_err("after cali fail! cannot write cadc_offset! ret=%d\r\n", ret);
		oplus_vooc_set_allow_reading(true);
		goto fg_gauge_calibrate_board_fail;
	}
	msleep(E2ROM_DELAY_MS);
	oplus_vooc_set_allow_reading(true);

	cur_max = 0x80000000;
	cur_min = 0x7FFFFFFF;
	cur_avg = 0;
	i = 0;
	error_cnt = 0;
	while (i < CALI_MAX_CNT) {
		msleep(1000);
		ret = fg_read_sbs_word(chip, CMD_CURRENT, &temp16);
		if (ret < 0) {
			if (++error_cnt <= 3)
				continue;
			pr_err("after cali fail! cannot read current!\r\n");
			goto fg_gauge_calibrate_board_fail;
		}

		tempS32 = (s32)((s16)temp16);
		cur_max = max(cur_max, tempS32);
		cur_min = min(cur_min, tempS32);
		cur_avg += tempS32;
		pr_err("read curr after cali: index=%d, cur=%d, cur_sum=%d, cur_max=%d, cur_min=%d\r\n", i, tempS32, cur_avg, cur_max, cur_min);
		if (abs(tempS32) > CALI_DEADBAND) {
			ret = -1;
			pr_err("read curr after cali fail! current too big!\r\n");
			goto fg_gauge_calibrate_board_fail;
		}

		error_cnt = 0;
		i++;
	}
	tempS32 = abs(cur_max - cur_min);
	if (tempS32 > CALI_DELTA_CURRENT) {
		ret = -1;
		pr_err("read curr after cali fail! current differ exceed! cur_max=%d, cur_min=%d, differ=%d\r\n", cur_max, cur_min, tempS32);
		goto fg_gauge_calibrate_board_fail;
	}
	cur_avg = (cur_avg - cur_max - cur_min) / (CALI_MAX_CNT - 2); /* 20230823, Ethan */
	memset(&(track_buf[0]), 0, sizeof(track_buf));
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$new_curr@@%d,%d,%d", cur_avg, cur_max, cur_min);
	bq27541_track_update_mode_buf(chip, track_buf);
	ret = (abs(cur_avg) > deadband) ? -1 : 0;
	if (ret < 0) {
		pr_err("after cali fail! cur_avg=%d, cur_max=%d, cur_min=%d, ret=%d\r\n", cur_avg, cur_max, cur_min, ret);
		goto fg_gauge_calibrate_board_fail;
	}
	pr_err("after cali OK! cur_avg=%d, cur_max=%d, cur_min=%d, ret=%d\r\n", cur_avg, cur_max, cur_min, ret);
fg_gauge_calibrate_board_ok:
	memset(&(track_buf[0]), 0, sizeof(track_buf));
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$cadc_offset@@%d,%d", cadc_offset_origin, cadc_offset);
	bq27541_track_update_mode_buf(chip, track_buf);
	ret = 1;
	caliFlag = DFCONFIG_CALIED;
	cadc_offset_origin = cadc_offset;
	goto fg_gauge_calibrate_board_clean_site;

fg_gauge_calibrate_board_fail:
	ret = -1;
	caliFlag = 0;
	memset(&(track_buf[0]), 0, sizeof(track_buf));
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$cadc_offset@@%d,%d", cadc_offset_origin, cadc_offset);
	bq27541_track_update_mode_buf(chip, track_buf);

fg_gauge_calibrate_board_clean_site:

	for (error_cnt = 0; error_cnt < 3 ; error_cnt++) {
		buf[0] = (u8)deadband;
		i = fg_write_dataflash(chip, ADDR_DEADBAND, LENGTH_DEADBAND, buf);
		msleep(CMD_SBS_DELAY);
		pr_err("clean site: write deadband. deadband=%d, ret=%d\r\n", deadband, i);

		buf[0] = (u8)(cadc_offset_origin >> 8);
		buf[1] = (u8)(cadc_offset_origin);
		i &= fg_write_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf);
		msleep(CMD_SBS_DELAY);
		pr_err("clean site: write cadc_offset. cadc_offset=%d, ret=%d\r\n", cadc_offset_origin, i);
		i &= fg_write_dataflash(chip, ADDR_PACKLOTCODE, LENGTH_CADCOFFSET, buf);
		msleep(CMD_SBS_DELAY);
		pr_err("clean site: write pack_lot_code. pack_lot_code=0x%04X, ret=%d\r\n", cadc_offset_origin, i);

		buf[0] = (u8)(caliFlag >> 8);
		buf[1] = (u8)(caliFlag);
		i &= fg_write_dataflash(chip, ADDR_DFCONFIGVERSION, LEN_DFCONFIGVERSION, buf);
		msleep(CMD_SBS_DELAY);
		pr_err("clean site: write df_config_version. df_config_version=0x%04X, ret=%d\r\n", caliFlag, i);

		if (i < 0)
			continue;

		oplus_vooc_set_allow_reading(false);
		fg_write_sbs_word(chip, 0, CMD_FORCE_UPDATE_E2ROM);
		msleep(E2ROM_DELAY_MS);
		oplus_vooc_set_allow_reading(true);

		i = fg_read_dataflash(chip, ADDR_DEADBAND, LENGTH_DEADBAND, buf);
		if (i >= 0)
			i = (buf[0] == deadband) ? 0 : -1;
		pr_err("clean site: read deadband. deadband=%d, read-deadband=%d, ret=%d\r\n", deadband, buf[0], i);
		if (i < 0)
			continue;
		msleep(CMD_SBS_DELAY);

		i = fg_read_dataflash(chip, ADDR_CADCOFFSET, LENGTH_CADCOFFSET, buf);
		tempS32 = (s32)(s16)(buf[0] * 0x100 + buf[1]);
		if (i >= 0)
			i = (tempS32 == cadc_offset_origin) ? 0 : -1;
		pr_err("clean site: read cadc_offset. cadc_offset=%d, read-cadc_offset=%d, ret=%d\r\n", cadc_offset_origin, tempS32, i);
		if (i < 0)
			continue;
		msleep(CMD_SBS_DELAY);

		i = fg_read_dataflash(chip, ADDR_PACKLOTCODE, LENGTH_CADCOFFSET, buf);
		tempS32 = (s32)(s16)(buf[0] * 0x100 + buf[1]);
		if (i >= 0)
			i = (tempS32 == cadc_offset_origin) ? 0 : -1;
		pr_err("clean site: read pack_lot_code. pack_lot_code=0x%04X, read-pack_lot_code=0x%04X, ret=%d\r\n", cadc_offset_origin, tempS32, i);
		if (i < 0)
			continue;
		msleep(CMD_SBS_DELAY);

		i = fg_read_dataflash(chip, ADDR_DFCONFIGVERSION, LEN_DFCONFIGVERSION, buf);
		temp16 = buf[0] * 0x100 + buf[1];
		if (i >= 0)
			i = (temp16 == caliFlag) ? 0 : -1;
		pr_err("clean site: read df_config_version. df_config_version=0x%04X, read-df_config_version=0x%04X, ret=%d\r\n", caliFlag, temp16, i);
		if (i < 0)
			continue;
		msleep(CMD_SBS_DELAY);
		break;
	}
	ret |= i;

fg_gauge_calibrate_board_end:
	fg_gauge_seal(chip);

	pr_err("ret=%d\r\n", ret);
	return ret;
}

static s32 fg_gauge_get_default_cell_model(struct chip_bq27541 *chip, char *profile_name, u16 *pBuf)
{
	struct device* dev = &chip->client->dev;
	struct device_node* np = dev->of_node;
	u16* kzBuf;
	u8 str[STRLEN];
	s32 buflen, ret;
	s32 ratio;
	s32 i, j;
	u16 temp16;
	u16 cycle0;

	if (profile_name == NULL) {
		pr_err("fail: Cannot find profile_name\r\n");
		return -1;
	}

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if ((np == NULL) || (pBuf == NULL)) {
		pr_err("Cannot find child node \"battery_params\"\r\n");
		return -1;
	}

	buflen = of_property_count_elems_of_size(np, profile_name, sizeof(u16));
	if (buflen < CELL_MODEL_COUNT) {
		pr_err("read len too small! ele_len=%d, key=%s\n", buflen, profile_name);
		return -1;
	}

	kzBuf = (u16*)devm_kzalloc(dev, buflen * sizeof(u16), 0);
	if (kzBuf == NULL) {
		pr_err("kzalloc error!\r\n");
		return -1;
	}

	ret = of_property_read_u16_array(np, profile_name, kzBuf, buflen);
	if (ret < 0) {
		pr_err("read model dtsi fail! ret=%d\r\n", ret);
		devm_kfree(dev, kzBuf);
		return ret;
	}

	memset(str, 0, STRLEN * sizeof(u8));
	for (i = 0, j = 0; i < CELL_MODEL_COUNT; i++) {
		pBuf[i] = kzBuf[i];
		j += sprintf(&str[j], "%u, ", pBuf[i]);
	}
	pr_err("read model dtsi is %s\r\n", str);


	ret = fg_read_sbs_word_then_check(chip, CMD_CYCLECOUNT, &cycle0);
	if (ret < 0) {
		pr_err("cannot read cycle_count! ret=%d\r\n", ret);
		devm_kfree(dev, kzBuf);
		return ret;
	}

	if (cycle0 >= 800)
		ratio = 3000;
	else if (cycle0 >= 600)
		ratio = 2500;
	else if (cycle0 >= 400)
		ratio = 2000;
	else if (cycle0 >= 200)
		ratio = 1500;
	else
		ratio = 1000;

	memset(str, 0, STRLEN);
	for (i = 0, j = 0; i < CELL_MODEL_COUNT; i++) {
		temp16 = pBuf[i] * ratio / MODELRATIO_BASE;

		if (temp16 > MAX_MODEL)
			temp16 = MAX_MODEL;
		if (temp16 < MIN_MODEL)
			temp16 = MIN_MODEL;

		pBuf[i] = temp16;
		j += sprintf(&str[j], "%u, ", pBuf[i]);
	}

	pr_err("CycleCount=%u, CycleRatio=%u, Model=%s\r\n", cycle0, ratio, str);

	return cycle0;
}

/* -1: err; 0: Model OK; else: Error Flag */
s32 fg_gauge_check_cell_model(struct chip_bq27541 *chip, char *profile_name) /* 20220625, Ethan */
{
	s32 ret;
	u8 buf[32];
	u16 model[15];
	s32 i, j, k, modelRatio;
	s32 maxValue, minValue;
	u16 temp16;
	u8 str[200];
	u16 pBuf[15];
	u8 track_buf[64] = {0};
	int len;

	if (!chip->gauge_check_model)
		return -1;

	memset(str, 0, sizeof(u8) * 200);
	ret = fg_gauge_get_default_cell_model(chip, profile_name, pBuf);
	if (ret < 0) {
		pr_err("fg_gauge_check_cell_model fail! cannot read default cell-model! ret=%d\r\n", ret);
		goto fg_gauge_check_cell_model_end;
	}
	temp16 = (u16)ret;

	mutex_lock(&chip->bq28z610_alt_manufacturer_access);
	ret = fg_read_ram_block(chip, CMD_CELLMODEL, 0, BYTE_COUNT_CELL_MODEL, buf);
	mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
	if (ret < 0) {
		pr_err("fail! cannot read cell-model! ret=%d\r\n", ret);
		goto fg_gauge_check_cell_model_end;
	}

	for (i = 0, j = 0; i < 15; i++) {
		model[i] = (u8)(buf[2 * i] ^ 0x5A) + 0x100 * (u8)(buf[2 * i + 1] ^ 0x43);
		if (model[i] == 0)
			return -1;
		j += sprintf(&str[j], "%u, ", model[i]);
	}
	pr_err("model is %s\r\n", str);

	ret = MODELFLAG_NONE;

	if (temp16 <= 800) {
		maxValue = 0;
		minValue = 0;

		for (i = TOOLARGE_STARTGRID; i <= TOOLARGE_ENDGRID; i++) {
			maxValue += model[i];
			minValue += pBuf[i];
		}

		/* model < 0x1_0000 =>  max < 0x10_0000. max * 1000 < 0x3e80_0000, won't overflow */
		modelRatio = maxValue * TOOLARGE_RATIO / minValue;

		if (modelRatio >= TOOLARGE_MAXVAULE) {
			ret |= MODELFLAG_NEW_LARGE;
			maxValue /= (TOOLARGE_ENDGRID - TOOLARGE_STARTGRID + 1);
			minValue /= (TOOLARGE_ENDGRID - TOOLARGE_STARTGRID + 1);
			pr_err("model too large! avg=%u, default_avg=%u, ratio=%d\r\n", maxValue, minValue, modelRatio);
		}
	}

	for (i = TOOSMALL_STARTGRID_0; i <= TOOSMALL_ENDGRID_0; i++) {
		if (model[i] < TOOSMALL_MINVAULE_0) {
			ret |= MODELFLAG_NEW_SMALL;
			pr_err("model too small! index=%d, model=%u\r\n", i, model[i]);
			break;
		}
	}
	for (i = TOOSMALL_STARTGRID_1; i <= TOOSMALL_ENDGRID_1; i++) {
		if (model[i] < TOOSMALL_MINVAULE_1) {
			ret |= MODELFLAG_NEW_SMALL;
			pr_err("model too small! index=%d, model=%u\r\n", i, model[i]);
			break;
		}
	}
	for (i = TOOSMALL_STARTGRID_2; i <= TOOSMALL_ENDGRID_2; i++) {
		if (model[i] < TOOSMALL_MINVAULE_2) {
			ret |= MODELFLAG_NEW_SMALL;
			pr_err("model too small! index=%d, model=%u\r\n", i, model[i]);
			break;
		}
	}

	for (i = SLIGHT_STARTGRID, k = 0; i <= SLIGHT_ENDGRID - SLIGHT_GAP + 1; i++) {
		maxValue = 0;
		minValue = 0x7FFFFFFF;
		for (j = i; j < i + SLIGHT_GAP; j++) {
			modelRatio = (u16)model[j];
			maxValue = max(maxValue, modelRatio);
			minValue = min(minValue, modelRatio);
		}

		modelRatio = maxValue * SLIGHT_RATIO / minValue;
		k += !!(modelRatio >= SLIGHT_MAXRATIO);
		pr_err("model slight singular cnt=%u, index=%u, max=%u, min=%u, ratio=%u\r\n", k, i, maxValue, minValue, modelRatio);
	}
	if (k >= SLIGHT_MAXCNT) {
		ret |= MODELFLAG_NEW_SLIGHT;
		pr_err("model slight singular! cnt=%u\r\n", k);
	}

	for (i = EXTREME_STARTGRID_0, k = 0; i <= EXTREME_ENDGRID_0 - EXTREME_GAP_0 + 1; i++) {
		maxValue = 0;
		minValue = 0x7FFFFFFF;
		for (j = i; j < i + EXTREME_GAP_0; j++) {
			modelRatio = (u16)model[j];
			maxValue = max(maxValue, modelRatio);
			minValue = min(minValue, modelRatio);
		}

		modelRatio = maxValue * EXTREME_RATIO / minValue;
		k += !!(modelRatio >= EXTREME_MAXRATIO_0);
		pr_err("model extreme singular cnt=%u, index=%u, max=%u, min=%u, ratio=%u\r\n", k, i, maxValue, minValue, modelRatio);
	}
	if (k >= EXTREME_MAXCNT_0) {
		ret |= MODELFLAG_NEW_EXTREME;
		pr_err("model extreme singular! cnt=%u\r\n", k);
	}

	for (i = EXTREME_STARTGRID_1, k = 0; i <= EXTREME_ENDGRID_1 - EXTREME_GAP_1 + 1; i++) {
		maxValue = 0;
		minValue = 0x7FFFFFFF;
		for (j = i; j < i + EXTREME_GAP_1; j++) {
			modelRatio = (u16)model[j];
			maxValue = max(maxValue, modelRatio);
			minValue = min(minValue, modelRatio);
		}

		modelRatio = maxValue * EXTREME_RATIO / minValue;
		k += !!(modelRatio >= EXTREME_MAXRATIO_1);
		pr_err("model extreme singular cnt=%u, index=%u, max=%u, min=%u, ratio=%u\r\n", k, i, maxValue, minValue, modelRatio);
	}
	if (k >= EXTREME_MAXCNT_1) {
		ret |= MODELFLAG_NEW_EXTREME;
		pr_err("model extreme singular! cnt=%u\r\n", k);
	}

	modelRatio = model[EXTREME_STARTGRID_2] * EXTREME_RATIO / model[EXTREME_ENDGRID_2];
	if (modelRatio >= EXTREME_MAXRATIO_2) {
		ret |= MODELFLAG_NEW_EXTREME;
		pr_err("model extreme singular! index=%u, ratio=%u, limit=%u\r\n", EXTREME_STARTGRID_2, modelRatio, EXTREME_MAXRATIO_2);
	}

fg_gauge_check_cell_model_end:
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$scene_%d@@check_mode$$model_%d@@", track_count, track_count);
	bq27541_track_update_mode_buf(chip, track_buf);
	track_count += 1;
	len = strlen(str);
	snprintf(&(str[len]),  sizeof(str) - len - 1, "$$excp_type@@0x%08x", ret);
	bq27541_track_update_mode_buf(chip, str);
	pr_err("end! ret=0x%08X\r\n", ret);
	return ret;
}

s32 fg_gauge_restore_cell_model(struct chip_bq27541 *chip, char *profile_name)
{
	u16 pBuf[15];
	u8 buf_read[BYTE_COUNT_CELL_MODEL];
	u8 buf_write[LENGTH_CELLMODEL];
	u8 str[STRLEN] = {0};
	s32 ret;
	s32 i, j;
	u8 byteH, byteL;
	u16 temp16;
	u16 cycle0;
	int len;
	u8 track_buf[64] = {0};

	ret = fg_gauge_get_default_cell_model(chip, profile_name, pBuf);
	if (ret < 0) {
		cycle0 = ret;
		pr_err("fg_gauge_restore_cell_model err! cannot get default model! ret=%d\r\n", ret);
		goto fg_gauge_restore_cell_model_end;
	}
	cycle0 = ret;

	ret = fg_gauge_unseal(chip);
	if (ret < 0) {
		pr_err("err! cannot unseal IC! ret=%d\r\n", ret);
		goto fg_gauge_restore_cell_model_end;
	}

	memset(buf_write, 0, LENGTH_CELLMODEL * sizeof(u8));
	buf_write[INDEX_CELLMODEL] = 0x18;
	buf_write[INDEX_CELLMODEL + 1] = 0x4C;
	buf_write[INDEX_XCELLMODEL] = 0xE7;
	buf_write[INDEX_XCELLMODEL + 1] = 0xE6;

	for (i = 0, j = 2; i < CELL_MODEL_COUNT; i++, j += 2) {
		byteH = (u8)(0x18 ^ j ^ (pBuf[i] >> 8));
		byteL = (u8)(0x18 ^ (j + 1) ^ pBuf[i]);

		buf_write[INDEX_CELLMODEL + j] = byteH;
		buf_write[INDEX_CELLMODEL + j + 1] = byteL;
		buf_write[INDEX_XCELLMODEL + j] = byteH;
		buf_write[INDEX_XCELLMODEL + j + 1] = byteL;
	}

	ret = fg_write_dataflash(chip, ADDR_CELLMODEL, LENGTH_CELLMODEL, buf_write);
	if (ret < 0) {
		pr_err("fail! cannot write cell-model to E2rom, ret=%d\r\n", ret);
		goto fg_gauge_restore_cell_model_end;
	}

	oplus_vooc_set_allow_reading(false);
	fg_write_sbs_word(chip, CMD_CNTL, CMD_FORCE_UPDATE_E2ROM);
	msleep(CMD_E2ROM_DELAY);
	oplus_vooc_set_allow_reading(true);

	mutex_lock(&chip->bq28z610_alt_manufacturer_access);
	ret = fg_read_ram_block(chip, CMD_CELLMODEL, 0, BYTE_COUNT_CELL_MODEL, buf_read);
	mutex_unlock(&chip->bq28z610_alt_manufacturer_access);
	if (ret < 0) {
		pr_err("fail! cannot read cell-model, ret=%d\r\n", ret);
		goto fg_gauge_restore_cell_model_end;
	}

	ret = 0;
	for (i = 0, j = 0; i < CELL_MODEL_COUNT; i++) {
		temp16 = 0x100 * (u8)(buf_read[2 * i + 1] ^ 0x43) + (u8)(buf_read[2 * i] ^ 0x5A);
		if (pBuf[i] != temp16)
			ret = -1;

		j += sprintf(&str[j], "%u, ", temp16);
	}

	pr_err("read model check is %d, model=%s\r\n", ret, str);

fg_gauge_restore_cell_model_end:
	pr_err("end: ret=%d\r\n", ret);
	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$scene_%d@@restore_mode$$model_%d@@", track_count, track_count);
	bq27541_track_update_mode_buf(chip, track_buf);
	len = strlen(str);
	snprintf(&(str[len]),  sizeof(str) - len - 1, "$$cycle_%d@@%d$$ret_%d@@%d", track_count, cycle0, track_count, ret);
	bq27541_track_update_mode_buf(chip, str);
	track_count += 1;
	fg_gauge_seal(chip);
	return ret;
}

s32 fg_gauge_check_por_soc(struct chip_bq27541 *chip)
{
	s32 ret;
	u8 buf[32];
	u16 temp16, cycleCount, cycleModel;
	u8 track_buf[64] = {0};

	if (!chip->gauge_check_por)
		return -1;

	ret = fg_read_block(chip, CMD_BLOCK3, INDEX_POR_FLAG, LENGTH_POR_FLAG, buf);
	if (ret < 0) {
		pr_err("error! cannot read block 3, ret=%d\r\n", ret);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	if (buf[0] != POR_FLAG_TRIGGER) {
		ret = 0;
		pr_err("flag is ok, donot need reset-gauge!\r\n");
		goto fg_gauge_check_por_soc_end;
	}

	ret = fg_read_sbs_word(chip, CMD_CURRENT, &temp16);
	if ((ret < 0) || (abs((s16)temp16) > MAX_ABS_CURRENT)) {
		pr_err("error! current too large or cannot read current! ret=%d, current=%d\r\n", ret, (s16)temp16);
		ret = -1;
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	ret = fg_read_sbs_word_then_check(chip, CMD_CYCLECOUNT, &cycleCount);
	if (ret < 0) {
		pr_err("error! cannot read CycleCount! ret=%d\r\n", ret);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	ret = fg_read_ram_block(chip, CMD_CYCLE_MODEL, INDEX_CYCLE_MODEL, LENGTH_CYCLE_MODEL, buf);
	if (ret < 0) {
		pr_err("error! cannot read CycleModel! ret=%d\r\n", ret);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);
	cycleModel = (u16)(buf[0] * 0x100 + buf[1]);

	snprintf(&(track_buf[0]),  sizeof(track_buf), "$$scene_%d@@por_soc$$cc_info@@%d,%d", track_count, cycleCount, cycleModel);
	bq27541_track_update_mode_buf(chip, track_buf);

	pr_err("end: ret=%d\r\n", ret);

	ret = (s32)(cycleCount - cycleModel);
	temp16 = !!(ret > CYCLE_MODEL_DIFFER);
	pr_err("CycleCount=%u, CycleModel=%u, differ=%d, reset_model=%d\r\n", cycleCount, cycleModel, ret, temp16);
	if (temp16) { /* long-time no update model. fix model */
		ret = fg_gauge_restore_cell_model(chip, DTSI_MODEL_NAME);
		if (ret < 0) {
			pr_err("fix model error! ret=%d\r\n", ret);
			goto fg_gauge_check_por_soc_end;
		}
	}

	sh366002_read_gaugeinfo_block(chip); /* 20230823, Ethan. Print GaugeInfo before Gauge_Enable */

	ret = fg_gauge_unseal(chip);
	if (ret < 0) {
		pr_err("unseal fail! ret=%d\r\n", ret);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	oplus_vooc_set_allow_reading(false);
	ret = fg_write_sbs_word(chip, CMD_CNTL, CMD_GAUGE_ENABLE);
	if (ret < 0) {
		pr_err("enable gauge fail! ret=%d\r\n", ret);
		oplus_vooc_set_allow_reading(true);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_GAUGE_DELAY);
	oplus_vooc_set_allow_reading(true);

	buf[0] = POR_FLAG_DEFAULT;
	ret = fg_write_dataflash(chip, ADDR_BLOCK3, LENGTH_POR_FLAG, buf);
	if (ret < 0) {
		pr_err("error! cannot write block 3, ret=%d\r\n", ret);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	oplus_vooc_set_allow_reading(false);
	ret = fg_write_sbs_word(chip, CMD_CNTL, CMD_FORCE_UPDATE_E2ROM);
	if (ret < 0) {
		pr_err("error! cannot write E2rom, ret=%d\r\n", ret);
		oplus_vooc_set_allow_reading(true);
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_E2ROM_DELAY);
	oplus_vooc_set_allow_reading(true);

	ret = fg_read_block(chip, CMD_BLOCK3, INDEX_POR_FLAG, LENGTH_POR_FLAG, buf);
	if ((ret < 0) || (buf[0] != POR_FLAG_DEFAULT)) {
		pr_err("error! block check fail! ret=%d, flag=0x%02X\r\n", ret, buf[INDEX_POR_FLAG]);
		ret = -1;
		goto fg_gauge_check_por_soc_end;
	}
	msleep(CMD_SBS_DELAY);

	sh366002_read_gaugeinfo_block(chip); /* 20230823, Ethan. Print GaugeInfo after Gauge_Enable */

fg_gauge_check_por_soc_end:
	track_count = 0;
	pr_err("end: ret=%d\r\n", ret);
	fg_gauge_seal(chip);
	return ret;
}

#define TRACK_TIME_SCHEDULE_FIX_CADC 100000
static int sh366002_track_upload_fix_cadc_info(int old_cadcoffset, int new_cadcoffset,
						int packlot, struct chip_bq27541 *bq_chip)
{
	int index = 0;
	struct sh366002_model_track *chip = &fix_cadc_offset;

	if (!chip->track_init_done)
		return -EINVAL;

	mutex_lock(&chip->track_lock);
	if (chip->uploading) {
		pr_info("uploading, should return\n");
		mutex_unlock(&chip->track_lock);
		return 0;
	}

	if (chip->load_trigger)
		kfree(chip->load_trigger);
	chip->load_trigger = kzalloc(sizeof(oplus_chg_track_trigger), GFP_KERNEL);
	if (!chip->load_trigger) {
		pr_err("load_trigger memery alloc fail\n");
		mutex_unlock(&chip->track_lock);
		return -ENOMEM;
	}
	chip->load_trigger->type_reason =
		TRACK_NOTIFY_TYPE_GENERAL_RECORD;
	chip->load_trigger->flag_reason =
		TRACK_NOTIFY_FLAG_GAUGE_MODE;
	chip->uploading = true;
	mutex_unlock(&chip->track_lock);
	if (bq_chip->capacity_pct == 0) {
		index += snprintf(
			&(chip->load_trigger->crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$device_id@@%s", "zy0602");
	} else {
		index += snprintf(
			&(chip->load_trigger->crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index, "$$device_id@@%s_%d", "zy0602", bq_chip->gauge_num);
	}

	index += snprintf(
		&(chip->load_trigger->crux_info[index]),
		OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
		"$$scene@@%s", "fix_cadc_offset");

	index += snprintf(&(chip->load_trigger->crux_info[index]),
			OPLUS_CHG_TRACK_CURX_INFO_LEN - index,
			"$$cadc_offset@@%d,%d,%d", old_cadcoffset, new_cadcoffset, packlot);
	schedule_delayed_work(&chip->load_trigger_work, msecs_to_jiffies(TRACK_TIME_SCHEDULE_FIX_CADC));
	pr_info("success\n");

	return 0;
}

static void sh366002_track_fix_cadc_load_trigger_work(
	struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sh366002_model_track *chip =
		container_of(
			dwork, struct sh366002_model_track,
			load_trigger_work);

	if (!chip->load_trigger)
		return;

	oplus_chg_track_upload_trigger_data(*(chip->load_trigger));
	kfree(chip->load_trigger);
	chip->load_trigger = NULL;
	chip->uploading = false;
}

static int sh366002_track_init(void)
{
	mutex_init(&fix_cadc_offset.track_lock);
	INIT_DELAYED_WORK(&fix_cadc_offset.load_trigger_work,
		sh366002_track_fix_cadc_load_trigger_work);
	fix_cadc_offset.track_init_done = true;

	return 0;
}

s32 sh366002_init(struct chip_bq27541 *chip)
{
	s32 ret;
	s32 version_ret;
	s32 retry;
	u16 temp16;
	struct device_node *np = NULL;

	if (chip->device_type != DEVICE_ZY0602)
		return 0;

	sh366002_track_init();
	chip->log_last_update_tick = get_jiffies_64();

	ret = of_property_read_u32(chip->dev->of_node, "qcom,dump_sh366002_block", &chip->dump_sh366002_block);
	if (ret)
		chip->dump_sh366002_block = 0;

	if (oplus_is_rf_ftm_mode())
		return 0;

	np = of_get_child_by_name(chip->dev->of_node, "battery_params");
	if (np == NULL) {
		pr_err("%s Not found battery_params,return\n", chip->gauge_num == 0 ? "main gauge" : "sub gauge");
		return 0;
	} else {
		pr_err("%s found battery_params\n", chip->gauge_num == 0 ? "main gauge" : "sub gauge");
	}

	version_ret = Check_Chip_Version(chip);
	if (version_ret == CHECK_VERSION_ERR) {
		pr_err("Probe: Check version error!\n");
	} else if (version_ret == CHECK_VERSION_OK) {
		pr_err("Probe: Check version ok!\n");
	} else {
		pr_err("Probe: Check version update: %X\n", version_ret);

		ret = ERRORTYPE_NONE;
		if (version_ret & CHECK_VERSION_FW) {
			version_ret = 0;
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				pr_err("Probe: FW Update start, retry=%d\n", retry);
				if (fg_gauge_unseal(chip) < 0) {
					pr_err("Probe: FW Update unseal fail! retry=%d\n", retry);
					continue;
				}

				ret = file_decode_process(chip, "sinofs_fw_data");
				pr_err("Probe: FW Update end, retry=%d, ret=%d\n", retry, ret);
				if (ret == ERRORTYPE_NONE) {
					version_ret = Check_Chip_Version(chip);
					if ((version_ret & CHECK_VERSION_FW) == 0)
						break;
					ret = ERRORTYPE_FINAL_COMPARE;
					pr_err("Probe: FW Verify fail! retry=%d, ret=0x%04X\n", retry, version_ret);
				}
				msleep(FILE_DECODE_DELAY);
			}
		}

		if (version_ret & CHECK_VERSION_AFI) {
			version_ret = 0;
			for (retry = 0; retry < FILE_DECODE_RETRY; retry++) {
				pr_err("Probe: AFI Update start, retry=%d\n", retry);

				ret = fg_read_sbs_word(chip, CMD_IAP_DEVICE, &temp16);
				msleep(CMD_SBS_DELAY);
				if (ret < 0)
					continue;
				if (temp16 == IAP_DEVICE_ID) { /* in iap mode, donot unseal! */
					pr_err("Probe: In iap mode, donot need unseal!\n");
				} else if (fg_gauge_unseal(chip) < 0) {
					pr_err("Probe: AFI Update unseal fail! retry=%d\n", retry);
					continue;
				}

				ret = file_decode_process(chip, "sinofs_afi_data");
				pr_err("Probe: AFI Update end, retry=%d, ret=%d\n", retry, ret);
				if (ret == ERRORTYPE_NONE) {
					version_ret = Check_Chip_Version(chip);
					if ((version_ret & CHECK_VERSION_AFI) == 0)
						break;
					ret = ERRORTYPE_FINAL_COMPARE;
					pr_err("Probe: AFI Verify fail! retry=%d, ret=0x%04X\n", retry, version_ret);
				}
				msleep(FILE_DECODE_DELAY);
			}
		}
		pr_err("Probe: afi update finish! ret=%d\n", ret);
	}
	fg_gauge_seal(chip);

	for (retry = 0; retry < CADC_OFFSET_RETRY; retry++) {
		ret = fg_gauge_fix_cadcOffset_then_disable_sleep_mode(chip);
		if (ret >= 0)
			break;
	}
	return 0;
}
