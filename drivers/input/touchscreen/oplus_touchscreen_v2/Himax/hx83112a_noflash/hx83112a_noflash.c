// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/task_work.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/machine.h>
#include <linux/regulator/consumer.h>

#include <linux/spi/spi.h>


#include "hx83112a_noflash.h"

#define OPLUS17001TRULY_TD4322_1080P_CMD_PANEL 29

/*******Part0:LOG TAG Declear********************/

#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "himax,hx83112a_nf"
#else
#define TPD_DEVICE "himax,hx83112a_nf"
#endif

/*******Part0: SPI Interface***************/
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
const struct mtk_chip_config hx_spi_ctrdata = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.cs_pol = 0,
};
#else
const struct mt_chip_conf hx_spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 3, /* 16.6MHz */
	.low_time = 3,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,

	.cpol = 0,
	.cpha = 0,

	.rx_mlsb = 1,
	.tx_mlsb = 1,

	.tx_endian = 0,
	.rx_endian = 0,

	.com_mod = DMA_TRANSFER,

	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if !defined(CONFIG_VMAP_STACK) || defined(CONFIG_TOUCHPANEL_MTK_PLATFORM)  /* For Checksum Fail On MTK Platform*/
static ssize_t himax_spi_sync(struct chip_data_hx83112a_nf *chip_info,
			      struct spi_message *message)
{
	int status;

	status = spi_sync(chip_info->hx_spi, message);

	if (status == 0) {
		status = message->status;

		if (status == 0) {
			status = message->actual_length;
		}
	}

	return status;
}

static int himax_spi_read(struct chip_data_hx83112a_nf *chip_info,
			  uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length,
			  uint8_t toRetry)
{
	struct spi_message message;
	struct spi_transfer xfer[2];
	int retry = 0;
	int error = -1;

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	xfer[0].tx_buf = command;
	xfer[0].len = command_len;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = data;
	xfer[1].len = length;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		error = spi_sync(chip_info->hx_spi, &message);

		if (error) {
			TPD_INFO("SPI read error: %d\n", error);

		} else {
			break;
		}
	}

	if (retry == toRetry) {
		TPD_INFO("%s: SPI read error retry over %d\n",
			 __func__, toRetry);
		return -EIO;
	}

	return 0;
}

static int himax_spi_write(struct chip_data_hx83112a_nf *chip_info,
			   uint8_t *buf, uint32_t length)
{
	struct spi_transfer t = {
		.tx_buf = buf,
		.len = length,
	};
	struct spi_message    m;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return himax_spi_sync(chip_info, &m);
}
#endif /* end of !defined(CONFIG_VMAP_STACK）|| defined(CONFIG_TOUCHPANEL_MTK_PLATFORM)*/

static int himax_bus_read(struct chip_data_hx83112a_nf *chip_info,
			  uint8_t command, uint32_t length, uint8_t *data)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&(chip_info->spi_lock));
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;
#ifdef CONFIG_VMAP_STACK
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM   /* For Checksum Fail On MTK Platform*/
	result = himax_spi_read(chip_info, &spi_format_buf[0], 3, data, length, 10);
#else
	result = spi_write_then_read(private_ts->s_client, &spi_format_buf[0], 3, data,
				     length);
#endif   /*end of CONFIG_TOUCHPANEL_MTK_PLATFORM*/
#else
	result = himax_spi_read(chip_info, &spi_format_buf[0], 3, data, length, 10);
#endif  /* end of CONFIG_VMAP_STACK*/
	mutex_unlock(&(chip_info->spi_lock));

	return result;
}

static int himax_bus_write(struct chip_data_hx83112a_nf *chip_info,
			   uint8_t command, uint32_t length, uint8_t *data)
{
	/* uint8_t spi_format_buf[length + 2]; */
	int result = 0;
	static uint8_t *spi_format_buf;
	int alloc_size = 0;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	alloc_size = 256;
#else
	alloc_size = 49156;
#endif
	mutex_lock(&(chip_info->spi_lock));

	if (spi_format_buf == NULL) {
		spi_format_buf = kzalloc((alloc_size + 2) * sizeof(uint8_t), GFP_KERNEL);
	}

	if (spi_format_buf == NULL) {
		TPD_INFO("%s: Can't allocate enough buf\n", __func__);
		return -ENOMEM;
	}

	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	memcpy((uint8_t *)(&spi_format_buf[2]), data, length);
#ifdef CONFIG_VMAP_STACK
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM    /* For Checksum Fail On MTK Platform*/
	result = himax_spi_write(chip_info, spi_format_buf, length + 2);
#else
	result = spi_write_then_read(private_ts->s_client, &spi_format_buf[0],
				     length + 2, &spi_format_buf[0], 0);
#endif   /*end of CONFIG_TOUCHPANEL_MTK_PLATFORM*/
#else
	result = himax_spi_write(chip_info, spi_format_buf, length + 2);
#endif  /* end of CONFIG_VMAP_STACK*/
	mutex_unlock(&(chip_info->spi_lock));

	return result;
}

/*******Part1: Function Declearation*******/
static uint32_t himax_hw_check_CRC(struct chip_data_hx83112a_nf *chip_info,
				   uint8_t *start_addr, int reload_length);
static fw_check_state hx83112a_nf_fw_check(void *chip_data,
		struct resolution_info *resolution_info, struct panel_info *panel_data);
static void himax_read_FW_ver(struct chip_data_hx83112a_nf *chip_info);
static int hx83112a_nf_resetgpio_set(struct hw_resource *hw_res, bool on);

/*******Part2:Call Back Function implement*******/

/* add for himax */
void himax_flash_write_burst(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t *reg_byte, uint8_t *write_data)
{
	uint8_t data_byte[8];
	int i = 0;
	int j = 0;

	for (i = 0; i < 4; i++) {
		data_byte[i] = reg_byte[i];
	}

	for (j = 4; j < 8; j++) {
		data_byte[j] = write_data[j - 4];
	}

	if (himax_bus_write(chip_info, 0x00, 8, data_byte) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return;
	}
}

void himax_flash_write_burst_length(struct chip_data_hx83112a_nf *chip_info,
				    uint8_t *reg_byte, uint8_t *write_data, int length)
{
	uint8_t *data_byte;
	data_byte = tp_devm_kzalloc(&chip_info->hx_spi->dev,
				    sizeof(uint8_t) * (length + 4), GFP_KERNEL);

	if (data_byte == NULL) {
		TPD_INFO("%s: Can't allocate enough buf\n", __func__);
		return;
	}

	memcpy(data_byte, reg_byte, 4); /* assign addr 4bytes */
	memcpy(data_byte + 4, write_data, length); /* assign data n bytes */

	if (himax_bus_write(chip_info, 0, length + 4, data_byte) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
	}

	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)data_byte,
		      sizeof(uint8_t) * (length + 4));
}

void himax_burst_enable(struct chip_data_hx83112a_nf *chip_info,
			uint8_t auto_add_4_byte)
{
	uint8_t tmp_data[4];
	tmp_data[0] = 0x31;

	if (himax_bus_write(chip_info, 0x13, 1, tmp_data) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return;
	}

	tmp_data[0] = (0x10 | auto_add_4_byte);

	if (himax_bus_write(chip_info, 0x0D, 1, tmp_data) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return;
	}

	/*isBusrtOn = true;*/
}

void himax_register_read(struct chip_data_hx83112a_nf *chip_info,
			 uint8_t *read_addr, int read_length, uint8_t *read_data, bool cfg_flag)
{
	uint8_t tmp_data[4];
	int ret;

	if (cfg_flag == false) {
		if (read_length > 256) {
			TPD_INFO("%s: read len over 256!\n", __func__);
			return;
		}

		if (read_length > 4) {
			himax_burst_enable(chip_info, 1);

		} else {
			himax_burst_enable(chip_info, 0);
		}

		tmp_data[0] = read_addr[0];
		tmp_data[1] = read_addr[1];
		tmp_data[2] = read_addr[2];
		tmp_data[3] = read_addr[3];
		ret = himax_bus_write(chip_info, 0x00, 4, tmp_data);

		if (ret < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		tmp_data[0] = 0x00;
		ret = himax_bus_write(chip_info, 0x0C, 1, tmp_data);

		if (ret < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		if (himax_bus_read(chip_info, 0x08, read_length, read_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		if (read_length > 4) {
			himax_burst_enable(chip_info, 0);
		}

	} else if (cfg_flag == true) {
		if (himax_bus_read(chip_info, read_addr[0], read_length, read_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

	} else {
		TPD_INFO("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
		return;
	}
}

void himax_register_write(struct chip_data_hx83112a_nf *chip_info,
			  uint8_t *write_addr, int write_length, uint8_t *write_data, bool cfg_flag)
{
	int i = 0;
	int address = 0;

	if (cfg_flag == false) {
		address = (write_addr[3] << 24) + (write_addr[2] << 16) +
			  (write_addr[1] << 8) + write_addr[0];

		for (i = address; i < address + write_length; i++) {
			if (write_length > 4) {
				himax_burst_enable(chip_info, 1);

			} else {
				himax_burst_enable(chip_info, 0);
			}

			himax_flash_write_burst_length(chip_info, write_addr, write_data, write_length);
		}

	} else if (cfg_flag == true) {
		if (himax_bus_write(chip_info, write_addr[0], write_length, write_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

	} else {
		TPD_INFO("%s: cfg_flag = %d, value is wrong!\n", __func__, cfg_flag);
		return;
	}
}

static int himax_mcu_register_write(struct chip_data_hx83112a_nf *chip_info,
				    uint8_t *write_addr, uint32_t write_length, uint8_t *write_data,
				    uint8_t cfg_flag)
{
	int total_read_times = 0;
	int max_bus_size = 128, test = 0;
	int total_size_temp = 0;
	int address = 0;
	int i = 0;

	uint8_t tmp_addr[4];
	uint8_t *tmp_data;

	total_size_temp = write_length;
	TPD_DETAIL("%s, Entering - total write size=%d\n", __func__, total_size_temp);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (write_length > 240) {
		max_bus_size = 240;

	} else {
		max_bus_size = write_length;
	}

#else

	if (write_length > 49152) {
		max_bus_size = 49152;

	} else {
		max_bus_size = write_length;
	}

#endif

	himax_burst_enable(chip_info, 1);

	tmp_addr[3] = write_addr[3];
	tmp_addr[2] = write_addr[2];
	tmp_addr[1] = write_addr[1];
	tmp_addr[0] = write_addr[0];
	TPD_INFO("%s, write addr = 0x%02X%02X%02X%02X\n", __func__, tmp_addr[3],
		 tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	tmp_data = tp_devm_kzalloc(&chip_info->hx_spi->dev,
				   sizeof(uint8_t) * max_bus_size, GFP_KERNEL);

	if (tmp_data == NULL) {
		TPD_INFO("%s: Can't allocate enough buf \n", __func__);
		return -1;
	}

	if (total_size_temp % max_bus_size == 0) {
		total_read_times = total_size_temp / max_bus_size;

	} else {
		total_read_times = total_size_temp / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			memcpy(tmp_data, write_data + (i * max_bus_size), max_bus_size);
			himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, max_bus_size);

			total_size_temp = total_size_temp - max_bus_size;

		} else {
			test = total_size_temp % max_bus_size;
			memcpy(tmp_data, write_data + (i * max_bus_size), test);
			TPD_DEBUG("last total_size_temp=%d\n", total_size_temp % max_bus_size);

			himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, max_bus_size);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[0] = write_addr[0] + (uint8_t)((address) & 0x00FF);

		if (tmp_addr[0] <  write_addr[0]) {
			tmp_addr[1] = write_addr[1] + (uint8_t)((address >> 8) & 0x00FF) + 1;

		} else {
			tmp_addr[1] = write_addr[1] + (uint8_t)((address >> 8) & 0x00FF);
		}

		udelay(100);
	}

	TPD_DETAIL("%s, End \n", __func__);
	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)tmp_data,
		      sizeof(uint8_t) * max_bus_size);
	return 0;
}


bool himax_sense_off(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0xA5;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	msleep(20);

	do {
		/*===========================================*/
		/*  0x31 ==> 0x27*/
		/*===========================================*/
		tmp_data[0] = 0x27;

		if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================*/
		/*  0x32 ==> 0x95*/
		/*===========================================*/
		tmp_data[0] = 0x95;

		if (himax_bus_write(chip_info, 0x32, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================*/
		/* Check enter_save_mode*/
		/* ======================*/
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		TPD_INFO("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*=====================================*/
			/* Reset TCON*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			/*=====================================*/
			/* Reset ADC*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			return true;

		} else {
			msleep(10);
#ifdef HX_RST_PIN_FUNC
			himax_ic_reset(chip_info, false, false);
#endif
		}
	} while (cnt++ < 15);

	return false;
}

bool himax_sense_off_mptest(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	do {
		if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00
				 && tmp_data[0] != 0x87)) {
			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x5C;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0xA5;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
		}

		msleep(20);
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		cnt++;
		TPD_INFO("%s: save mode lock cnt = %d, data[0] = %2X!\n", __func__, cnt,
			 tmp_data[0]);
	} while (tmp_data[0] != 0x87 && (cnt < 50));

	cnt = 0;

	do {
		/*===========================================*/
		/*  0x31 ==> 0x27*/
		/*===========================================*/
		tmp_data[0] = 0x27;

		if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================*/
		/*  0x32 ==> 0x95*/
		/*===========================================*/
		tmp_data[0] = 0x95;

		if (himax_bus_write(chip_info, 0x32, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================*/
		/* Check enter_save_mode*/
		/* ======================*/
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		TPD_INFO("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*=====================================*/
			/* Reset TCON*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			/*=====================================*/
			/* Reset ADC*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			return true;

		} else {
			msleep(10);
#ifdef HX_RST_PIN_FUNC
			himax_ic_reset(chip_info, false, false);
#endif
		}
	} while (cnt++ < 15);

	return false;
}

bool himax_enter_safe_mode(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t cnt = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0xA5;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	msleep(20);

	do {
		/*===========================================*/
		/*  0x31 ==> 0x27*/
		/*===========================================*/
		tmp_data[0] = 0x27;

		if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================*/
		/*  0x32 ==> 0x95*/
		/*===========================================*/
		tmp_data[0] = 0x95;

		if (himax_bus_write(chip_info, 0x32, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================*/
		/*  0x31 ==> 0x00*/
		/*===========================================*/
		tmp_data[0] = 0x00;

		if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*msleep(10);*/
		/*===========================================*/
		/*  0x31 ==> 0x27*/
		/*===========================================*/
		tmp_data[0] = 0x27;

		if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/*===========================================*/
		/*  0x32 ==> 0x95*/
		/*===========================================*/
		tmp_data[0] = 0x95;

		if (himax_bus_write(chip_info, 0x32, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return false;
		}

		/* ======================*/
		/* Check enter_save_mode*/
		/* ======================*/
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		TPD_DETAIL("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

		if (tmp_data[0] == 0x0C) {
			/*=====================================*/
			/* Reset TCON*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x20;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			/*=====================================*/
			/* Reset ADC*/
			/*=====================================*/
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x02;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x94;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			msleep(1);
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x01;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			return true;

		} else {
			msleep(10);
#ifdef HX_RST_PIN_FUNC
			himax_ic_reset(chip_info, false, false);
#endif
		}
	} while (cnt++ < 20);

	return false;
}

void himax_interface_on(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_data[5];
	uint8_t tmp_data2[2];
	int cnt = 0;

	/*Read a dummy register to wake up I2C.*/
	if (himax_bus_read(chip_info, 0x08, 4, tmp_data) < 0) {  /* to knock I2C*/
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return;
	}

	do {
		/*===========================================*/
		/* Enable continuous burst mode : 0x13 ==> 0x31*/
		/*===========================================*/
		tmp_data[0] = 0x31;

		if (himax_bus_write(chip_info, 0x13, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		/*===========================================*/
		/* AHB address auto +4 : 0x0D ==> 0x11*/
		/* Do not AHB address auto +4 : 0x0D ==> 0x10*/
		/*===========================================*/
		tmp_data[0] = (0x10);

		if (himax_bus_write(chip_info, 0x0D, 1, tmp_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		/* Check cmd*/
		himax_bus_read(chip_info, 0x13, 1, tmp_data);
		himax_bus_read(chip_info, 0x0D, 1, tmp_data2);

		if (tmp_data[0] == 0x31 && tmp_data2[0] == 0x10) {
			/*isBusrtOn = true;*/
			break;
		}

		msleep(1);
	} while (++cnt < 10);

	if (cnt > 0) {
		TPD_INFO("%s:Polling burst mode: %d times", __func__, cnt);
	}
}

void himax_diag_register_set(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t diag_command)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	TPD_INFO("diag_command = %d\n", diag_command);

	himax_interface_on(chip_info);

	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x02;
	tmp_addr[1] = 0x04;
	tmp_addr[0] = 0xB4;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = diag_command;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	TPD_INFO("%s: tmp_data[3] = 0x%02X, tmp_data[2] = 0x%02X, tmp_data[1] = 0x%02X, tmp_data[0] = 0x%02X!\n",
		 __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
}


bool wait_wip(struct chip_data_hx83112a_nf *chip_info, int Timing)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t in_buffer[10];
	/*uint8_t out_buffer[20];*/
	int retry_cnt = 0;

	/*=====================================*/
	/* SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780*/
	/*=====================================*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x02;
	tmp_data[1] = 0x07;
	tmp_data[0] = 0x80;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	in_buffer[0] = 0x01;

	do {
		/*=====================================*/
		/* SPI Transfer Control : 0x8000_0020 ==> 0x4200_0003*/
		/*=====================================*/
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x20;
		tmp_data[3] = 0x42;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x03;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		/*=====================================*/
		/* SPI Command : 0x8000_0024 ==> 0x0000_0005*/
		/* read 0x8000_002C for 0x01, means wait success*/
		/*=====================================*/
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x05;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		in_buffer[0] = in_buffer[1] = in_buffer[2] = in_buffer[3] = 0xFF;
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x2C;
		himax_register_read(chip_info, tmp_addr, 4, in_buffer, false);

		if ((in_buffer[0] & 0x01) == 0x00) {
			return true;
		}

		retry_cnt++;

		if (in_buffer[0] != 0x00 || in_buffer[1] != 0x00 || in_buffer[2] != 0x00
				|| in_buffer[3] != 0x00) {
			TPD_INFO("%s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d \n",
				 __func__,
				 retry_cnt, in_buffer[0], in_buffer[1], in_buffer[2], in_buffer[3]);
		}

		if (retry_cnt > 100) {
			TPD_INFO("%s: Wait wip error!\n", __func__);
			return false;
		}

		msleep(Timing);
	} while ((in_buffer[0] & 0x01) == 0x01);

	return true;
}


void himax_sense_on(struct chip_data_hx83112a_nf *chip_info, uint8_t FlashMode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int retry = 0;

	TPD_DETAIL("Enter %s  \n", __func__);

	himax_interface_on(chip_info);
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
	/*msleep(20);*/

	if (!FlashMode) {
		/*===AHBI2C_SystemReset==========*/
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x18;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x55;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

	} else {
		do {
			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x98;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x53;
			himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

			tmp_addr[0] = 0xE4;
			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

			TPD_DETAIL("%s:Read status from IC = %X, %X\n", __func__, tmp_data[0],
				   tmp_data[1]);
		} while ((tmp_data[1] != 0x01 || tmp_data[0] != 0x00) && retry++ < 5);

		if (retry >= 5) {
			TPD_INFO("%s: Fail:\n", __func__);

			/*===AHBI2C_SystemReset==========*/
			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x18;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x55;
			himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

		} else {
			TPD_DETAIL("%s:OK and Read status from IC = %X, %X\n", __func__, tmp_data[0],
				   tmp_data[1]);

			/* reset code*/
			tmp_data[0] = 0x00;

			if (himax_bus_write(chip_info, 0x31, 1, tmp_data) < 0) {
				TPD_INFO("%s: i2c access fail!\n", __func__);
			}

			if (himax_bus_write(chip_info, 0x32, 1, tmp_data) < 0) {
				TPD_INFO("%s: i2c access fail!\n", __func__);
			}

			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x98;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);
		}
	}
}

/**
 * hx83112a_nf_enable_interrupt -   Device interrupt ability control.
 * @chip_info: struct include i2c resource.
 * @enable: disable or enable control purpose.
 * Return  0: succeed, -1: failed.
 */
static int hx83112a_nf_enable_interrupt(struct chip_data_hx83112a_nf *chip_info,
					bool enable)
{
	TPD_DETAIL("%s enter, enable = %d.\n", __func__, enable);

	if (enable == true && chip_info->irq_en_cnt == 0) {
		enable_irq(chip_info->hx_irq);
		chip_info->irq_en_cnt = 1;

	} else if (enable == false && chip_info->irq_en_cnt == 1) {
		disable_irq_nosync(chip_info->hx_irq);
		chip_info->irq_en_cnt = 0;

	} else {
		TPD_DETAIL("irq is not pairing! enable= %d, cnt = %d\n", enable,
			   chip_info->irq_en_cnt);
	}

	return 0;
}

#ifdef HX_ZERO_FLASH
struct himax_core_fp g_core_fp;
struct zf_operation *pzf_op = NULL;
/*char *i_CTPM_firmware_name = "Himax_firmware.bin";*/
bool g_auto_update_flag = false;
int g_power_onof = 1;

void himax_in_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len)
{
	/*TPD_INFO("%s: Entering!\n", __func__);*/
	switch (len) {
	case 1:
		cmd[0] = addr;
		/*TPD_INFO("%s: cmd[0] = 0x%02X\n", __func__, cmd[0]);*/
		break;

	case 2:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		/*TPD_INFO("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X\n", __func__, cmd[0], cmd[1]);*/
		break;

	case 4:
		cmd[0] = addr % 0x100;
		cmd[1] = (addr >> 8) % 0x100;
		cmd[2] = (addr >> 16) % 0x100;
		cmd[3] = addr / 0x1000000;
		/*  TPD_INFO("%s: cmd[0] = 0x%02X,cmd[1] = 0x%02X,cmd[2] = 0x%02X,cmd[3] = 0x%02X\n",
		    __func__, cmd[0], cmd[1], cmd[2], cmd[3]);*/
		break;

	default:
		TPD_INFO("%s: input length fault, len = %d!\n", __func__, len);
	}
}

void hx_update_dirly_0f(struct chip_data_hx83112a_nf *chip_info)
{
	TPD_INFO("It will update fw after esd event in zero flash mode!\n");
	g_core_fp.fp_0f_operation_dirly(chip_info);
}

void himax_mcu_sys_reset(struct chip_data_hx83112a_nf *chip_info)
{
	himax_register_write(chip_info, pzf_op->addr_system_reset,  4,
			     pzf_op->data_system_reset,  false);
}

int hx_dis_rload_0f(struct chip_data_hx83112a_nf *chip_info, int disable)
{
	/*Diable Flash Reload*/
	int retry = 10;
	int check_val = 0;
	uint8_t tmp_data[4] = {0};

	TPD_DETAIL("%s: Entering !\n", __func__);

	do {
		himax_flash_write_burst(chip_info, pzf_op->addr_dis_flash_reload,
					pzf_op->data_dis_flash_reload);
		himax_register_read(chip_info, pzf_op->addr_dis_flash_reload, 4, tmp_data,
				    false);
		TPD_DETAIL("Now data: tmp_data[3] = 0x%02X || tmp_data[2] = 0x%02X || tmp_data[1] = 0x%02X || tmp_data[0] = 0x%02X\n",
			   tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);

		if (tmp_data[3] != 0x00 || tmp_data[2] != 0x00 || tmp_data[1] != 0x9A
				|| tmp_data[0] != 0xA9) {
			TPD_INFO("Now data: tmp_data[3] = 0x%02X || tmp_data[2] = 0x%02X || tmp_data[1] = 0x%02X || tmp_data[0] = 0x%02X\n",
				 tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
			TPD_INFO("Not Same,Write Fail, there is %d retry times!\n", retry);

		} else {
			check_val = 1;
			TPD_DETAIL("It's same! Write success!\n");
		}

		msleep(5);
	} while (check_val == 0 && retry-- > 0);

	TPD_DETAIL("%s: END !\n", __func__);

	return check_val;
}

void himax_mcu_clean_sram_0f(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t *addr, int write_len, int type)
{
	int total_read_times = 0;
	int max_bus_size = MAX_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;

	uint8_t fix_data = 0x00;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[MAX_TRANS_SZ] = {0};

	TPD_DETAIL("%s, Entering \n", __func__);

	total_size = write_len;
	total_size_temp = write_len;

	if (total_size > MAX_TRANS_SZ) {
		max_bus_size = MAX_TRANS_SZ;
	}

	total_size_temp = write_len;

	himax_burst_enable(chip_info, 1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	TPD_DETAIL("%s, write addr tmp_addr[3] = 0x%2.2X, tmp_addr[2] = 0x%2.2X, tmp_addr[1] = 0x%2.2X, tmp_addr[0] = 0x%2.2X\n",
		   __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	switch (type) {
	case 0:
		fix_data = 0x00;
		break;

	case 1:
		fix_data = 0xAA;
		break;

	case 2:
		fix_data = 0xBB;
		break;
	}

	for (i = 0; i < MAX_TRANS_SZ; i++) {
		tmp_data[i] = fix_data;
	}

	TPD_DETAIL("%s, total size=%d\n", __func__, total_size);

	if (total_size_temp % max_bus_size == 0) {
		total_read_times = total_size_temp / max_bus_size;

	} else {
		total_read_times = total_size_temp / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		/*TPD_DETAIL("[log]write %d time start!\n", i);*/
		if (total_size_temp >= max_bus_size) {
			himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data,  max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;

		} else {
			TPD_DETAIL("last total_size_temp=%d\n", total_size_temp);
			himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data,
						       total_size_temp % max_bus_size);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

		msleep(10);
	}

	TPD_DETAIL("%s, END \n", __func__);
}

void himax_mcu_write_sram_0f(struct chip_data_hx83112a_nf *chip_info,
			     const struct firmware *fw_entry, uint8_t *addr, int start_index,
			     uint32_t write_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;

	uint8_t tmp_addr[4];
	/*uint8_t *tmp_data;*/
	uint32_t now_addr;

	TPD_DETAIL("%s, ---Entering \n", __func__);

	total_size = fw_entry->size;

	total_size_temp = write_len;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (write_len > MAX_TRANS_SZ) {
		max_bus_size = MAX_TRANS_SZ;

	} else {
		max_bus_size = write_len;
	}

#else

	if (write_len > 49152) {
		max_bus_size = 49152;

	} else {
		max_bus_size = write_len;
	}

#endif
	himax_burst_enable(chip_info, 1);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	TPD_DETAIL("%s, write addr tmp_addr[3] = 0x%2.2X, tmp_addr[2] = 0x%2.2X, tmp_addr[1] = 0x%2.2X, tmp_addr[0] = 0x%2.2X\n",
		   __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
	now_addr = (addr[3] << 24) + (addr[2] << 16) + (addr[1] << 8) + addr[0];
	TPD_DETAIL("now addr= 0x%08X\n", now_addr);

	TPD_DETAIL("%s,  total size=%d\n", __func__, total_size);

	if (chip_info->tmp_data == NULL) {
		/*TPD_INFO("%s,  enteralloc chip_info->tmp_data\n", __func__);*/
		chip_info->tmp_data = tp_devm_kzalloc(&chip_info->hx_spi->dev,
						      sizeof(uint8_t) * firmware_update_space, GFP_KERNEL);

		if (chip_info->tmp_data == NULL) {
			TPD_INFO("%s, alloc chip_info->tmp_data failed\n", __func__);
			return;
		}

		/*TPD_INFO("%s, end---------alloc chip_info->tmp_data\n", __func__);*/
	}

	memcpy(chip_info->tmp_data, fw_entry->data, total_size);

	/*
	for (i = 0;i < 10;i++) {
	    TPD_INFO("[%d] 0x%2.2X", i, tmp_data[i]);
	}
	TPD_INFO("\n");
	*/
	if (total_size_temp % max_bus_size == 0) {
		total_read_times = total_size_temp / max_bus_size;

	} else {
		total_read_times = total_size_temp / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		/*
		TPD_INFO("[log]write %d time start!\n", i);
		TPD_INFO("[log]addr[3] = 0x%02X, addr[2] = 0x%02X, addr[1] = 0x%02X, addr[0] = 0x%02X!\n", tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
		*/
		if (total_size_temp >= max_bus_size) {
			himax_flash_write_burst_length(chip_info, tmp_addr,
						       &(chip_info->tmp_data[start_index + i * max_bus_size]),  max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;

		} else {
			TPD_DETAIL("last total_size_temp=%d\n", total_size_temp);
			himax_flash_write_burst_length(chip_info, tmp_addr,
						       &(chip_info->tmp_data[start_index + i * max_bus_size]),
						       total_size_temp % max_bus_size);
		}

		/*TPD_INFO("[log]write %d time end!\n", i);*/
		address = ((i + 1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

		if (tmp_addr[0] < addr[0]) {
			tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF) + 1;

		} else {
			tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);
		}


		udelay(100);
	}

	TPD_DETAIL("%s, ----END \n", __func__);
	/*kfree (tmp_data);*/
	memset(chip_info->tmp_data, 0, total_size);
}
int himax_sram_write_crc_check(struct chip_data_hx83112a_nf *chip_info,
			       const struct firmware *fw_entry, uint8_t *addr, int strt_idx, uint32_t len)
{
	int retry = 0;
	int crc = -1;

	do {
		g_core_fp.fp_write_sram_0f(chip_info, fw_entry, addr, strt_idx, len);
		crc = himax_hw_check_CRC(chip_info, pzf_op->data_sram_start_addr, HX_48K_SZ);
		retry++;
		/*I("%s, HW CRC %s in %d time\n", __func__, (crc == 0)?"OK":"Fail", retry);*/
	} while (crc != 0 && retry < 10);

	return crc;
}
static int himax_mcu_Calculate_CRC_with_AP(unsigned char *FW_content,
		int CRC_from_FW, int len)
{
	int i, j, length = 0;
	int fw_data;
	int fw_data_2;
	int crc = 0xFFFFFFFF;
	int poly_nomial = 0x82F63B78;

	length = len / 4;

	for (i = 0; i < length; i++) {
		fw_data = FW_content[i * 4];

		for (j = 1; j < 4; j++) {
			fw_data_2 = FW_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}

		crc = fw_data ^ crc;

		for (j = 0; j < 32; j++) {
			if ((crc % 2) != 0) {
				crc = ((crc >> 1) & 0x7FFFFFFF) ^ poly_nomial;

			} else {
				crc = (((crc >> 1) & 0x7FFFFFFF)/*& 0x7FFFFFFF*/);
			}
		}

		/*I("CRC = %x, i = %d \n", CRC, i);*/
	}

	return crc;
}

bool hx_parse_bin_cfg_data(struct chip_data_hx83112a_nf *chip_info,
			   const struct firmware *fw_entry)
{
	int part_num = 0;
	int i = 0;
	uint8_t buf[16];
	int i_max = 0;
	int i_min = 0;
	uint32_t dsram_base = 0xFFFFFFFF;
	uint32_t dsram_max = 0;
	struct zf_info *zf_info_arr;

	/*1. get number of partition*/
	part_num = fw_entry->data[HX64K + 12];
	TPD_INFO("%s, Number of partition is %d\n", __func__, part_num);

	if (part_num <= 1) {
		TPD_INFO("%s, size of cfg part failed! part_num = %d\n", __func__, part_num);
		return false;
	}

	/*2. initial struct of array*/
	zf_info_arr = tp_devm_kzalloc(&chip_info->hx_spi->dev,
				      part_num * sizeof(struct zf_info), GFP_KERNEL);

	if (zf_info_arr == NULL) {
		TPD_INFO("%s, Allocate ZF info array failed!\n", __func__);
		return false;
	}

	for (i = 0; i < part_num; i++) {
		/*3. get all partition*/
		memcpy(buf, &fw_entry->data[i * 0x10 + HX64K], 16);
		memcpy(zf_info_arr[i].sram_addr, buf, 4);
		zf_info_arr[i].write_size = buf[5] << 8 | buf[4];
		zf_info_arr[i].fw_addr = buf[9] << 8 | buf[8];
		zf_info_arr[i].cfg_addr = zf_info_arr[i].sram_addr[0];
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[1] << 8;
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[2] << 16;
		zf_info_arr[i].cfg_addr += zf_info_arr[i].sram_addr[3] << 24;

		/*TPD_INFO("%s, [%d] SRAM addr = %08X\n", __func__, i, zf_info_arr[i].cfg_addr);
		TPD_INFO("%s, [%d] fw_addr = %04X!\n", __func__, i, zf_info_arr[i].fw_addr);
		TPD_INFO("%s, [%d] write_size = %d!\n", __func__, i, zf_info_arr[i].write_size); */
		if (i == 0) {
			continue;
		}

		if (dsram_base > zf_info_arr[i].cfg_addr) {
			dsram_base = zf_info_arr[i].cfg_addr;
			i_min = i;

		} else if (dsram_max < zf_info_arr[i].cfg_addr) {
			dsram_max = zf_info_arr[i].cfg_addr;
			i_max = i;
		}
	}

	for (i = 0; i < 4; i++) {
		chip_info->sram_min[i] = zf_info_arr[i_min].sram_addr[i];
	}

	chip_info->cfg_sz = (dsram_max - dsram_base) + zf_info_arr[i_max].write_size;
	chip_info->cfg_sz = chip_info->cfg_sz + (chip_info->cfg_sz % 16);

	TPD_INFO("%s, chip_info->cfg_sz = %d!, dsram_base = %X, dsram_max = %X\n",
		 __func__, chip_info->cfg_sz, dsram_base, dsram_max);

	if (chip_info->FW_buf == NULL) {
		chip_info->FW_buf = tp_devm_kzalloc(&chip_info->hx_spi->dev,
						    sizeof(unsigned char) * FW_BIN_16K_SZ, GFP_KERNEL);
	}

	for (i = 1; i < part_num; i++) {
		memcpy(chip_info->FW_buf + (zf_info_arr[i].cfg_addr - dsram_base),
		       (unsigned char *)&fw_entry->data[zf_info_arr[i].fw_addr],
		       zf_info_arr[i].write_size);
	}

	chip_info->cfg_crc = himax_mcu_Calculate_CRC_with_AP(chip_info->FW_buf, 0,
			     chip_info->cfg_sz);
	TPD_INFO("chenyunrui:hx83112a_nf_cfg_crc = %d\n", chip_info->cfg_crc);
	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)zf_info_arr,
		      part_num * sizeof(struct zf_info));
	return true;
}

static int hx83112a_nf_nf_zf_part_info(struct chip_data_hx83112a_nf *chip_info,
				       const struct firmware *fw_entry)
{
	struct timespec timeStart, timeEnd, timeDelta;
	int retry = 0;
	int crc = -1;
	bool ret = false;

	if (!hx_parse_bin_cfg_data(chip_info, fw_entry)) {
		TPD_INFO("%s, Parse cfg from bin failed\n", __func__);
	}

	himax_register_write(chip_info, pzf_op->addr_system_reset, 4,
			     pzf_op->data_system_reset, false);
	himax_enter_safe_mode(chip_info);
	getnstimeofday(&timeStart);
	/* first 48K */

	himax_sram_write_crc_check(chip_info, fw_entry, pzf_op->data_sram_start_addr, 0,
				   HX_48K_SZ);
	crc = himax_hw_check_CRC(chip_info, pzf_op->data_sram_start_addr, HX_48K_SZ);
	ret = (crc == 0) ? true : false;

	if (crc != 0) {
		TPD_INFO("48k CRC Failed! CRC = %X", crc);
	}

	do {
		himax_mcu_register_write(chip_info, chip_info->sram_min, chip_info->cfg_sz,
					 chip_info->FW_buf, 0);
		/*himax_register_write(chip_info, chip_info->sram_min, chip_info->cfg_sz, chip_info->FW_buf, 0); */
		crc = himax_hw_check_CRC(chip_info, chip_info->sram_min, chip_info->cfg_sz);

		if (crc != chip_info->cfg_crc) {
			TPD_INFO("Config CRC FAIL, HW CRC = %X, SW CRC = %X, retry time = %d", crc,
				 chip_info->cfg_crc, retry);
		}

		retry++;
	} while (!ret && retry < 10);

	if (g_power_onof == 1) {
		g_core_fp.fp_write_sram_0f(chip_info, fw_entry, pzf_op->data_mode_switch,
					   0xC33C, 4);

	} else {
		g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_mode_switch, 4, 2);
	}

	getnstimeofday(&timeEnd);
	timeDelta.tv_nsec = (timeEnd.tv_sec * 1000000000 + timeEnd.tv_nsec) -
			    (timeStart.tv_sec * 1000000000 + timeStart.tv_nsec);
	TPD_INFO("update firmware time = %ld us\n", timeDelta.tv_nsec / 1000);
	return 0;
}

void himax_mcu_firmware_update_0f(struct chip_data_hx83112a_nf *chip_info,
				  const struct firmware *fw_entry)
{
	int retry = 0;
	int crc = -1;
	int ret = 0;
	uint8_t temp_addr[4];
	uint8_t temp_data[4];
	struct firmware *request_fw_headfile = NULL;
	const struct firmware *tmp_fw_entry = NULL;
	struct monitor_data *monitor_data = chip_info->monitor_data;
	bool reload = false;

	TPD_DETAIL("%s, Entering \n", __func__);

fw_reload:

	if (fw_entry == NULL || reload) {
		TPD_INFO("Get FW from headfile\n");

		if (request_fw_headfile == NULL) {
			request_fw_headfile = tp_devm_kzalloc(&chip_info->hx_spi->dev,
							      sizeof(struct firmware), GFP_KERNEL);
		}

		if (request_fw_headfile == NULL) {
			TPD_INFO("%s kzalloc failed!\n", __func__);
			return;
		}

		if (chip_info->g_fw_sta) {
			TPD_INFO("request firmware failed, get from g_fw_buf\n");
			request_fw_headfile->size = chip_info->g_fw_len;
			request_fw_headfile->data = chip_info->g_fw_buf;
			tmp_fw_entry = request_fw_headfile;

		} else {
			tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE,
					     "Request fw from headfile");
			TPD_INFO("request firmware failed, get from headfile\n");

			if (chip_info->p_firmware_headfile->firmware_data) {
				request_fw_headfile->size = chip_info->p_firmware_headfile->firmware_size;
				request_fw_headfile->data = chip_info->p_firmware_headfile->firmware_data;
				tmp_fw_entry = request_fw_headfile;
				chip_info->using_headfile = true;

			} else {
				TPD_INFO("firmware_data is NULL! exit firmware update!\n");

				if (request_fw_headfile != NULL) {
					tp_devm_kfree(&chip_info->hx_spi->dev, (void **)request_fw_headfile,
						      sizeof(struct firmware));
					request_fw_headfile = NULL;
				}

				return;
			}
		}

	} else {
		tmp_fw_entry = fw_entry;
	}

	if ((int)tmp_fw_entry->size > HX64K) {
		ret = hx83112a_nf_nf_zf_part_info(chip_info, tmp_fw_entry);

	} else {
		himax_register_write(chip_info, pzf_op->addr_system_reset, 4,
				     pzf_op->data_system_reset, false);
		himax_enter_safe_mode(chip_info);

		/* first 48K */
		do {
			g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry,
						   pzf_op->data_sram_start_addr, 0, HX_48K_SZ);
			crc = himax_hw_check_CRC(chip_info, pzf_op->data_sram_start_addr,  HX_48K_SZ);

			temp_addr[3] = 0x08;
			temp_addr[2] = 0x00;
			temp_addr[1] = 0xBF;
			temp_addr[0] = 0xFC;
			himax_register_read(chip_info, temp_addr, 4, temp_data, false);

			TPD_DETAIL("%s, 48K LAST 4 BYTES: data[3] = 0x%02X, data[2] = 0x%02X, data[1] = 0x%02X, data[0] = 0x%02X\n",
				   __func__, temp_data[3], temp_data[2], temp_data[1], temp_data[0]);

			if (crc == 0) {
				TPD_DETAIL("%s, HW CRC OK in %d time \n", __func__, retry);
				break;

			} else {
				TPD_INFO("%s, HW CRC FAIL in %d time !\n", __func__, retry);
			}

			retry++;
		} while (((temp_data[3] == 0 && temp_data[2] == 0 && temp_data[1] == 0
				&& temp_data[0] == 0 && retry < 80) || (crc != 0 && retry < 30))
				&& !(chip_info->using_headfile && retry < 3));

		if (crc != 0) {
			tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "HW CRC Failed");
			TPD_INFO("Last time CRC Fail!\n");

			if (reload) {
				return;

			} else {
				reload = true;
				goto fw_reload;
			}
		}

		/* if g_power_onof!= 1, it will be clean mode! */
		/*config and setting */
		/*config info*/
		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_cfg_info,
							   0xC000, 128);/*132*/
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_cfg_info, 128);

				if (crc == 0) {
					TPD_DETAIL("%s, config info ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, config info fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "Config info CRC Failed");
				TPD_INFO("config info CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_cfg_info, 128, 2);
		}

		/*FW config*/
		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_fw_cfg_p1,
							   0xC100, 528);/*482*/
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_fw_cfg_p1, 528);

				if (crc == 0) {
					TPD_DETAIL("%s, 1 FW config ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, 1 FW config fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "FW config 1 CRC Failed");
				TPD_INFO("1 FW config CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_fw_cfg_p1, 528, 1);
		}

		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_fw_cfg_p3,
							   0xCA00, 128);
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_fw_cfg_p3, 128);

				if (crc == 0) {
					TPD_DETAIL("%s, 3 FW config ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, 3 FW config fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "FW config 3 CRC Failed");
				TPD_INFO("3 FW config CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_fw_cfg_p3, 128, 1);
		}

		/*ADC config*/
		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_adc_cfg_1,
							   0xD640, 1200);
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_adc_cfg_1, 1200);

				if (crc == 0) {
					TPD_DETAIL("%s, 1 ADC config ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, 1 ADC config fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "ADC config 1 CRC Failed");
				TPD_INFO("1 ADC config CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_adc_cfg_1, 1200, 2);
		}

		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_adc_cfg_2,
							   0xD320, 800);
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_adc_cfg_2, 800);

				if (crc == 0) {
					TPD_DETAIL("%s, 2 ADC config ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, 2 ADC config fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "ADC config 2 CRC Failed");
				TPD_INFO("2 ADC config CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_adc_cfg_2, 800, 2);
		}

		/*mapping table*/
		if (g_power_onof == 1) {
			retry = 0;

			do {
				g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_map_table,
							   0xE000, 1536);
				crc = himax_hw_check_CRC(chip_info, pzf_op->data_map_table, 1536);

				if (crc == 0) {
					TPD_DETAIL("%s, mapping table ok in %d time \n", __func__, retry);
					break;

				} else {
					TPD_INFO("%s, mapping table fail in %d time !\n", __func__, retry);
				}

				retry++;
			} while ((crc != 0 && retry < 30) || (chip_info->using_headfile && retry < 15));

			if (crc != 0) {
				tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE,
						     "Mapping table CRC Failed");
				TPD_INFO("mapping table CRC Fail!\n");

				if (!reload) {
					reload = true;
					goto fw_reload;
				}
			}

		} else {
			g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_map_table, 1536, 2);
		}
	}

	/* switch mode*/
	if (g_power_onof == 1) {
		g_core_fp.fp_write_sram_0f(chip_info, tmp_fw_entry, pzf_op->data_mode_switch,
					   0xC30C, 4);

	} else {
		g_core_fp.fp_clean_sram_0f(chip_info, pzf_op->data_mode_switch, 4, 2);
	}

	hx83112a_nf_fw_check(private_ts->chip_data, &private_ts->resolution_info,
			     &private_ts->panel_data);

	if (request_fw_headfile != NULL) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)request_fw_headfile,
			      sizeof(struct firmware));
		request_fw_headfile = NULL;
	}

	tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE, "FW update Success");
	TPD_DETAIL("%s, END \n", __func__);
}
int hx_0f_op_file_dirly(struct chip_data_hx83112a_nf *chip_info,
			char *file_name)
{
	int err = NO_ERR;
	const struct firmware *fw_entry = NULL;


	TPD_INFO("%s, Entering \n", __func__);
	TPD_INFO("file name = %s\n", file_name);

	if (private_ts->fw_update_app_support) {
		err = request_firmware_select(&fw_entry, file_name, private_ts->dev);

	} else {
		err = request_firmware(&fw_entry, file_name, private_ts->dev);
	}

	if (err < 0) {
		TPD_INFO("%s, fail in line%d error code=%d, file maybe fail\n", __func__,
			 __LINE__, err);
		return err;
	}

	/*himax_int_enable (0);*/

	if (chip_info->g_f_0f_updat == 1) {
		TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
		release_firmware(fw_entry);
		err = -1;
		return err;

	} else {
		TPD_INFO("%s:Entering Update Flow!\n", __func__);
		chip_info->g_f_0f_updat = 1;
	}

	hx83112a_nf_enable_interrupt(chip_info, false);

	/* trigger reset */
	hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
	hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/

	g_core_fp.fp_firmware_update_0f(chip_info, fw_entry);
	release_firmware(fw_entry);

	chip_info->g_f_0f_updat = 0;
	TPD_INFO("%s, END \n", __func__);
	return err;
}

int himax_mcu_0f_operation_dirly(struct chip_data_hx83112a_nf *chip_info)
{
	int err = NO_ERR;
	/*const struct firmware *fw_entry = NULL;*/

	TPD_DETAIL("%s, Entering \n", __func__);

	if (chip_info->g_f_0f_updat == 1) {
		TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
		err = -1;
		return err;

	} else {
		TPD_INFO("%s:Entering Update Flow!\n", __func__);
		chip_info->g_f_0f_updat = 1;
	}

	/*hx83112a_nf_enable_interrupt(chip_info, false);*/

	g_core_fp.fp_firmware_update_0f(chip_info, NULL);
	/*release_firmware (chip_info->g_fw_entry);*/

	chip_info->g_f_0f_updat = 0;
	TPD_DETAIL("%s, END \n", __func__);
	return err;
}

int himax_mcu_0f_operation_test_dirly(struct chip_data_hx83112a_nf *chip_info,
				      const struct firmware *fw_entry)
{
	int err = NO_ERR;

	TPD_DETAIL("%s, Entering \n", __func__);

	if (fw_entry == NULL) {
		TPD_INFO("%s, fail in line%d test firmware is NULL\n", __func__, __LINE__);
		return -1;
	}

	hx83112a_nf_enable_interrupt(chip_info, false);

	g_core_fp.fp_firmware_update_0f(chip_info, fw_entry);
	release_firmware(fw_entry);

	TPD_DETAIL("%s, END \n", __func__);
	return err;
}

void himax_mcu_0f_operation(struct work_struct *work)
{
	struct chip_data_hx83112a_nf *chip_info = container_of(work,
			struct chip_data_hx83112a_nf, work_0f_update.work);
	TPD_INFO("%s, Entering \n", __func__);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (private_ts->boot_mode != RECOVERY_BOOT  && !is_oem_unlocked())
#else
	if (private_ts->boot_mode != MSM_BOOT_MODE__RECOVERY  && !is_oem_unlocked())
#endif
	{
		TPD_INFO("file name = %s\n", private_ts->panel_data.fw_name);

	} else {
		TPD_INFO("TP firmware has been requested.\n");
	}

	if (chip_info->g_f_0f_updat == 1) {
		TPD_INFO("%s:[Warning]Other thread is updating now!\n", __func__);
		return;

	} else {
		TPD_INFO("%s:Entering Update Flow!\n", __func__);
		chip_info->g_f_0f_updat = 1;
	}

	hx83112a_nf_enable_interrupt(chip_info, false);

	/* trigger reset */
	hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
	hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/

	g_core_fp.fp_firmware_update_0f(chip_info, NULL);
	/*release_firmware (chip_info->g_fw_entry);*/

	g_core_fp.fp_reload_disable(chip_info, 0);
	msleep(10);
	himax_read_FW_ver(chip_info);
	msleep(10);
	himax_sense_on(chip_info, 0x00);
	msleep(10);
	TPD_INFO("%s:End \n", __func__);

#ifdef CONFIG_OPLUS_TP_APK

	if (chip_info->debug_mode_sta) {
		if (private_ts->apk_op && private_ts->apk_op->apk_debug_set) {
			private_ts->apk_op->apk_debug_set(private_ts->chip_data, true);
		}
	}

#endif /* end of CONFIG_OPLUS_TP_APK*/

	hx83112a_nf_enable_interrupt(chip_info, true);

	chip_info->g_f_0f_updat = 0;
	TPD_INFO("%s, END \n", __func__);
	return;
}

#ifdef HX_0F_DEBUG
void himax_mcu_read_sram_0f(struct chip_data_hx83112a_nf *chip_info,
			    const struct firmware *fw_entry, uint8_t *addr, int start_index, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0, j = 0;
	int not_same = 0;

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;
	int *not_same_buff;

	TPD_INFO("%s, Entering \n", __func__);

	himax_burst_enable(chip_info, 1);

	total_size = read_len;

	total_size_temp = read_len;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (read_len > MAX_RECVS_SZ) {
		max_bus_size = MAX_RECVS_SZ;

	} else {
		max_bus_size = read_len;
	}

#else

	if (read_len > 2048) {
		max_bus_size = 2048;

	} else {
		max_bus_size = read_len;
	}

#endif
	temp_info_data = tp_devm_kzalloc(&chip_info->hx_spi->dev,
					 sizeof(uint8_t) * total_size, GFP_KERNEL);
	not_same_buff = tp_devm_kzalloc(&chip_info->hx_spi->dev,
					sizeof(int) * total_size, GFP_KERNEL);


	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	TPD_INFO("%s, read addr tmp_addr[3] = 0x%2.2X, tmp_addr[2] = 0x%2.2X, tmp_addr[1] = 0x%2.2X, tmp_addr[0] = 0x%2.2X\n",
		 __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	TPD_INFO("%s, total size=%d\n", __func__, total_size);

	himax_burst_enable(chip_info, 1);

	if (total_size % max_bus_size == 0) {
		total_read_times = total_size / max_bus_size;

	} else {
		total_read_times = total_size / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			himax_register_read(chip_info, tmp_addr, max_bus_size,
					    &temp_info_data[i * max_bus_size], false);
			total_size_temp = total_size_temp - max_bus_size;

		} else {
			himax_register_read(chip_info, tmp_addr, total_size_temp % max_bus_size,
					    &temp_info_data[i * max_bus_size], false);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

		if (tmp_addr[0] < addr[0]) {
			tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF) + 1;

		} else {
			tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);
		}

		msleep(10);
	}

	TPD_INFO("%s, READ Start \n", __func__);
	TPD_INFO("%s, start_index = %d \n", __func__, start_index);
	j = start_index;

	for (i = 0; i < read_len; i++, j++) {
		if (chip_info->g_fw_buf[j] != temp_info_data[i]) {
			not_same++;
			not_same_buff[i] = 1;
		}

		TPD_INFO("0x%2.2X, ", temp_info_data[i]);

		if (i > 0 && i % 16 == 15) {
			printk("\n");
		}
	}

	TPD_INFO("%s, READ END \n", __func__);
	TPD_INFO("%s, Not Same count=%d\n", __func__, not_same);

	if (not_same != 0) {
		j = start_index;

		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1) {
				TPD_INFO("bin = [%d] 0x%2.2X\n", i, chip_info->g_fw_buf[j]);
			}
		}

		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1) {
				TPD_INFO("sram = [%d] 0x%2.2X \n", i, temp_info_data[i]);
			}
		}
	}

	TPD_INFO("%s, READ END \n", __func__);
	TPD_INFO("%s, Not Same count=%d\n", __func__, not_same);
	TPD_INFO("%s, END \n", __func__);

	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)not_same_buff,
		      sizeof(int) * total_size);
	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_info_data,
		      sizeof(uint8_t) * total_size);
}

void himax_mcu_read_all_sram(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t *addr, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_RECVS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;
	/*
	struct file *fn;
	struct filename *vts_name;
	*/

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;

	TPD_INFO("%s, Entering \n", __func__);

	himax_burst_enable(chip_info, 1);

	total_size = read_len;

	total_size_temp = read_len;

	temp_info_data = tp_devm_kzalloc(&chip_info->hx_spi->dev,
					 sizeof(uint8_t) * total_size, GFP_KERNEL);


	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	TPD_INFO("%s, read addr tmp_addr[3] = 0x%2.2X, tmp_addr[2] = 0x%2.2X, tmp_addr[1] = 0x%2.2X, tmp_addr[0] = 0x%2.2X\n",
		 __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);

	TPD_INFO("%s, total size=%d\n", __func__, total_size);

	if (total_size % max_bus_size == 0) {
		total_read_times = total_size / max_bus_size;

	} else {
		total_read_times = total_size / max_bus_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			himax_register_read(chip_info, tmp_addr, max_bus_size,
					    &temp_info_data[i * max_bus_size], false);
			total_size_temp = total_size_temp - max_bus_size;

		} else {
			himax_register_read(chip_info, tmp_addr, total_size_temp % max_bus_size,
					    &temp_info_data[i * max_bus_size], false);
		}

		address = ((i + 1) * max_bus_size);
		tmp_addr[1] = addr[1] + (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = addr[0] + (uint8_t)((address) & 0x00FF);

		msleep(10);
	}

	TPD_INFO("%s, NOW addr tmp_addr[3] = 0x%2.2X, tmp_addr[2] = 0x%2.2X, tmp_addr[1] = 0x%2.2X, tmp_addr[0] = 0x%2.2X\n",
		 __func__, tmp_addr[3], tmp_addr[2], tmp_addr[1], tmp_addr[0]);
	/*for (i = 0;i < read_len;i++) {
	    TPD_INFO("0x%2.2X, ", temp_info_data[i]);

	    if (i > 0 && i%16 == 15)
	        printk("\n");
	}*/

	/* need modify
	TPD_INFO("Now Write File start!\n");
	vts_name = getname_kernel("/sdcard/dump_dsram.txt");
	fn = file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
	if (!IS_ERR (fn)) {
	    TPD_INFO("%s create file and ready to write\n", __func__);
	    fn->f_op->write (fn, temp_info_data, read_len*sizeof (uint8_t), &fn->f_pos);
	    filp_close (fn, NULL);
	}
	TPD_INFO("Now Write File End!\n");
	*/

	TPD_INFO("%s, END \n", __func__);

	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_info_data,
		      sizeof(uint8_t) * total_size);
}

void himax_mcu_firmware_read_0f(struct chip_data_hx83112a_nf *chip_info,
				const struct firmware *fw_entry, int type)
{
	uint8_t tmp_addr[4] = {0};

	TPD_INFO("%s, Entering \n", __func__);

	if (type == 0) { /* first 48K */
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_sram_start_addr, 0,
					  HX_48K_SZ);
		g_core_fp.fp_read_all_sram(chip_info, tmp_addr, 0xC000);

	} else { /*last 16k*/
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_cfg_info, 0xC000,
					  132);
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_fw_cfg, 0xC0FE,
					  512);
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_adc_cfg_1, 0xD000,
					  376);
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_adc_cfg_2, 0xD178,
					  376);
		g_core_fp.fp_read_sram_0f(chip_info, fw_entry, pzf_op->data_adc_cfg_3, 0xD000,
					  376);
		g_core_fp.fp_read_all_sram(chip_info, pzf_op->data_sram_clean, HX_32K_SZ);
	}

	TPD_INFO("%s, END \n", __func__);
}

void himax_mcu_0f_operation_check(struct chip_data_hx83112a_nf *chip_info,
				  int type)
{
	TPD_INFO("%s, Entering \n", __func__);

	TPD_INFO("first 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n", chip_info->g_fw_buf[0],
		 chip_info->g_fw_buf[1], chip_info->g_fw_buf[2], chip_info->g_fw_buf[3]);
	TPD_INFO("next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n", chip_info->g_fw_buf[4],
		 chip_info->g_fw_buf[5], chip_info->g_fw_buf[6], chip_info->g_fw_buf[7]);
	TPD_INFO("and next 4 bytes 0x%2X, 0x%2X, 0x%2X, 0x%2X !\n",
		 chip_info->g_fw_buf[8], chip_info->g_fw_buf[9], chip_info->g_fw_buf[10],
		 chip_info->g_fw_buf[11]);

	g_core_fp.fp_firmware_read_0f(chip_info, NULL, type);

	/*release_firmware(chip_info->g_fw_entry);*/
	TPD_INFO("%s, END \n", __func__);
	return;
}
#endif


int hx_0f_init(void)
{
	pzf_op = kzalloc(sizeof(struct zf_operation), GFP_KERNEL);

	g_core_fp.fp_reload_disable = hx_dis_rload_0f;
	g_core_fp.fp_sys_reset = himax_mcu_sys_reset;
	g_core_fp.fp_clean_sram_0f = himax_mcu_clean_sram_0f;
	g_core_fp.fp_write_sram_0f = himax_mcu_write_sram_0f;
	g_core_fp.fp_firmware_update_0f = himax_mcu_firmware_update_0f;
	g_core_fp.fp_0f_operation = himax_mcu_0f_operation;
	g_core_fp.fp_0f_operation_dirly = himax_mcu_0f_operation_dirly;
	g_core_fp.fp_0f_op_file_dirly = hx_0f_op_file_dirly;
#ifdef HX_0F_DEBUG
	g_core_fp.fp_read_sram_0f = himax_mcu_read_sram_0f;
	g_core_fp.fp_read_all_sram = himax_mcu_read_all_sram;
	g_core_fp.fp_firmware_read_0f = himax_mcu_firmware_read_0f;
	g_core_fp.fp_0f_operation_check = himax_mcu_0f_operation_check;
#endif

	himax_in_parse_assign_cmd(zf_addr_dis_flash_reload,
				  pzf_op->addr_dis_flash_reload, sizeof(pzf_op->addr_dis_flash_reload));
	himax_in_parse_assign_cmd(zf_data_dis_flash_reload,
				  pzf_op->data_dis_flash_reload, sizeof(pzf_op->data_dis_flash_reload));
	himax_in_parse_assign_cmd(ZF_ADDR_SYSTEM_RESET, pzf_op->addr_system_reset,
				  sizeof(pzf_op->addr_system_reset));
	himax_in_parse_assign_cmd(ZF_DATA_SYSTEM_RESET, pzf_op->data_system_reset,
				  sizeof(pzf_op->data_system_reset));
	himax_in_parse_assign_cmd(ZF_DATA_SRAM_START_ADDR, pzf_op->data_sram_start_addr,
				  sizeof(pzf_op->data_sram_start_addr));
	himax_in_parse_assign_cmd(ZF_DATA_SRAM_CLEAN, pzf_op->data_sram_clean,
				  sizeof(pzf_op->data_sram_clean));
	himax_in_parse_assign_cmd(ZF_DATA_CFG_INFO, pzf_op->data_cfg_info,
				  sizeof(pzf_op->data_cfg_info));
	himax_in_parse_assign_cmd(ZF_DATA_FW_CFG_P1, pzf_op->data_fw_cfg_p1,
				  sizeof(pzf_op->data_fw_cfg_p1));
	himax_in_parse_assign_cmd(ZF_DATA_FW_CFG_P2, pzf_op->data_fw_cfg_p2,
				  sizeof(pzf_op->data_fw_cfg_p2));
	himax_in_parse_assign_cmd(ZF_DATA_FW_CFG_P3, pzf_op->data_fw_cfg_p3,
				  sizeof(pzf_op->data_fw_cfg_p3));
	himax_in_parse_assign_cmd(ZF_DATA_ADC_CFG_1, pzf_op->data_adc_cfg_1,
				  sizeof(pzf_op->data_adc_cfg_1));
	himax_in_parse_assign_cmd(ZF_DATA_ADC_CFG_2, pzf_op->data_adc_cfg_2,
				  sizeof(pzf_op->data_adc_cfg_2));
	himax_in_parse_assign_cmd(ZF_DATA_ADC_CFG_3, pzf_op->data_adc_cfg_3,
				  sizeof(pzf_op->data_adc_cfg_3));
	himax_in_parse_assign_cmd(ZF_DATA_MAP_TALBLE, pzf_op->data_map_table,
				  sizeof(pzf_op->data_map_table));
	himax_in_parse_assign_cmd(ZF_DATA_MODE_SWITCH, pzf_op->data_mode_switch,
				  sizeof(pzf_op->data_mode_switch));


	return 0;
}

#endif

bool himax_ic_package_check(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t ret_data = 0x00;
	int i = 0;

	hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/
	hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
	hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/
	msleep(5);

	himax_enter_safe_mode(chip_info);

	for (i = 0; i < 5; i++) {
		/* Product ID*/
		/* Touch*/
		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xD0;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		TPD_INFO("%s:Read driver IC ID = %X, %X, %X\n", __func__, tmp_data[3],
			 tmp_data[2], tmp_data[1]);

		/*if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x11) && ((tmp_data[1] == 0x2a) || (tmp_data[1] == 0x2b)))*/
		if (1) {
			hx_0f_init();
			/*Himax: Set FW and CFG Flash Address*/
			chip_info->FW_VER_MAJ_FLASH_ADDR   = 49157;  /*0x00C005*/
			chip_info->FW_VER_MAJ_FLASH_LENG   = 1;
			chip_info->FW_VER_MIN_FLASH_ADDR   = 49158;  /*0x00C006*/
			chip_info->FW_VER_MIN_FLASH_LENG   = 1;
			chip_info->CFG_VER_MAJ_FLASH_ADDR = 49408;  /*0x00C100*/
			chip_info->CFG_VER_MAJ_FLASH_LENG = 1;
			chip_info->CFG_VER_MIN_FLASH_ADDR = 49409;  /*0x00C101*/
			chip_info->CFG_VER_MIN_FLASH_LENG = 1;
			chip_info->CID_VER_MAJ_FLASH_ADDR = 49154;  /*0x00C002*/
			chip_info->CID_VER_MAJ_FLASH_LENG = 1;
			chip_info->CID_VER_MIN_FLASH_ADDR = 49155;  /*0x00C003*/
			chip_info->CID_VER_MIN_FLASH_LENG = 1;
			/*PANEL_VERSION_ADDR = 49156;  0x00C004*/
			/*PANEL_VERSION_LENG = 1;*/
#ifdef HX_AUTO_UPDATE_FW
			g_i_FW_VER = i_CTPM_FW[chip_info->FW_VER_MAJ_FLASH_ADDR] << 8 |
				     i_CTPM_FW[chip_info->FW_VER_MIN_FLASH_ADDR];
			g_i_CFG_VER = i_CTPM_FW[chip_info->CFG_VER_MAJ_FLASH_ADDR] << 8 |
				      i_CTPM_FW[chip_info->CFG_VER_MIN_FLASH_ADDR];
			g_i_CID_MAJ = i_CTPM_FW[chip_info->CID_VER_MAJ_FLASH_ADDR];
			g_i_CID_MIN = i_CTPM_FW[chip_info->CID_VER_MIN_FLASH_ADDR];
#endif
			TPD_INFO("Himax IC package 83112_in\n");
			ret_data = true;
			break;

		} else {
			ret_data = false;
			TPD_INFO("%s:Read driver ID register Fail:\n", __func__);
		}
	}

	return ret_data;
}


void himax_power_on_init(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	TPD_INFO("%s\n", __func__);

	/*RawOut select initial*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x02;
	tmp_addr[1] = 0x04;
	tmp_addr[0] = 0xB4;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

	/*DSRAM func initial*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x07;
	tmp_addr[0] = 0xFC;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

	himax_sense_on(chip_info, 0x00);
}


static void himax_read_FW_ver(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t cmd[4];
	uint8_t data[64];
	uint8_t data2[64];
	int retry = 200;
	int reload_status = 0;

	himax_sense_on(chip_info, 0);

	while (reload_status == 0) {
		cmd[3] = 0x10;  /* oplus fw id bin address : 0xc014   , 49172    Tp ic address : 0x 10007014*/
		cmd[2] = 0x00;
		cmd[1] = 0x7f;
		cmd[0] = 0x00;
		himax_register_read(chip_info, cmd, 4, data, false);

		cmd[3] = 0x10;
		cmd[2] = 0x00;
		cmd[1] = 0x72;
		cmd[0] = 0xc0;
		himax_register_read(chip_info, cmd, 4, data2, false);

		if ((data[1] == 0x3A && data[0] == 0xA3) || (data2[1] == 0x72
				&& data2[0] == 0xc0)) {
			TPD_INFO("reload OK! \n");
			reload_status = 1;
			break;

		} else if (retry == 0) {
			TPD_INFO("reload 20 times! fail \n");
			return;

		} else {
			retry--;
			msleep(10);
			TPD_INFO("reload fail, delay 10ms retry=%d\n", retry);
		}
	}

	TPD_INFO("%s : data[0] = 0x%2.2X, data[1] = 0x%2.2X, data[2] = 0x%2.2X, data[3] = 0x%2.2X\n",
		 __func__, data[0], data[1], data[2], data[3]);
	TPD_INFO("reload_status=%d\n", reload_status);

	himax_sense_off(chip_info);

	/*=====================================*/
	/* Read FW version : 0x1000_7004  but 05,06 are the real addr for FW Version*/
	/*=====================================*/

	cmd[3] = 0x10;
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x04;
	himax_register_read(chip_info, cmd, 4, data, false);


	TPD_INFO("PANEL_VER : %X \n", data[0]);
	TPD_INFO("FW_VER : %X \n", data[1] << 8 | data[2]);

	cmd[3] = 0x10;
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x84;
	himax_register_read(chip_info, cmd, 4, data, false);

	TPD_INFO("CFG_VER : %X \n", data[2] << 8 | data[3]);
	TPD_INFO("TOUCH_VER : %X \n", data[2]);
	TPD_INFO("DISPLAY_VER : %X \n", data[3]);

	cmd[3] = 0x10;
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x00;
	himax_register_read(chip_info, cmd, 4, data, false);

	TPD_INFO("CID_VER : %X \n", (data[2] << 8 | data[3]));
	return;
}


void himax_read_OPLUS_FW_ver(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t cmd[4];
	uint8_t data[4];
	uint32_t touch_ver = 0;

	cmd[3] = 0x10;  /* oplus fw id bin address : 0xc014    Tp ic address : 0x 10007014*/
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x14;
	himax_register_read(chip_info, cmd, 4, data, false);
	chip_info->fw_id = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
	TPD_INFO("%s : data[0] = 0x%2.2X, data[1] = 0x%2.2X, data[2] = 0x%2.2X, data[3] = 0x%2.2X\n",
		 __func__, data[0], data[1], data[2], data[3]);

	cmd[3] = 0x10;  /* oplus fw id bin address : 0xc014    Tp ic address : 0x 10007014*/
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x84;
	himax_register_read(chip_info, cmd, 4, data, false);
	touch_ver = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
	TPD_INFO("%s :touch_ver = 0x%08X\n", __func__, touch_ver);

	cmd[3] = 0x10;
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x00;
	himax_register_read(chip_info, cmd, 4, data, false);
	chip_info->fw_ver = data[2] << 8 | data[3];
	TPD_INFO("%s :fw_Ver = 0x%04X \n", __func__, chip_info->fw_ver);
	return;
}

uint32_t himax_hw_check_CRC(struct chip_data_hx83112a_nf *chip_info,
			    uint8_t *start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int cnt = 0;
	int length = reload_length / 4;

	/*CRC4  0x8005_0020 <= from, 0x8005_0028 <= 0x0099_length*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x05;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x20;
	/*tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0xFB; tmp_data[0] = 0x00;*/
	himax_flash_write_burst(chip_info, tmp_addr, start_addr);

	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x05;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x28;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	cnt = 0;
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x05;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x00;

	do {
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		if ((tmp_data[0] & 0x01) != 0x01) {
			tmp_addr[3] = 0x80;
			tmp_addr[2] = 0x05;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x18;
			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
			TPD_DETAIL("%s: tmp_data[3]=%X, tmp_data[2]=%X, tmp_data[1]=%X, tmp_data[0]=%X  \n",
				   __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
			result = ((tmp_data[3] << 24) + (tmp_data[2] << 16) + (tmp_data[1] << 8) +
				  tmp_data[0]);
			break;
		}
	} while (cnt++ < 100);

	return result;
}


bool himax_calculateChecksum(struct chip_data_hx83112a_nf *chip_info,
			     bool change_iref)
{
	uint8_t CRC_result = 0;
	uint8_t tmp_data[4];

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;

	CRC_result = himax_hw_check_CRC(chip_info, tmp_data, FW_SIZE_64K);

	msleep(50);

	return !CRC_result;
}

int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max)
{
	int raw_data_len;

	if (raw_cnt_rmd != 0x00) {
		raw_data_len = 128 - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;

	} else {
		raw_data_len = 128 - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;
	}

	return raw_data_len;
}


int himax_report_data_init(struct chip_data_hx83112a_nf *chip_info,
			   int max_touch_point, int tx_num, int rx_num)
{
	if (chip_info->hx_touch_data->hx_coord_buf != NULL) {
		tp_devm_kfree(&chip_info->hx_spi->dev,
			      (void **)chip_info->hx_touch_data->hx_coord_buf,
			      sizeof(uint8_t) * (chip_info->hx_touch_data->touch_info_size));
	}

	if (chip_info->hx_touch_data->diag_mutual != NULL) {
		tp_devm_kfree(&chip_info->hx_spi->dev,
			      (void **)chip_info->hx_touch_data->diag_mutual,
			      tx_num * rx_num * sizeof(int32_t));
	}

	/*#if defined(HX_SMART_WAKEUP)*/
	chip_info->hx_touch_data->event_size = 128;
	/*#endif*/

	chip_info->hx_touch_data->touch_all_size = 128; /*himax_get_touch_data_size();*/

	chip_info->touch_info_point_cnt = max_touch_point * 4;

	if ((max_touch_point % 4) == 0) {
		chip_info->touch_info_point_cnt += (max_touch_point / 4) * 4;

	} else {
		chip_info->touch_info_point_cnt += ((max_touch_point / 4) + 1) * 4;
	}

	chip_info->hx_touch_data->raw_cnt_max = max_touch_point / 4;
	chip_info->hx_touch_data->raw_cnt_rmd = max_touch_point % 4;

	if (chip_info->hx_touch_data->raw_cnt_rmd != 0x00) { /*more than 4 fingers*/
		chip_info->hx_touch_data->rawdata_size = cal_data_len(
					chip_info->hx_touch_data->raw_cnt_rmd, max_touch_point,
					chip_info->hx_touch_data->raw_cnt_max);
		chip_info->hx_touch_data->touch_info_size = (max_touch_point +
				chip_info->hx_touch_data->raw_cnt_max + 2) * 4;

	} else { /*less than 4 fingers*/
		chip_info->hx_touch_data->rawdata_size = cal_data_len(
					chip_info->hx_touch_data->raw_cnt_rmd, max_touch_point,
					chip_info->hx_touch_data->raw_cnt_max);
		chip_info->hx_touch_data->touch_info_size = (max_touch_point +
				chip_info->hx_touch_data->raw_cnt_max + 1) * 4;
	}

	if ((tx_num * rx_num + tx_num + rx_num) % chip_info->hx_touch_data->rawdata_size
			== 0) {
		chip_info->hx_touch_data->rawdata_frame_size = (tx_num * rx_num + tx_num +
				rx_num) / chip_info->hx_touch_data->rawdata_size;

	} else {
		chip_info->hx_touch_data->rawdata_frame_size = (tx_num * rx_num + tx_num +
				rx_num) / chip_info->hx_touch_data->rawdata_size + 1;
	}

	TPD_INFO("%s: rawdata_frame_size = %d ", __func__,
		 chip_info->hx_touch_data->rawdata_frame_size);
	TPD_INFO("%s: max_touch_point:%d, hx_raw_cnt_max:%d, hx_raw_cnt_rmd:%d, g_hx_rawdata_size:%d, chip_info->hx_touch_data->touch_info_size:%d\n",
		 __func__, max_touch_point, chip_info->hx_touch_data->raw_cnt_max,
		 chip_info->hx_touch_data->raw_cnt_rmd, chip_info->hx_touch_data->rawdata_size,
		 chip_info->hx_touch_data->touch_info_size);

	chip_info->hx_touch_data->hx_coord_buf = tp_devm_kzalloc(
				&chip_info->hx_spi->dev,
				sizeof(uint8_t) * (chip_info->hx_touch_data->touch_info_size), GFP_KERNEL);

	if (chip_info->hx_touch_data->hx_coord_buf == NULL) {
		goto mem_alloc_fail;
	}

	chip_info->hx_touch_data->diag_mutual = tp_devm_kzalloc(&chip_info->hx_spi->dev,
						tx_num * rx_num * sizeof(int32_t), GFP_KERNEL);

	if (chip_info->hx_touch_data->diag_mutual == NULL) {
		goto mem_alloc_fail;
	}

	/*#ifdef HX_TP_PROC_DIAG*/
	chip_info->hx_touch_data->hx_rawdata_buf = tp_devm_kzalloc(
				&chip_info->hx_spi->dev,
				sizeof(uint8_t) * (chip_info->hx_touch_data->touch_all_size -
						chip_info->hx_touch_data->touch_info_size), GFP_KERNEL);

	if (chip_info->hx_touch_data->hx_rawdata_buf == NULL) {
		goto mem_alloc_fail;
	}

	/*#endif*/

	/*#if defined(HX_SMART_WAKEUP)*/
	chip_info->hx_touch_data->hx_event_buf = tp_devm_kzalloc(
				&chip_info->hx_spi->dev,
				sizeof(uint8_t) * (chip_info->hx_touch_data->event_size), GFP_KERNEL);

	if (chip_info->hx_touch_data->hx_event_buf == NULL) {
		goto mem_alloc_fail;
	}

	/*#endif*/

	return NO_ERR;

mem_alloc_fail:
	tp_devm_kfree(&chip_info->hx_spi->dev,
		      (void **)chip_info->hx_touch_data->hx_coord_buf,
		      sizeof(uint8_t) * (chip_info->hx_touch_data->touch_info_size));
	/*#if defined(HX_TP_PROC_DIAG)*/
	tp_devm_kfree(&chip_info->hx_spi->dev,
		      (void **)chip_info->hx_touch_data->hx_rawdata_buf,
		      sizeof(uint8_t) * (chip_info->hx_touch_data->touch_all_size -
					 chip_info->hx_touch_data->touch_info_size));
	/*#endif*/
	/*#if defined(HX_SMART_WAKEUP)*/
	tp_devm_kfree(&chip_info->hx_spi->dev,
		      (void **)chip_info->hx_touch_data->hx_event_buf,
		      sizeof(uint8_t) * (chip_info->hx_touch_data->event_size));
	/*#endif*/

	TPD_INFO("%s: Memory allocate fail!\n", __func__);
	return MEM_ALLOC_FAIL;
}

bool himax_read_event_stack(struct chip_data_hx83112a_nf *chip_info,
			    uint8_t *buf, uint8_t length)
{
	uint8_t cmd[4];

	/*  AHB_I2C Burst Read Off*/
	cmd[0] = 0x00;

	if (himax_bus_write(chip_info, 0x11, 1, cmd) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return 0;
	}

	himax_bus_read(chip_info, 0x30, length, buf);

	/*  AHB_I2C Burst Read On*/
	cmd[0] = 0x01;

	if (himax_bus_write(chip_info, 0x11, 1, cmd) < 0) {
		TPD_INFO("%s: i2c access fail!\n", __func__);
		return 0;
	}

	return 1;
}

int g_zero_event_count = 0;
int himax_ic_esd_recovery(int hx_esd_event, int hx_zero_event, int length)
{
	if (hx_esd_event == length) {
		g_zero_event_count = 0;
		goto checksum_fail;

	} else if (hx_zero_event == length) {
		g_zero_event_count++;
		TPD_INFO("[HIMAX TP MSG]: ALL Zero event is %d times.\n", g_zero_event_count);

		if (g_zero_event_count > 5) {
			g_zero_event_count = 0;
			TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
			goto checksum_fail;
		}

		goto err_workqueue_out;
	}

checksum_fail:
	return CHECKSUM_FAIL;
err_workqueue_out:
	return WORK_OUT;
}

static int hx83112a_nf_resetgpio_set(struct hw_resource *hw_res, bool on)
{
	int ret = 0;

	if (gpio_is_valid(hw_res->reset_gpio)) {
		TPD_DETAIL("Set the reset_gpio on=%d \n", on);
		ret = gpio_direction_output(hw_res->reset_gpio, on);

		if (ret) {
			TPD_INFO("Set the reset_gpio on=%d fail\n", on);
		}

		msleep(RESET_TO_NORMAL_TIME);
		TPD_DETAIL("%s hw_res->reset_gpio = %d\n", __func__, hw_res->reset_gpio);
	}

	return ret;
}

void himax_esd_hw_reset(struct chip_data_hx83112a_nf *chip_info)
{
	int ret = 0;
	int load_fw_times = 10;

	TPD_DETAIL("START_Himax TP: ESD - Reset\n");
	chip_info->esd_reset_activate = 1;

	hx83112a_nf_enable_interrupt(chip_info, false);

	do {
		hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/
		hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
		hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/

		TPD_DETAIL("%s: ESD reset finished\n", __func__);

		TPD_DETAIL("It will update fw after esd event in zero flash mode!\n");

		load_fw_times--;
		g_core_fp.fp_0f_operation_dirly(chip_info);
		ret = g_core_fp.fp_reload_disable(chip_info,
						  0);/*success return 1, fail return 0*/
	} while (!ret && load_fw_times > 0);

	if (!load_fw_times) {
		TPD_INFO("%s: load_fw_times over 10 times\n", __func__);
	}

	himax_sense_on(chip_info, 0x00);
	/* need_modify*/
	/* report all leave event
	himax_report_all_leave_event(private_ts);*/

	hx83112a_nf_enable_interrupt(chip_info, true);
}


int himax_checksum_cal(struct chip_data_hx83112a_nf *chip_info, uint8_t *buf,
		       int ts_status)
{
	/*#if defined(HX_ESD_RECOVERY)*/
	int hx_eb_event = 0;
	int hx_ec_event = 0;
	int hx_ed_event = 0;
	int hx_esd_event = 0;
	int hx_zero_event = 0;
	int shaking_ret = 0;
	/*#endif*/
	uint16_t check_sum_cal = 0;
	int32_t loop_i = 0;
	int length = 0;

	/* Normal */
	if (ts_status == HX_REPORT_COORD) {
		length = chip_info->hx_touch_data->touch_info_size;
	}

	/* SMWP */
	else if (ts_status == HX_REPORT_SMWP_EVENT) {
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);

	} else {
		TPD_INFO("%s, Neither Normal Nor SMWP error!\n", __func__);
	}

	/*TPD_INFO("Now status=%d,length=%d\n",ts_status,length);*/
	for (loop_i = 0; loop_i < length; loop_i++) {
		check_sum_cal += buf[loop_i];

		/* #ifdef HX_ESD_RECOVERY  */
		if (ts_status == HX_REPORT_COORD) {
			/* case 1 ESD recovery flow */
			if (buf[loop_i] == 0xEB) {
				hx_eb_event++;

			} else if (buf[loop_i] == 0xEC) {
				hx_ec_event++;

			} else if (buf[loop_i] == 0xED) {
				hx_ed_event++;

			} else if (buf[loop_i] == 0x00) { /* case 2 ESD recovery flow-Disable */
				hx_zero_event++;

			} else {
				hx_eb_event = 0;
				hx_ec_event = 0;
				hx_ed_event = 0;
				hx_zero_event = 0;
				g_zero_event_count = 0;
			}

			if (hx_eb_event == length) {
				hx_esd_event = length;
				TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xEB.\n");

			} else if (hx_ec_event == length) {
				hx_esd_event = length;
				TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xEC.\n");

			} else if (hx_ed_event == length) {
				hx_esd_event = length;
				TPD_INFO("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");

			} else {
				hx_esd_event = 0;
			}
		}

		/* #endif */
	}

	if (ts_status == HX_REPORT_COORD) {
		/*#ifdef HX_ESD_RECOVERY*/
		if (hx_esd_event == length || hx_zero_event == length) {
			shaking_ret = himax_ic_esd_recovery(hx_esd_event, hx_zero_event, length);

			if (shaking_ret == CHECKSUM_FAIL) {
				himax_esd_hw_reset(chip_info);
				goto checksum_fail;

			} else if (shaking_ret == ERR_WORK_OUT) {
				goto err_workqueue_out;

			} else {
				/*TPD_INFO("I2C running. Nothing to be done!\n");*/
				goto workqueue_out;
			}

		} else if (chip_info->esd_reset_activate) {
			/* drop 1st interrupts after chip reset */
			chip_info->esd_reset_activate = 0;
			TPD_INFO("[HX_ESD_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n",
				 __func__);
			goto checksum_fail;

		} else if (chip_info->hw_reset_active) {
			/* drop 1st interrupts after chip reset */
			chip_info->hw_reset_active = 0;
			TPD_INFO("[HX_HW_RESET_ACTIVATE]:%s: Back from reset, ready to serve.\n",
				 __func__);
			goto ready_to_serve;
		}
	}

	/*#endif*/
	if ((check_sum_cal % 0x100 != 0)) {
		TPD_INFO("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n",
			 check_sum_cal);
		/*goto checksum_fail;*/
		goto workqueue_out;
	}

	/* TPD_INFO("%s:End\n",__func__); */
	return NO_ERR;

ready_to_serve:
	return READY_TO_SERVE;
checksum_fail:
	return CHECKSUM_FAIL;
	/*#ifdef HX_ESD_RECOVERY*/
err_workqueue_out:
	return ERR_WORK_OUT;
workqueue_out:
	return WORK_OUT;
	/*#endif*/
}

void himax_log_touch_data(uint8_t *buf, struct himax_report_data *hx_touch_data)
{
	int loop_i = 0;
	int print_size = 0;

	if (!hx_touch_data->diag_cmd) {
		print_size = hx_touch_data->touch_info_size;

	} else {
		print_size = hx_touch_data->touch_all_size;
	}

	for (loop_i = 0; loop_i < print_size; loop_i++) {
		printk("0x%02X ",  buf[loop_i]);

		if ((loop_i + 1) % 8 == 0) {
			printk("\n");
		}

		if (loop_i == (print_size - 1)) {
			printk("\n");
		}
	}
}



void himax_reload_disable(struct chip_data_hx83112a_nf *chip_info, int on)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	TPD_INFO("%s:entering\n", __func__);

	if (on) { /*reload disable*/
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;

	} else { /*reload enable*/
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
	}

	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	TPD_INFO("%s: setting OK!\n", __func__);
}

int hx_test_data_get(struct chip_data_hx83112a_nf *chip_info, uint32_t RAW[],
		     char *start_log, char *result, int now_item, struct auto_testdata *hx_testdata)
{
	uint32_t i;

	ssize_t len = 0;
	char *testdata = NULL;
	uint32_t SZ_SIZE = 5 * chip_info->hw_res->tx_num * chip_info->hw_res->rx_num *
			   2;

	TPD_INFO("%s: Entering, Now type=%s!\n", __func__,
		 g_himax_inspection_mode[now_item]);

	testdata = tp_devm_kzalloc(&chip_info->hx_spi->dev, sizeof(char) * SZ_SIZE,
				   GFP_KERNEL);

	if (!testdata) {
		TPD_INFO("%s:%d testdata kzalloc buf error\n", __func__, __LINE__);
		return -ENOMEM;
	}

	len += snprintf((testdata + len), SZ_SIZE - len, "%s", start_log);

	for (i = 0; i < chip_info->hw_res->tx_num * chip_info->hw_res->rx_num; i++) {
		if (i > 1 && ((i + 1) % chip_info->hw_res->rx_num) == 0) {
			len += snprintf((testdata + len), SZ_SIZE - len, "%5d,\n", RAW[i]);
		} else
			len += snprintf((testdata + len), SZ_SIZE - len,
					"%5d,", RAW[i]);
	}

	len += snprintf((testdata + len), SZ_SIZE - len, "\n%s", result);

	tp_test_write(hx_testdata->fp, hx_testdata->length, testdata, len,
		      hx_testdata->pos);

	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)testdata,
		      sizeof(char) * SZ_SIZE);
	TPD_INFO("%s: End!\n", __func__);
	return NO_ERR;
}


int himax_get_rawdata(struct chip_data_hx83112a_nf *chip_info, uint32_t *RAW,
		      uint32_t datalen)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t *tmp_rawdata;
	uint8_t retry = 0;
	uint16_t checksum_cal;
	uint32_t i = 0;

	uint8_t max_i2c_size = MAX_RECVS_SZ;
	int address = 0;
	int total_read_times = 0;
	int total_size = datalen * 2 + 4;
	int total_size_temp;
#if 1/*def RAWDATA_DEBUG_PF*/
	uint32_t j = 0;
	uint32_t index = 0;
	uint32_t Min_DATA = 0xFFFFFFFF;
	uint32_t Max_DATA = 0x00000000;
#endif

	/*1 Set Data Ready PWD*/
	while (retry < 200) {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = Data_PWD1;
		tmp_data[0] = Data_PWD0;
		himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, 4);

		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

		if ((tmp_data[0] == Data_PWD0 && tmp_data[1] == Data_PWD1) ||
				(tmp_data[0] == Data_PWD1 && tmp_data[1] == Data_PWD0)) {
			break;
		}

		retry++;
		msleep(1);
	}

	if (retry >= 200) {
		return RESULT_ERR;

	} else {
		retry = 0;
	}

	while (retry < 200) {
		if (tmp_data[0] == Data_PWD1 && tmp_data[1] == Data_PWD0) {
			break;
		}

		retry++;
		msleep(1);
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	}

	if (retry >= 200) {
		return RESULT_ERR;

	} else {
		retry = 0;
	}

	tmp_rawdata = tp_devm_kzalloc(&chip_info->hx_spi->dev,
				      sizeof(uint8_t) * (total_size + 8), GFP_KERNEL);

	if (!tmp_rawdata) {
		return RESULT_ERR;
	}

	/*2 Read Data from SRAM*/
	while (retry < 10) {
		checksum_cal = 0;
		total_size_temp = total_size;
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;

		if (total_size % max_i2c_size == 0) {
			total_read_times = total_size / max_i2c_size;

		} else {
			total_read_times = total_size / max_i2c_size + 1;
		}

		for (i = 0; i < (total_read_times); i++) {
			if (total_size_temp >= max_i2c_size) {
				himax_register_read(chip_info, tmp_addr, max_i2c_size,
						    &tmp_rawdata[i * max_i2c_size], false);
				total_size_temp = total_size_temp - max_i2c_size;

			} else {
				/*TPD_INFO("last total_size_temp=%d\n",total_size_temp);*/
				himax_register_read(chip_info, tmp_addr, total_size_temp % max_i2c_size,
						    &tmp_rawdata[i * max_i2c_size], false);
			}

			address = ((i + 1) * max_i2c_size);
			tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
			tmp_addr[0] = (uint8_t)((address) & 0x00FF);
		}

		/**/
		/*3 Check Checksum*/
		for (i = 2; i < total_size; i = i + 2) {
			checksum_cal += tmp_rawdata[i + 1] * 256 + tmp_rawdata[i];
		}

		if (checksum_cal == 0) {
			break;
		}

		retry++;
	}

	if (retry >= 10) {
		TPD_INFO("Retry over 10 times: do recovery\n");
		himax_esd_hw_reset(chip_info);
		return RESULT_RETRY;
	}

	/*4 Copy Data*/
	for (i = 0; i < chip_info->hw_res->rx_num * chip_info->hw_res->tx_num; i++) {
		RAW[i] = tmp_rawdata[(i * 2) + 1 + 4] * 256 + tmp_rawdata[(i * 2) + 4];
	}


#if 1/*def RAWDATA_DEBUG_PF*/

	for (j = 0; j < chip_info->hw_res->tx_num; j++) {
		if (j == 0) {
			printk("      RX%2d", j + 1);

		} else {
			printk("  RX%2d", j + 1);
		}
	}

	printk("\n");

	for (i = 0; i < chip_info->hw_res->rx_num; i++) {
		printk("TX%2d", i + 1);

		for (j = 0; j < chip_info->hw_res->tx_num; j++) {
			/*if ((j == SKIPRXNUM) && (i >= SKIPTXNUM_START) && (i <= SKIPTXNUM_END)) {*/
			/* continue;*/
			/*} else {*/
			index = ((chip_info->hw_res->rx_num * chip_info->hw_res->tx_num - i) -
				 chip_info->hw_res->rx_num * j) - 1;

			printk("%6d", RAW[index]);

			if (RAW[index] > Max_DATA) {
				Max_DATA = RAW[index];
			}

			if (RAW[index] < Min_DATA) {
				Min_DATA = RAW[index];
			}

			/*}*/
		}

		/*index++;*/
		printk("\n");
	}

	TPD_INFO("Max = %5d, Min = %5d \n", Max_DATA, Min_DATA);
#endif

	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)tmp_rawdata,
		      sizeof(uint8_t) * (total_size + 8));

	return RESULT_OK;
}

void himax_switch_data_type(struct chip_data_hx83112a_nf *chip_info,
			    uint8_t checktype)
{
	uint8_t datatype = 0x00;

	switch (checktype) {
	case HIMAX_INSPECTION_OPEN:
		datatype = DATA_OPEN;
		break;

	case HIMAX_INSPECTION_MICRO_OPEN:
		datatype = DATA_MICRO_OPEN;
		break;

	case HIMAX_INSPECTION_SHORT:
		datatype = DATA_SHORT;
		break;

	case HIMAX_INSPECTION_RAWDATA:
		datatype = DATA_RAWDATA;
		break;

	case HIMAX_INSPECTION_NOISE:
		datatype = DATA_NOISE;
		break;

	case HIMAX_INSPECTION_BACK_NORMAL:
		datatype = DATA_BACK_NORMAL;
		break;

	case HIMAX_INSPECTION_LPWUG_RAWDATA:
		datatype = DATA_LPWUG_RAWDATA;
		break;

	case HIMAX_INSPECTION_LPWUG_NOISE:
		datatype = DATA_LPWUG_NOISE;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
		datatype = DATA_LPWUG_IDLE_RAWDATA;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		datatype = DATA_LPWUG_IDLE_NOISE;
		break;

	default:
		TPD_INFO("Wrong type=%d\n", checktype);
		break;
	}

	himax_diag_register_set(chip_info, datatype);
}

int himax_switch_mode(struct chip_data_hx83112a_nf *chip_info, int mode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	TPD_INFO("%s: Entering\n", __func__);

	/*Stop Handshaking*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x00;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, 4);

	/*Swtich Mode*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x7F;
	tmp_addr[0] = 0x04;

	switch (mode) {
	case HIMAX_INSPECTION_SORTING:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SORTING_START;
		tmp_data[0] = PWD_SORTING_START;
		break;

	case HIMAX_INSPECTION_OPEN:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;

	case HIMAX_INSPECTION_MICRO_OPEN:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;

	case HIMAX_INSPECTION_SHORT:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SHORT_START;
		tmp_data[0] = PWD_SHORT_START;
		break;

	case HIMAX_INSPECTION_RAWDATA:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_RAWDATA_START;
		tmp_data[0] = PWD_RAWDATA_START;
		break;

	case HIMAX_INSPECTION_NOISE:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_NOISE_START;
		tmp_data[0] = PWD_NOISE_START;
		break;

	case HIMAX_INSPECTION_LPWUG_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_NOISE:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LPWUG_START;
		tmp_data[0] = PWD_LPWUG_START;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LPWUG_IDLE_START;
		tmp_data[0] = PWD_LPWUG_IDLE_START;
		break;
	}

	himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, 4);

	TPD_INFO("%s: End of setting!\n", __func__);

	return 0;
}


void himax_set_N_frame(struct chip_data_hx83112a_nf *chip_info, uint16_t Nframe,
		       uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x72;
	tmp_addr[0] = 0x94;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = (uint8_t)((Nframe & 0xFF00) >> 8);
	tmp_data[0] = (uint8_t)(Nframe & 0x00FF);
	himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, 4);

	/*SKIP FRMAE*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x70;
	tmp_addr[0] = 0xF4;
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

	switch (checktype) {
	case HIMAX_INSPECTION_LPWUG_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_NOISE:
		tmp_data[0] = BS_LPWUG;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		tmp_data[0] = BS_LPWUG_IDLE;
		break;

	case HIMAX_INSPECTION_RAWDATA:
	case HIMAX_INSPECTION_NOISE:
		tmp_data[0] = BS_RAWDATANOISE;
		break;

	default:
		tmp_data[0] = BS_OPENSHORT;
		break;
	}

	himax_flash_write_burst_length(chip_info, tmp_addr, tmp_data, 4);
}


uint32_t himax_check_mode(struct chip_data_hx83112a_nf *chip_info,
			  uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t wait_pwd[2];
	/* uint8_t count = 0;*/

	wait_pwd[0] = PWD_NONE;
	wait_pwd[1] = PWD_NONE;

	switch (checktype) {
	case HIMAX_INSPECTION_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;

	case HIMAX_INSPECTION_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;

	case HIMAX_INSPECTION_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;

	case HIMAX_INSPECTION_RAWDATA:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;

	case HIMAX_INSPECTION_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;

	case HIMAX_INSPECTION_LPWUG_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_NOISE:
		wait_pwd[0] = PWD_LPWUG_END;
		wait_pwd[1] = PWD_LPWUG_END;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		wait_pwd[0] = PWD_LPWUG_IDLE_END;
		wait_pwd[1] = PWD_LPWUG_IDLE_END;
		break;

	default:
		TPD_INFO("Wrong type=%d\n", checktype);
		break;
	}

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x7F;
	tmp_addr[0] = 0x04;
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	TPD_INFO("%s: himax_wait_sorting_mode, tmp_data[0]=%x, tmp_data[1]=%x\n",
		 __func__, tmp_data[0], tmp_data[1]);

	if (wait_pwd[0] == tmp_data[0] && wait_pwd[1] == tmp_data[1]) {
		TPD_INFO("Change to mode=%s\n", g_himax_inspection_mode[checktype]);
		return 0;

	} else {
		return 1;
	}
}

void himax_get_noise_base(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t ratio, threshold, threshold_LPWUG;

	/*tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x70;
	tmp_addr[0] = 0x8C;
	*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x70;
	tmp_addr[0] = 0x94; /*ratio*/
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	ratio = tmp_data[1];

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x70;
	tmp_addr[0] = 0xA0; /*threshold_LPWUG*/
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	threshold_LPWUG = tmp_data[0];

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x70;
	tmp_addr[0] = 0x9C; /*threshold*/
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	threshold = tmp_data[0];

	/*TPD_INFO("tmp_data[0]=0x%x tmp_data[1]=0x%x tmp_data[2]=0x%x tmp_data[3]=0x%x\n",
	          tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
	*/
	/*NOISEMAX = tmp_data[3]*(NOISE_P/256);*/
	chip_info->NOISEMAX = ratio * threshold;
	chip_info->LPWUG_NOISEMAX = ratio * threshold_LPWUG;
	TPD_INFO("NOISEMAX=%d LPWUG_NOISE_MAX=%d \n", chip_info->NOISEMAX,
		 chip_info->LPWUG_NOISEMAX);
}

uint16_t himax_get_noise_weight(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint16_t weight;

	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x72;
	tmp_addr[0] = 0xC8;
	himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
	weight = (tmp_data[1] << 8) | tmp_data[0];
	TPD_INFO("%s: weight = %d ", __func__, weight);

	return weight;
}

uint32_t himax_wait_sorting_mode(struct chip_data_hx83112a_nf *chip_info,
				 uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t wait_pwd[2];
	uint8_t count = 0;

	wait_pwd[0] = PWD_NONE;
	wait_pwd[1] = PWD_NONE;

	switch (checktype) {
	case HIMAX_INSPECTION_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;

	case HIMAX_INSPECTION_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;

	case HIMAX_INSPECTION_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;

	case HIMAX_INSPECTION_RAWDATA:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;

	case HIMAX_INSPECTION_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;

	case HIMAX_INSPECTION_LPWUG_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_NOISE:
		wait_pwd[0] = PWD_LPWUG_END;
		wait_pwd[1] = PWD_LPWUG_END;
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		wait_pwd[0] = PWD_LPWUG_IDLE_END;
		wait_pwd[1] = PWD_LPWUG_IDLE_END;
		break;

	default:
		TPD_INFO("Wrong type=%d\n", checktype);
		break;
	}

	do {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x04;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: himax_wait_sorting_mode, tmp_data[0]=%x, tmp_data[1]=%x\n",
			 __func__, tmp_data[0], tmp_data[1]);

		if (wait_pwd[0] == tmp_data[0] && wait_pwd[1] == tmp_data[1]) {
			return 0;
		}

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 0x900000A8, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xE4;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 0x900000E4, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xF8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 0x900000F8, tmp_data[0]=%x, tmp_data[1]=%x, tmp_data[2]=%x, tmp_data[3]=%x \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
		TPD_INFO("Now retry %d times!\n", count++);
		msleep(50);
	} while (count < 50);

	return 1;
}

int himax_check_notch(struct chip_data_hx83112a_nf *chip_info, int index)
{
	int tx = chip_info->hw_res->tx_num;
	int rx = chip_info->hw_res->rx_num;

	if ((((tx - 4) / 2 <= index / 36) && (index / 36 <= (tx - 4) / 2 + 3))
			&& (index % 36 >= rx - 1)) {
		return 1;

	} else {
		return 0;
	}
}

int mpTestFunc(struct chip_data_hx83112a_nf *chip_info, uint8_t checktype,
	       uint32_t datalen, struct auto_testdata *hx_testdata)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};

	uint32_t i = 0;
	uint16_t weight = 0;
	uint32_t RAW[datalen];
#ifdef RAWDATA_NOISE
	uint32_t RAW_Rawdata[datalen];
#endif
	uint32_t *max_limits;
	uint32_t *min_limits;
	char *rslt_log = NULL;
	char *start_log = NULL;
	int ret = 0;
	struct test_item_info *p_test_item_info = NULL;

	if (himax_check_mode(chip_info, checktype)) {
		TPD_INFO("Need Change Mode, target=%s", g_himax_inspection_mode[checktype]);

		himax_sense_off_mptest(chip_info);

#ifndef HX_ZERO_FLASH
		himax_reload_disable(chip_info, 1);
#endif

		himax_switch_mode(chip_info, checktype);

		if (checktype == HIMAX_INSPECTION_NOISE) {
			himax_set_N_frame(chip_info, NOISEFRAME, checktype);
			/*himax_get_noise_base(chip_info);*/

		} else if (checktype >= HIMAX_INSPECTION_LPWUG_RAWDATA) {
			TPD_INFO("N frame = %d\n", 1);
			himax_set_N_frame(chip_info, 1, checktype);

		} else {
			himax_set_N_frame(chip_info, 2, checktype);
		}


		himax_sense_on(chip_info, 1);

		ret = himax_wait_sorting_mode(chip_info, checktype);

		if (ret) {
			TPD_INFO("%s: himax_wait_sorting_mode FAIL\n", __func__);
			return ret;
		}
	}

	himax_switch_data_type(chip_info, checktype);

	ret = himax_get_rawdata(chip_info, RAW, datalen);

	if (ret) {
		TPD_INFO("%s: himax_get_rawdata FAIL\n", __func__);

		tmp_addr[3] = 0x90;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0xA8;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 900000A8: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X, \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x40;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 10007F40: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X, \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 10000000: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X, \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x04;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 10007F04: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X, \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x02;
		tmp_addr[1] = 0x04;
		tmp_addr[0] = 0xB4;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: 800204B4: data[0]=%0x02X, data[1]=%0x02X, data[2]=%0x02X, data[3]=%0x02X, \n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		/*900000A8,10007F40,10000000,10007F04,800204B4*/
		return ret;
	}

	/* back to normal */
	himax_switch_data_type(chip_info, HIMAX_INSPECTION_BACK_NORMAL);

	rslt_log = tp_devm_kzalloc(&chip_info->hx_spi->dev, 256 * sizeof(char),
				   GFP_KERNEL);

	if (!rslt_log) {
		TPD_INFO("%s:%d rslt_log kzalloc buf error\n", __func__, __LINE__);
		goto RET_OUT;
	}

	start_log = tp_devm_kzalloc(&chip_info->hx_spi->dev, 256 * sizeof(char),
				    GFP_KERNEL);

	if (!start_log) {
		TPD_INFO("%s:%d  start_log kzalloc buf error\n", __func__, __LINE__);
		goto RET_OUT;
	}

	p_test_item_info = get_test_item_info(hx_testdata->fw, checktype);

	if (!p_test_item_info) {
		TPD_INFO("item: %s get_test_item_info fail\n",
			 g_himax_inspection_mode[checktype]);
		goto RET_OUT;
	}

	max_limits = (uint32_t *)(hx_testdata->fw->data +
				  p_test_item_info->top_limit_offset);
	min_limits = (uint32_t *)(hx_testdata->fw->data +
				  p_test_item_info->floor_limit_offset);
	snprintf(start_log, 256 * sizeof(char), "\n\n%s%s\n",
		 g_himax_inspection_mode[checktype], ": data as follow!\n");

	/*Check Data*/
	switch (checktype) {
	case HIMAX_INSPECTION_OPEN:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: open test FAIL\n", __func__);
				ret = RESULT_ERR;
			}
		}

		TPD_INFO("%s: open test PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_MICRO_OPEN:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: micro open test FAIL\n", __func__);
				ret =  RESULT_ERR;
			}
		}

		TPD_INFO("M_OPENMAX = %d, M_OPENMIN = %d\n",
			 max_limits[1],
			 min_limits[1]);
		TPD_INFO("%s: micro open test PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_SHORT:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: short test FAIL\n", __func__);
				ret = RESULT_ERR;
			}
		}

		TPD_INFO("%s: short test PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_RAWDATA:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: rawdata test FAIL\n", __func__);
				ret = RESULT_ERR;
			}
		}

		TPD_INFO("%s: rawdata test PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_NOISE:
		/*for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
		    if (himax_check_notch(chip_info, i)) {
		        continue;
		    }
		    if (RAW[i] > chip_info->NOISEMAX) {
		        TPD_INFO("%s: noise test FAIL\n", __func__);
		        return RESULT_ERR;
		    }
		}*/
		himax_get_noise_base(chip_info);
		snprintf(start_log, 256 * sizeof(char), "\n Threshold = %d\n",
			 chip_info->NOISEMAX);
		weight = himax_get_noise_weight(chip_info);

		if (weight > chip_info->NOISEMAX) {
			TPD_INFO("%s: noise test FAIL\n", __func__);
			ret = RESULT_ERR;
		}

		TPD_INFO("%s: noise test PASS\n", __func__);

#ifdef RAWDATA_NOISE
		TPD_INFO("[MP_RAW_TEST_RAW]\n");

		himax_switch_data_type(chip_info, HIMAX_INSPECTION_RAWDATA);
		ret = himax_get_rawdata(chip_info, RAW, datalen);

		if (ret == RESULT_ERR) {
			TPD_INFO("%s: himax_get_rawdata FAIL\n", __func__);
			ret = RESULT_ERR;
		}

#endif
		break;

	case HIMAX_INSPECTION_LPWUG_RAWDATA:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_RAWDATA FAIL\n", __func__);
				ret = THP_AFE_INSPECT_ERAW;
			}
		}

		TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_RAWDATA PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_LPWUG_NOISE:
		himax_get_noise_base(chip_info);
		weight = himax_get_noise_weight(chip_info);

		if (weight > chip_info->LPWUG_NOISEMAX || weight < min_limits[i]) {
			TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_NOISE FAIL\n", __func__);
			ret = THP_AFE_INSPECT_ENOISE;
		}

		TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_NOISE PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA FAIL\n", __func__);
				ret = THP_AFE_INSPECT_ERAW;
			}
		}

		TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA PASS\n", __func__);
		break;

	case HIMAX_INSPECTION_LPWUG_IDLE_NOISE:
		for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
			if (himax_check_notch(chip_info, i)) {
				continue;
			}

			if (RAW[i] > max_limits[i] || RAW[i] < min_limits[i]) {
				TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_NOISE FAIL\n", __func__);
				ret = THP_AFE_INSPECT_ENOISE;
			}
		}

		TPD_INFO("%s: HIMAX_INSPECTION_LPWUG_IDLE_NOISE PASS\n", __func__);
		break;

	default:
		TPD_INFO("Wrong type=%d\n", checktype);
		break;
	}

	snprintf(rslt_log, 256 * sizeof(char), "%s%s\n",
		 g_himax_inspection_mode[checktype], ret ? " Test Failed!" : " Test Pass!");

	hx_test_data_get(chip_info, RAW, start_log, rslt_log, checktype, hx_testdata);

	if (rslt_log) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)rslt_log, 256 * sizeof(char));
	}

	if (start_log) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)start_log, 256 * sizeof(char));
	}

	if (ret) {
		return ret;

	} else {
		return RESULT_OK;
	}

RET_OUT:

	if (p_test_item_info) {
		kfree(p_test_item_info);
	}

	if (rslt_log) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)rslt_log, 256 * sizeof(char));
	}

	if (start_log) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)start_log, 256 * sizeof(char));
	}

	return RESULT_ERR;
}

/* Get sub-string from original string by using some charaters
 * return size of result
 */

int hx_get_size_str_arr(char **input)
{
	int i = 0;
	int result = 0;

	while (input[i] != NULL) {
		i++;
	}

	result = i;
	TPD_DEBUG("There is %d in [0]=%s\n", result, input[0]);

	return result;
}

int himax_chip_self_test(struct seq_file *s, void *chip_data,
			 struct auto_testdata *hx_testdata)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	int error = 0;
	int error_num = 0;
	char buf[128] = {0};
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t back_data[4];
	uint8_t retry_cnt = 3;

	TPD_INFO("%s:Entering\n", __func__);

	/*1. Open Test*/
	TPD_INFO("[MP_OPEN_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_OPEN,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 15, "test Item:\n");
	snprintf(buf, 128, "1. Open Test: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 32, "1. Open Test: %s\n", error ? "NG" : "Ok");
	TPD_INFO("%s", buf);
	seq_printf(s, buf);

	if (error != 0) {
		error_num++;
	}

	/*2. Micro-Open Test*/
	retry_cnt = 3;
	TPD_INFO("[MP_MICRO_OPEN_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_MICRO_OPEN,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "2. Micro Open Test: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 32, "2. Micro Open Test: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);
	seq_printf(s, buf);

	if (error != 0) {
		error_num++;
	}

	/*3. Short Test*/
	retry_cnt = 3;
	TPD_INFO("[MP_SHORT_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_SHORT,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "3. Short Test: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 32, "3. Short Test: %s\n", error ? "NG" : "Ok");
	TPD_INFO("%s", buf);
	seq_printf(s, buf);

	if (error != 0) {
		error_num++;
	}

#ifndef RAWDATA_NOISE
	/*4. RawData Test*/
	retry_cnt = 3;
	TPD_INFO("[MP_RAW_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_RAWDATA,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "4. Raw data Test: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 32, "4. Raw data Test: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);
	seq_printf(s, buf);

	if (error != 0) {
		error_num++;
	}

#endif

	/*5. Noise Test*/
	retry_cnt = 3;
	TPD_INFO("[MP_NOISE_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_NOISE,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "5. Noise Test: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 32, "5. Noise Test: %s\n", error ? "NG" : "Ok");
	TPD_INFO("%s", buf);
	seq_printf(s, buf);

	if (error != 0) {
		error_num++;
	}

	/*himax_set_SMWP_enable(ts->SMWP_enable,suspended);*/
	/*Enable:0x10007F10 = 0xA55AA55A*/
	retry_cnt = 0;

	do {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x10;
		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
		back_data[3] = 0XA5;
		back_data[2] = 0X5A;
		back_data[1] = 0XA5;
		back_data[0] = 0X5A;
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: tmp_data[0] = 0x%22X, retry_cnt=%d \n", __func__, tmp_data[0],
			 retry_cnt);
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
			|| tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
			&& retry_cnt < HIMAX_REG_RETRY_TIMES);

	TPD_INFO("%s:End", __func__);
	himax_sense_off_mptest(chip_info);
	/*himax_switch_mode(HIMAX_INSPECTION_LPWUG_RAWDATA);*/
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 22, "Final_result: %s\n",
					 error_num ? "Fail" : "Pass");
	return error_num;
}

/*power saving level*/
void himax_init_psl(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*==============================================================*/
	/* SCU_Power_State_PW : 0x9000_00A0 ==> 0x0000_0000 (Reset PSL)*/
	/*==============================================================*/
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0xA0;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

	TPD_INFO("%s: power saving level reset OK!\n", __func__);
}


void himax_chip_erase(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_interface_on(chip_info);

	/* init psl */
	himax_init_psl(chip_info);

	/*=====================================*/
	/* SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780*/
	/*=====================================*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x02;
	tmp_data[1] = 0x07;
	tmp_data[0] = 0x80;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	/*=====================================*/
	/* Chip Erase*/
	/* Write Enable : 1. 0x8000_0020 ==> 0x4700_0000*/
	/*                2. 0x8000_0024 ==> 0x0000_0006*/
	/*=====================================*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x20;
	tmp_data[3] = 0x47;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x06;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	/*=====================================*/
	/* Chip Erase*/
	/* Erase Command : 0x8000_0024 ==> 0x0000_00C7*/
	/*=====================================*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x24;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0xC7;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	msleep(2000);

	if (!wait_wip(chip_info, 100)) {
		TPD_INFO("%s:83112_Chip_Erase Fail\n", __func__);
	}
}

void himax_flash_programming(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t *FW_content, int FW_Size)
{
	int page_prog_start = 0;
	int program_length = 48;
	int i = 0, j = 0, k = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t buring_data[256];    /* Read for flash data, 128K*/
	/* 4 bytes for 0x80002C padding*/

	himax_interface_on(chip_info);

	/*=====================================*/
	/* SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780*/
	/*=====================================*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x02;
	tmp_data[1] = 0x07;
	tmp_data[0] = 0x80;
	himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	for (page_prog_start = 0; page_prog_start < FW_Size;
			page_prog_start = page_prog_start + 256) {
		/*msleep(5);*/
		/*=====================================*/
		/* Write Enable : 1. 0x8000_0020 ==> 0x4700_0000*/
		/*                2. 0x8000_0024 ==> 0x0000_0006*/
		/*=====================================*/
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x20;
		tmp_data[3] = 0x47;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x06;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		/*=================================*/
		/* SPI Transfer Control*/
		/* Set 256 bytes page write : 0x8000_0020 ==> 0x610F_F000*/
		/* Set read start address   : 0x8000_0028 ==> 0x0000_0000*/
		/*=================================*/
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x20;
		tmp_data[3] = 0x61;
		tmp_data[2] = 0x0F;
		tmp_data[1] = 0xF0;
		tmp_data[0] = 0x00;
		/* data bytes should be 0x6100_0000 + ((word_number)*4-1)*4096 = 0x6100_0000 + 0xFF000 = 0x610F_F000*/
		/* Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64*/
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x28;
		/*tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;  Flash start address 1st : 0x0000_0000*/

		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;

		} else if (page_prog_start >= 0x100 && page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;

		} else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}

		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);


		/*=================================*/
		/* Send 16 bytes data : 0x8000_002C ==> 16 bytes data*/
		/*=================================*/
		buring_data[0] = 0x2C;
		buring_data[1] = 0x00;
		buring_data[2] = 0x00;
		buring_data[3] = 0x80;

		for (i = /*0*/page_prog_start, j = 0; i < 16 + page_prog_start/**/; i++, j++) {
			buring_data[j + 4] = FW_content[i];
		}


		if (himax_bus_write(chip_info, 0x00, 20, buring_data) < 0) {
			TPD_INFO("%s: i2c access fail!\n", __func__);
			return;
		}

		/*=================================*/
		/* Write command : 0x8000_0024 ==> 0x0000_0002*/
		/*=================================*/
		tmp_addr[3] = 0x80;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x24;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x02;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		/*=================================*/
		/* Send 240 bytes data : 0x8000_002C ==> 240 bytes data*/
		/*=================================*/

		for (j = 0; j < 5; j++) {
			for (i = (page_prog_start + 16 + (j * 48)), k = 0;
					i < (page_prog_start + 16 + (j * 48)) + program_length; i++, k++) {
				buring_data[k + 4] = FW_content[i]; /*(byte)i;*/
			}

			if (himax_bus_write(chip_info, 0x00, program_length + 4, buring_data) < 0) {
				TPD_INFO("%s: i2c access fail!\n", __func__);
				return;
			}
		}

		if (!wait_wip(chip_info, 1)) {
			TPD_INFO("%s:83112_Flash_Programming Fail\n", __func__);
		}
	}
}

int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct chip_data_hx83112a_nf *chip_info,
					unsigned char *fw, int len, bool change_iref) /* Alice - Un */
{
	/*int CRC_from_FW = 0;*/
	int burnfw_success = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	if (len != FW_SIZE_64K) {
		TPD_INFO("%s: The file size is not 64K bytes\n", __func__);
		return false;
	}

	/*#ifdef HX_RST_PIN_FUNC
	    himax_ic_reset(chip_info, false,false);
	#else*/
	/*===AHBI2C_SystemReset==========*/
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x18;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x55;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);
	/*#endif*/

	himax_enter_safe_mode(chip_info);
	himax_chip_erase(chip_info);
	himax_flash_programming(chip_info, fw, FW_SIZE_64K);

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;

	if (himax_hw_check_CRC(chip_info, tmp_data, FW_SIZE_64K) == 0) {
		burnfw_success = 1;

	} else {
		burnfw_success = 0;
	}

	/*RawOut select initial*/
	tmp_addr[3] = 0x80;
	tmp_addr[2] = 0x02;
	tmp_addr[1] = 0x04;
	tmp_addr[0] = 0xB4;

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

	/*DSRAM func initial*/
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x07;
	tmp_addr[0] = 0xFC;

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(chip_info, false, false);
#else
	/*===AHBI2C_SystemReset==========*/
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x18;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x55;
	himax_register_write(chip_info, tmp_addr, 4, tmp_data, false);
#endif
	return burnfw_success;
}

static size_t hx83112a_nf_proc_register_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	size_t ret = 0;
	uint16_t loop_i;
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	int max_bus_size = MAX_RECVS_SZ;
	uint8_t data[MAX_RECVS_SZ];
#else
	int max_bus_size = 128;
	uint8_t data[128];
#endif
	char *temp_buf;
	/*struct touchpanel_data *ts = PDE_DATA(file_inode(file));*/
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	memset(data, 0x00, sizeof(data));

	if (!chip_info->proc_send_flag) {
		temp_buf = tp_devm_kzalloc(&chip_info->hx_spi->dev, len, GFP_KERNEL);

		TPD_INFO("himax_register_show: %02X, %02X, %02X, %02X\n",
			 chip_info->register_command[3], chip_info->register_command[2],
			 chip_info->register_command[1], chip_info->register_command[0]);

		himax_register_read(chip_info, chip_info->register_command, max_bus_size, data,
				    chip_info->cfg_flag);

		ret += snprintf(temp_buf + ret, len - ret, "command:  %02X, %02X, %02X, %02X\n",
				chip_info->register_command[3], chip_info->register_command[2],
				chip_info->register_command[1], chip_info->register_command[0]);

		for (loop_i = 0; loop_i < max_bus_size; loop_i++) {
			ret += snprintf(temp_buf + ret, len - ret, "0x%2.2X ", data[loop_i]);

			if ((loop_i % 16) == 15) {
				ret += snprintf(temp_buf + ret, len - ret, "\n");
			}
		}

		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len)) {
			TPD_INFO("%s, here:%d\n", __func__, __LINE__);
		}

		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_buf, len);
		chip_info->proc_send_flag = 1;

	} else {
		chip_info->proc_send_flag = 0;
	}

	return ret;
}

static size_t hx83112a_nf_proc_register_write(struct file *file,
		const char *buff, size_t len, loff_t *pos)
{
	char buf[81] = {0};
	char buf_tmp[16];
	uint8_t byte_length = 0;
	uint8_t length = 0;
	unsigned long result = 0;
	uint8_t loop_i = 0;
	uint16_t base = 2;
	char *data_str = NULL;
	uint8_t w_data[20];
	uint8_t x_pos[20];
	uint8_t count = 0;
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (len >= 80) {
		TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len)) {
		return -EFAULT;
	}

	buf[len] = '\0';

	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memset(w_data, 0x0, sizeof(w_data));
	memset(x_pos, 0x0, sizeof(x_pos));

	TPD_INFO("himax %s \n", buf);

	if ((buf[0] == 'r' || buf[0] == 'w') && buf[1] == ':' && buf[2] == 'x') {
		length = strlen(buf);

		/*TPD_INFO("%s: length = %d.\n", __func__,length);*/
		for (loop_i = 0; loop_i < length; loop_i++) { /*find postion of 'x'*/
			if (buf[loop_i] == 'x') {
				x_pos[count] = loop_i;
				count++;
			}
		}

		data_str = strrchr(buf, 'x');
		TPD_INFO("%s: %s.\n", __func__, data_str);
		length = strlen(data_str + 1) - 1;

		if (buf[0] == 'r') {
			if (buf[3] == 'F' && buf[4] == 'E' && length == 4) {
				length = length - base;
				chip_info->cfg_flag = true;
				memcpy(buf_tmp, data_str + base + 1, length);

			} else {
				chip_info->cfg_flag = false;
				memcpy(buf_tmp, data_str + 1, length);
			}

			byte_length = length / 2;

			if (!kstrtoul(buf_tmp, 16, &result)) {
				for (loop_i = 0; loop_i < byte_length; loop_i++) {
					chip_info->register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
				}
			}

		} else if (buf[0] == 'w') {
			if (buf[3] == 'F' && buf[4] == 'E') {
				chip_info->cfg_flag = true;
				memcpy(buf_tmp, buf + base + 3, length);

			} else {
				chip_info->cfg_flag = false;
				memcpy(buf_tmp, buf + 3, length);
			}

			if (count < 3) {
				byte_length = length / 2;

				if (!kstrtoul(buf_tmp, 16, &result)) { /*command*/
					for (loop_i = 0; loop_i < byte_length; loop_i++) {
						chip_info->register_command[loop_i] = (uint8_t)(result >> loop_i * 8);
					}
				}

				if (!kstrtoul(data_str + 1, 16, &result)) { /*data*/
					for (loop_i = 0; loop_i < byte_length; loop_i++) {
						w_data[loop_i] = (uint8_t)(result >> loop_i * 8);
					}
				}

				himax_register_write(chip_info, chip_info->register_command, byte_length,
						     w_data, chip_info->cfg_flag);

			} else {
				byte_length = x_pos[1] - x_pos[0] - 2;

				for (loop_i = 0; loop_i < count; loop_i++) { /*parsing addr after 'x'*/
					memcpy(buf_tmp, buf + x_pos[loop_i] + 1, byte_length);

					/*TPD_INFO("%s: buf_tmp = %s\n", __func__,buf_tmp);*/
					if (!kstrtoul(buf_tmp, 16, &result)) {
						if (loop_i == 0) {
							chip_info->register_command[loop_i] = (uint8_t)(result);
							/*TPD_INFO("%s: chip_info->register_command = %X\n", __func__,chip_info->register_command[0]);*/

						} else {
							w_data[loop_i - 1] = (uint8_t)(result);
							/*TPD_INFO("%s: w_data[%d] = %2X\n", __func__,loop_i - 1,w_data[loop_i - 1]);*/
						}
					}
				}

				byte_length = count - 1;
				himax_register_write(chip_info, chip_info->register_command, byte_length,
						     &w_data[0], chip_info->cfg_flag);
			}

		} else {
			return len;
		}
	}

	return len;
}

void himax_return_event_stack(struct chip_data_hx83112a_nf *chip_info)
{
	int retry = 20;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	TPD_INFO("%s:entering\n", __func__);

	do {
		TPD_INFO("%s, now %d times\n!", __func__, retry);
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		retry--;
		/*msleep(10);*/
	} while ((tmp_data[1] != 0x00 && tmp_data[0] != 0x00) && retry > 0);

	TPD_INFO("%s: End of setting!\n", __func__);
}
/*IC_BASED_END*/

int himax_write_read_reg(struct chip_data_hx83112a_nf *chip_info,
			 uint8_t *tmp_addr, uint8_t *tmp_data, uint8_t hb, uint8_t lb)
{
	int cnt = 0;

	do {
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

		msleep(20);
		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s:Now tmp_data[0] = 0x%02X, [1] = 0x%02X, [2] = 0x%02X, [3] = 0x%02X\n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
	} while ((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

	if (cnt >= 99) {
		TPD_INFO("himax_write_read_reg ERR Now register 0x%08X : high byte = 0x%02X, low byte = 0x%02X\n",
			 tmp_addr[3], tmp_data[1], tmp_data[0]);
		return -1;
	}

	TPD_INFO("Now register 0x%08X : high byte = 0x%02X, low byte = 0x%02X\n",
		 tmp_addr[3], tmp_data[1], tmp_data[0]);
	return NO_ERR;
}

void himax_get_DSRAM_data(struct chip_data_hx83112a_nf *chip_info,
			  uint8_t *info_data, uint8_t x_num, uint8_t y_num)
{
	int i = 0;
	/*int cnt = 0;*/
	unsigned char tmp_addr[4];
	unsigned char tmp_data[4];
	uint8_t max_i2c_size = MAX_RECVS_SZ;
	int m_key_num = 0;
	int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	int total_size_temp;
	int mutual_data_size = x_num * y_num * 2;
	int total_read_times = 0;
	int address = 0;
	uint8_t *temp_info_data; /*max mkey size = 8*/
	uint32_t check_sum_cal = 0;
	int fw_run_flag = -1;
	/*uint16_t temp_check_sum_cal = 0;*/

	temp_info_data = tp_devm_kzalloc(&chip_info->hx_spi->dev,
					 sizeof(uint8_t) * (total_size + 8), GFP_KERNEL);

	/*1. Read number of MKey R100070E8H to determin data size*/
	m_key_num = 0;
	/*TPD_INFO("%s,m_key_num=%d\n",__func__,m_key_num);*/
	total_size += m_key_num * 2;

	/* 2. Start DSRAM Rawdata and Wait Data Ready */
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x00;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x5A;
	tmp_data[0] = 0xA5;
	fw_run_flag = himax_write_read_reg(chip_info, tmp_addr, tmp_data, 0xA5, 0x5A);

	if (fw_run_flag < 0) {
		TPD_INFO("%s Data NOT ready => bypass \n", __func__);
		goto OUT;
	}

	/* 3. Read RawData */
	total_size_temp = total_size;
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x00;

	if (total_size % max_i2c_size == 0) {
		total_read_times = total_size / max_i2c_size;

	} else {
		total_read_times = total_size / max_i2c_size + 1;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_i2c_size) {
			himax_register_read(chip_info, tmp_addr, max_i2c_size,
					    &temp_info_data[i * max_i2c_size], false);
			total_size_temp = total_size_temp - max_i2c_size;

		} else {
			/*TPD_INFO("last total_size_temp=%d\n",total_size_temp);*/
			himax_register_read(chip_info, tmp_addr, total_size_temp % max_i2c_size,
					    &temp_info_data[i * max_i2c_size], false);
		}

		address = ((i + 1) * max_i2c_size);
		tmp_addr[1] = (uint8_t)((address >> 8) & 0x00FF);
		tmp_addr[0] = (uint8_t)((address) & 0x00FF);
	}

	/* 4. FW stop outputing */
	/*TPD_INFO("chip_info->dsram_flag=%d\n",chip_info->dsram_flag);*/
	if (chip_info->dsram_flag == false) {
		/*TPD_INFO("Return to Event Stack!\n");*/
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	} else {
		/*TPD_INFO("Continue to SRAM!\n");*/
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x00;
		tmp_addr[0] = 0x00;
		tmp_data[3] = 0x11;
		tmp_data[2] = 0x22;
		tmp_data[1] = 0x33;
		tmp_data[0] = 0x44;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
	}

	/* 5. Data Checksum Check */
	for (i = 2; i < total_size; i = i + 2) { /* 2:PASSWORD NOT included */
		check_sum_cal += (temp_info_data[i + 1] * 256 + temp_info_data[i]);
		printk("0x%2x:0x%4x ", temp_info_data[i], check_sum_cal);

		if (i % 32 == 0) {
			printk("\n");
		}
	}

	if (check_sum_cal % 0x10000 != 0) {
		memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
		TPD_INFO("%s check_sum_cal fail=%2x \n", __func__, check_sum_cal);
		goto OUT;

	} else {
		memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
		TPD_INFO("%s checksum PASS \n", __func__);
	}

OUT:
	tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_info_data,
		      sizeof(uint8_t) * (total_size + 8));
}

void himax_ts_diag_func(struct chip_data_hx83112a_nf *chip_info,
			int32_t *mutual_data)
{
	int i = 0;
	int j = 0;
	unsigned int index = 0;
	int total_size = chip_info->hw_res->tx_num * chip_info->hw_res->rx_num * 2;
	uint8_t info_data[total_size];

	int32_t new_data;
	/* 1:common dsram,2:100 frame Max,3:N-(N-1)frame */
	int dsram_type = 0;
	char write_buf[total_size * 3];

	memset(write_buf, '\0', sizeof(write_buf));

	dsram_type = chip_info->diag_command / 10;

	TPD_INFO("%s:Entering chip_info->diag_command=%d\n!", __func__,
		 chip_info->diag_command);

	if (dsram_type == 8) {
		dsram_type = 1;
		TPD_INFO("%s Sorting Mode run sram type1 ! \n", __func__);
	}

	himax_burst_enable(chip_info, 1);
	himax_get_DSRAM_data(chip_info, info_data, chip_info->hw_res->rx_num,
			     chip_info->hw_res->tx_num);

	index = 0;

	for (i = 0; i < chip_info->hw_res->tx_num; i++) {
		for (j = 0; j < chip_info->hw_res->rx_num; j++) {
			new_data = ((int8_t)info_data[index + 1] << 8 | info_data[index]);
			mutual_data[i * chip_info->hw_res->rx_num + j] = new_data;
			index += 2;
		}
	}
}

void diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num,
			 int self_num, uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data)
{
	int raw_data_len_word;
	int index = 0;
	int temp1, temp2, i;

	if (hx_touch_data->hx_rawdata_buf[0] == 0x3A
			&& hx_touch_data->hx_rawdata_buf[1] == 0xA3
			&& hx_touch_data->hx_rawdata_buf[2] > 0
			&& hx_touch_data->hx_rawdata_buf[3] == diag_cmd) {
		raw_data_len_word = hx_touch_data->rawdata_size / 2;
		index = (hx_touch_data->hx_rawdata_buf[2] - 1) * raw_data_len_word;

		/*TPD_INFO("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);*/
		/*TPD_INFO("raw_data_len=%d , raw_data_len_word=%d , hx_touch_info_size=%d\n", raw_data_len, raw_data_len_word, hx_touch_info_size);*/
		for (i = 0; i < raw_data_len_word; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) {
				/*mutual*/
				mutual_data[index + i] = ((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 + 4 + 1])
							 * 256 + hx_touch_data->hx_rawdata_buf[i * 2 +
									 4]; /* 4: RawData Header, 1:HSB  */

			} else {
				/*self*/
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2) {
					break;
				}

				self_data[i + index - mul_num] = (((int8_t)hx_touch_data->hx_rawdata_buf[i * 2 +
								   4 + 1]) << 8) | hx_touch_data->hx_rawdata_buf[i * 2 +
										   4]; /* 4: RawData Header */
				/*self_data[i+index-mul_num+1] = hx_touch_data->hx_rawdata_buf[i*2 + 4 + 1];*/
			}
		}
	}
}


/*return checksum value  */
bool diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/*Check 128th byte CRC*/
	for (i = 0, check_sum_cal = 0;
			i < (hx_touch_data->touch_all_size - hx_touch_data->touch_info_size);
			i = i + 2) {
		check_sum_cal += (hx_touch_data->hx_rawdata_buf[i + 1] * 256 +
				  hx_touch_data->hx_rawdata_buf[i]);
	}

	if (check_sum_cal % 0x10000 != 0) {
		TPD_INFO("%s fail=%2X \n", __func__, check_sum_cal);
		return 0;
		/*goto bypass_checksum_failed_packet;*/
	}

	return 1;
}

static size_t hx83112a_nf_proc_diag_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	char messages[80] = {0};
	uint8_t command[2] = {0x00, 0x00};
	uint8_t receive[1];

	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	/* 0: common, other: dsram*/
	int storage_type = 0;
	/* 1:IIR, 2:DC, 3:Bank, 4:IIR2, 5:IIR2_N, 6:FIR2, 7:Baseline, 8:dump coord */
	int rawdata_type = 0;

	memset(receive, 0x00, sizeof(receive));

	if (len >= 80) {
		TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}

	if (messages[1] == 0x0A) {
		chip_info->diag_command = messages[0] - '0';

	} else {
		chip_info->diag_command = (messages[0] - '0') * 10 + (messages[1] - '0');
	}

	storage_type = chip_info->diag_command / 10;
	rawdata_type = chip_info->diag_command % 10;

	TPD_INFO(" messages       = %s\n"
		 " chip_info->diag_command = 0x%x\n"
		 " storage_type   = 0x%x\n"
		 " rawdata_type   = 0x%x\n",
		 messages, chip_info->diag_command, storage_type, rawdata_type);

	if (chip_info->diag_command > 0 && rawdata_type == 0) {
		TPD_INFO("[Himax]chip_info->diag_command = 0x%x, storage_type=%d, rawdata_type=%d! Maybe no support!\n",
			 chip_info->diag_command, storage_type, rawdata_type);
		chip_info->diag_command = 0x00;

	} else {
		TPD_INFO("[Himax]chip_info->diag_command = 0x%x, storage_type=%d, rawdata_type=%d\n",
			 chip_info->diag_command, storage_type, rawdata_type);
	}

	if (storage_type == 0 && rawdata_type > 0 && rawdata_type < 8) {
		TPD_INFO("%s, common\n", __func__);

		if (chip_info->dsram_flag) {
			/*(1) Clear DSRAM flag*/
			chip_info->dsram_flag = false;
			/*(2) Enable ISR*/
			enable_irq(chip_info->hx_irq);
			/*(3) FW leave sram and return to event stack*/
			himax_return_event_stack(chip_info);
		}

		command[0] = chip_info->diag_command;
		himax_diag_register_set(chip_info, command[0]);

	} else if (storage_type > 0 && storage_type < 8 && rawdata_type > 0
			&& rawdata_type < 8) {
		TPD_INFO("%s, dsram\n", __func__);

		/*0. set diag flag*/
		if (chip_info->dsram_flag) {
			/*(1) Clear DSRAM flag*/
			chip_info->dsram_flag = false;
			/*(2) Enable ISR*/
			enable_irq(chip_info->hx_irq);
			/*(3) FW leave sram and return to event stack*/
			himax_return_event_stack(chip_info);
		}

		switch (rawdata_type) {
		case 1:
			command[0] = 0x09; /*IIR*/
			break;

		case 2:
			command[0] = 0x0A;/*RAWDATA*/
			break;

		case 3:
			command[0] = 0x08;/*Baseline*/
			break;

		default:
			command[0] = 0x00;
			TPD_INFO("%s: Sram no support this type !\n", __func__);
			break;
		}

		himax_diag_register_set(chip_info, command[0]);
		TPD_INFO("%s: Start get raw data in DSRAM\n", __func__);
		/*1. Disable ISR*/
		disable_irq(chip_info->hx_irq);

		/*2. Set DSRAM flag*/
		chip_info->dsram_flag = true;

	} else {
		/*set diag flag*/
		if (chip_info->dsram_flag) {
			TPD_INFO("return and cancel sram thread!\n");
			/*(1) Clear DSRAM flag*/
			chip_info->dsram_flag = false;
			himax_return_event_stack(chip_info);
		}

		command[0] = 0x00;
		chip_info->diag_command = 0x00;
		himax_diag_register_set(chip_info, command[0]);
		TPD_INFO("return to normal chip_info->diag_command = 0x%x\n",
			 chip_info->diag_command);
	}

	return len;
}

static size_t hx83112a_nf_proc_diag_read(struct file *file, char *buff,
		size_t len, loff_t *pos)
{
	size_t ret = 0;
	char *temp_buf;
	uint16_t mutual_num;
	uint16_t self_num;
	uint16_t width;
	int dsram_type = 0;
	int data_type = 0;
	int i = 0;
	int j = 0;
	int k = 0;

	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (!chip_info->proc_send_flag) {
		temp_buf = tp_devm_kzalloc(&chip_info->hx_spi->dev, len, GFP_KERNEL);

		if (!temp_buf) {
			goto RET_OUT;
		}

		dsram_type = chip_info->diag_command / 10;
		data_type = chip_info->diag_command % 10;

		mutual_num = chip_info->hw_res->tx_num * chip_info->hw_res->rx_num;
		self_num = chip_info->hw_res->tx_num +
			   chip_info->hw_res->rx_num; /*don't add KEY_COUNT*/
		width = chip_info->hw_res->rx_num;
		ret += snprintf(temp_buf + ret, len - ret,
				"ChannelStart (rx tx) : %4d, %4d\n\n", chip_info->hw_res->rx_num,
				chip_info->hw_res->tx_num);

		/* start to show out the raw data in adb shell*/
		if ((data_type >= 1 && data_type <= 7)) {
			if (dsram_type > 0) {
				himax_ts_diag_func(chip_info, chip_info->hx_touch_data->diag_mutual);
			}

			for (j = 0; j < chip_info->hw_res->rx_num; j++) {
				for (i = 0; i < chip_info->hw_res->tx_num; i++) {
					k = ((mutual_num - j) - chip_info->hw_res->rx_num * i) - 1;
					ret += snprintf(temp_buf + ret, len - ret, "%6d",
							chip_info->hx_touch_data->diag_mutual[k]);
				}

				ret += snprintf(temp_buf + ret, len - ret, " %6d\n", chip_info->diag_self[j]);
			}

			ret += snprintf(temp_buf + ret, len - ret, "\n");

			for (i = 0; i < chip_info->hw_res->tx_num; i++) {
				ret += snprintf(temp_buf + ret, len - ret, "%6d", chip_info->diag_self[i]);
			}
		}

		ret += snprintf(temp_buf + ret, len - ret, "\n");
		ret += snprintf(temp_buf + ret, len - ret, "ChannelEnd");
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		/*if ((chip_info->diag_command >= 1 && chip_info->diag_command <= 7) || dsram_type > 0)*/
		{
			/* print Mutual/Slef Maximum and Minimum */
			/*himax_get_mutual_edge();*/
			for (i = 0; i < (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num); i++) {
				if (chip_info->hx_touch_data->diag_mutual[i] > chip_info->max_mutual) {
					chip_info->max_mutual = chip_info->hx_touch_data->diag_mutual[i];
				}

				if (chip_info->hx_touch_data->diag_mutual[i] < chip_info->min_mutual) {
					chip_info->min_mutual = chip_info->hx_touch_data->diag_mutual[i];
				}
			}

			/*himax_get_self_edge();*/
			for (i = 0; i < (chip_info->hw_res->tx_num + chip_info->hw_res->rx_num); i++) {
				if (chip_info->diag_self[i] > chip_info->max_self) {
					chip_info->max_self = chip_info->diag_self[i];
				}

				if (chip_info->diag_self[i] < chip_info->min_self) {
					chip_info->min_self = chip_info->diag_self[i];
				}
			}

			ret += snprintf(temp_buf + ret, len - ret, "Mutual Max:%3d, Min:%3d\n",
					chip_info->max_mutual, chip_info->min_mutual);
			ret += snprintf(temp_buf + ret, len - ret, "Self Max:%3d, Min:%3d\n",
					chip_info->max_self, chip_info->min_self);

			/* recovery status after print*/
			chip_info->max_mutual = 0;
			chip_info->min_mutual = 0xFFFF;
			chip_info->max_self = 0;
			chip_info->min_self = 0xFFFF;
		}

		if (copy_to_user(buff, temp_buf, len)) {
			TPD_INFO("%s, here:%d\n", __func__, __LINE__);
		}

		chip_info->proc_send_flag = 1;
	RET_OUT:

		if (temp_buf) {
			tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_buf, len);
		}

	} else {
		chip_info->proc_send_flag = 0;
	}

	return ret;
}

uint8_t himax_read_DD_status(struct chip_data_hx83112a_nf *chip_info,
			     uint8_t *cmd_set, uint8_t *tmp_data)
{
	int cnt = 0;
	uint8_t req_size = cmd_set[0];
	uint8_t cmd_addr[4] = {0xFC, 0x00, 0x00, 0x90}; /*0x900000FC -> cmd and hand shaking*/
	uint8_t tmp_addr[4] = {0x80, 0x7F, 0x00, 0x10}; /*0x10007F80 -> data space*/

	cmd_set[3] = 0xAA;
	himax_register_write(chip_info, cmd_addr, 4, cmd_set, 0);

	TPD_INFO("%s: cmd_set[0] = 0x%02X, cmd_set[1] = 0x%02X, cmd_set[2] = 0x%02X, cmd_set[3] = 0x%02X\n",
		 __func__, cmd_set[0], cmd_set[1], cmd_set[2], cmd_set[3]);

	for (cnt = 0; cnt < 100; cnt++) {
		himax_register_read(chip_info, cmd_addr, 4, tmp_data, false);
		TPD_INFO("%s: tmp_data[0] = 0x%02X, tmp_data[1] = 0x%02X, tmp_data[2] = 0x%02X, tmp_data[3] = 0x%02X, cnt=%d\n",
			 __func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3], cnt);
		msleep(10);

		if (tmp_data[3] == 0xBB) {
			TPD_INFO("%s Data ready goto moving data\n", __func__);
			break;

		} else if (cnt >= 99) {
			TPD_INFO("%s Data not ready in FW \n", __func__);
			return FW_NOT_READY;
		}
	}

	himax_register_read(chip_info, tmp_addr, req_size, tmp_data, false);
	return NO_ERR;
}

static size_t hx83112a_nf_proc_DD_debug_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	int ret = 0;
	uint8_t tmp_data[64];
	uint8_t loop_i = 0;
	char *temp_buf;

	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (!chip_info->proc_send_flag) {
		temp_buf = tp_devm_kzalloc(&chip_info->hx_spi->dev, len, GFP_KERNEL);

		if (chip_info->mutual_set_flag == 1) {
			if (himax_read_DD_status(chip_info, chip_info->cmd_set, tmp_data) == NO_ERR) {
				for (loop_i = 0; loop_i < chip_info->cmd_set[0]; loop_i++) {
					if ((loop_i % 8) == 0) {
						ret += snprintf(temp_buf + ret, len - ret, "0x%02X : ", loop_i);
					}

					ret += snprintf(temp_buf + ret, len - ret, "0x%02X ", tmp_data[loop_i]);

					if ((loop_i % 8) == 7) {
						ret += snprintf(temp_buf + ret, len - ret, "\n");
					}
				}
			}
		}

		/*else*/
		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len)) {
			TPD_INFO("%s, here:%d\n", __func__, __LINE__);
		}

		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_buf, len);
		chip_info->proc_send_flag = 1;

	} else {
		chip_info->proc_send_flag = 0;
	}

	return ret;
}

static size_t hx83112a_nf_proc_DD_debug_write(struct file *file,
		const char *buff, size_t len, loff_t *pos)
{
	uint8_t i = 0;
	uint8_t cnt = 2;
	unsigned long result = 0;
	char buf_tmp[20];
	char buf_tmp2[4];

	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (len >= 20) {
		TPD_INFO("%s: no command exceeds 20 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len)) {
		return -EFAULT;
	}

	memset(buf_tmp2, 0x0, sizeof(buf_tmp2));

	if (buf_tmp[2] == 'x' && buf_tmp[6] == 'x' && buf_tmp[10] == 'x') {
		chip_info->mutual_set_flag = 1;

		for (i = 3; i < 12; i = i + 4) {
			memcpy(buf_tmp2, buf_tmp + i, 2);

			if (!kstrtoul(buf_tmp2, 16, &result)) {
				chip_info->cmd_set[cnt] = (uint8_t)result;

			} else {
				TPD_INFO("String to oul is fail in cnt = %d, buf_tmp2 = %s", cnt, buf_tmp2);
			}

			cnt--;
		}

		TPD_INFO("cmd_set[2] = %02X, cmd_set[1] = %02X, cmd_set[0] = %02X\n",
			 chip_info->cmd_set[2], chip_info->cmd_set[1], chip_info->cmd_set[0]);

	} else {
		chip_info->mutual_set_flag = 0;
	}

	return len;
}

int himax_read_FW_status(struct chip_data_hx83112a_nf *chip_info,
			 uint8_t *state_addr, uint8_t *tmp_addr)
{
	uint8_t req_size = 0;
	uint8_t status_addr[4] = {0x44, 0x7F, 0x00, 0x10}; /*0x10007F44*/
	uint8_t cmd_addr[4] = {0xF8, 0x00, 0x00, 0x90}; /*0x900000F8*/

	if (state_addr[0] == 0x01) {
		state_addr[1] = 0x04;
		state_addr[2] = status_addr[0];
		state_addr[3] = status_addr[1];
		state_addr[4] = status_addr[2];
		state_addr[5] = status_addr[3];
		req_size = 0x04;
		himax_sense_off(chip_info);
		himax_register_read(chip_info, status_addr, req_size, tmp_addr, false);
		himax_sense_on(chip_info, 1);

	} else if (state_addr[0] == 0x02) {
		state_addr[1] = 0x30;
		state_addr[2] = cmd_addr[0];
		state_addr[3] = cmd_addr[1];
		state_addr[4] = cmd_addr[2];
		state_addr[5] = cmd_addr[3];
		req_size = 0x30;
		himax_register_read(chip_info, cmd_addr, req_size, tmp_addr, false);
	}

	return NO_ERR;
}

static size_t hx83112a_nf_proc_FW_debug_read(struct file *file, char *buf,
		size_t len, loff_t *pos)
{
	int ret = 0;
	uint8_t loop_i = 0;
	uint8_t tmp_data[64];
	char *temp_buf;

	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (!chip_info->proc_send_flag) {
		temp_buf = tp_devm_kzalloc(&chip_info->hx_spi->dev, len, GFP_KERNEL);

		chip_info->cmd_set[0] = 0x01;

		if (himax_read_FW_status(chip_info, chip_info->cmd_set, tmp_data) == NO_ERR) {
			ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t",
					chip_info->cmd_set[5], chip_info->cmd_set[4], chip_info->cmd_set[3],
					chip_info->cmd_set[2]);

			for (loop_i = 0; loop_i < chip_info->cmd_set[1]; loop_i++) {
				ret += snprintf(temp_buf + ret, len - ret, "%5d\t", tmp_data[loop_i]);
			}

			ret += snprintf(temp_buf + ret, len - ret, "\n");
		}

		chip_info->cmd_set[0] = 0x02;

		if (himax_read_FW_status(chip_info, chip_info->cmd_set, tmp_data) == NO_ERR) {
			for (loop_i = 0; loop_i < chip_info->cmd_set[1]; loop_i = loop_i + 2) {
				if ((loop_i % 16) == 0)
					ret += snprintf(temp_buf + ret, len - ret, "0x%02X%02X%02X%02X :\t",
							chip_info->cmd_set[5], chip_info->cmd_set[4],
							chip_info->cmd_set[3] + (((chip_info->cmd_set[2] + loop_i) >> 8) & 0xFF),
							(chip_info->cmd_set[2] + loop_i) & 0xFF);

				ret += snprintf(temp_buf + ret, len - ret, "%5d\t",
						tmp_data[loop_i] + (tmp_data[loop_i + 1] << 8));

				if ((loop_i % 16) == 14) {
					ret += snprintf(temp_buf + ret, len - ret, "\n");
				}
			}
		}

		ret += snprintf(temp_buf + ret, len - ret, "\n");

		if (copy_to_user(buf, temp_buf, len)) {
			TPD_INFO("%s, here:%d\n", __func__, __LINE__);
		}

		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)temp_buf, len);
		chip_info->proc_send_flag = 1;

	} else {
		chip_info->proc_send_flag = 0;
	}

	return ret;
}

static int hx83112a_nf_configuration_init(
				struct chip_data_hx83112a_nf *chip_info, bool config)
{
	int ret = 0;
	TPD_INFO("%s, configuration init = %d\n", __func__, config);
	return ret;
}

int himax_ic_reset(struct chip_data_hx83112a_nf *chip_info, uint8_t loadconfig,
		   uint8_t int_off)
{
	int ret = 0;
	chip_info->hw_reset_active = 1;

	TPD_INFO("%s, status: loadconfig=%d, int_off=%d\n", __func__, loadconfig,
		 int_off);

	if (chip_info->hw_res->reset_gpio) {
		if (int_off) {
			ret = hx83112a_nf_enable_interrupt(chip_info, false);

			if (ret < 0) {
				TPD_INFO("%s: hx83112a_nf enable interrupt failed.\n", __func__);
				return ret;
			}
		}

		hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/

		hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/

		if (loadconfig) {
			ret = hx83112a_nf_configuration_init(chip_info, false);

			if (ret < 0) {
				TPD_INFO("%s: hx83112a_nf configuration init failed.\n", __func__);
				return ret;
			}

			ret = hx83112a_nf_configuration_init(chip_info, true);

			if (ret < 0) {
				TPD_INFO("%s: hx83112a_nf configuration init failed.\n", __func__);
				return ret;
			}
		}

		if (int_off) {
			ret = hx83112a_nf_enable_interrupt(chip_info, true);

			if (ret < 0) {
				TPD_INFO("%s: hx83112a_nf enable interrupt failed.\n", __func__);
				return ret;
			}
		}
	}

	return 0;
}

static size_t hx83112a_nf_proc_reset_write(struct file *file, const char *buff,
		size_t len, loff_t *pos)
{
	char buf_tmp[12];
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (len >= 12) {
		TPD_INFO("%s: no command exceeds 12 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf_tmp, buff, len)) {
		return -EFAULT;
	}

	if (buf_tmp[0] == '1') {
		himax_ic_reset(chip_info, false, false);

	} else if (buf_tmp[0] == '2') {
		himax_ic_reset(chip_info, false, true);

	} else if (buf_tmp[0] == '3') {
		himax_ic_reset(chip_info, true, false);

	} else if (buf_tmp[0] == '4') {
		himax_ic_reset(chip_info, true, true);

	} else if ((buf_tmp[0] == 'z') && (buf_tmp[1] == 'r')) {
		himax_mcu_0f_operation_check(chip_info, 0);
		himax_mcu_0f_operation_check(chip_info, 1);

	} else if ((buf_tmp[0] == 'z') && (buf_tmp[1] == 'w')) {
		himax_mcu_0f_operation_dirly(chip_info);
	}

	return len;
}

static size_t hx83112a_nf_proc_sense_on_off_write(struct file *file,
		const char *buff,
		size_t len, loff_t *pos)
{
	char buf[80] = {0};
	struct touchpanel_data *ts = PDE_DATA(file_inode(file));
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			ts->chip_data;

	if (len >= 80) {
		TPD_INFO("%s: no command exceeds 80 chars.\n", __func__);
		return -EFAULT;
	}

	if (copy_from_user(buf, buff, len)) {
		return -EFAULT;
	}

	if (buf[0] == '0') {
		himax_sense_off(chip_info);
		TPD_INFO("Sense off \n");

	} else if (buf[0] == '1') {
		if (buf[1] == 's') {
			himax_sense_on(chip_info, 0x00);
			TPD_INFO("Sense on re-map on, run sram \n");

		} else {
			himax_sense_on(chip_info, 0x01);
			TPD_INFO("Sense on re-map off, run flash \n");
		}

	} else {
		TPD_INFO("Do nothing \n");
	}

	return len;
}
/*add for himax end*/
#ifdef CONFIG_OPLUS_TP_APK
static void himax_gesture_debug_mode_set(
				struct chip_data_hx83112a_nf *chip_info, bool on_off)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	char buf[80] = {0};
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x7F;
	tmp_addr[0] = 0xF8;

	if (on_off) {
		chip_info->switch_algo = buf[0];
		chip_info->check_point_format = 1;
		tmp_data[3] = 0xA1;
		tmp_data[2] = 0x1A;
		tmp_data[1] = 0xA1;
		tmp_data[0] = 0x1A;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		TPD_INFO("%s: Report 40 trajectory coordinate points .\n", __func__);

	} else {
		chip_info->switch_algo = 0;
		chip_info->check_point_format = 0;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;

		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		TPD_INFO("%s: close FW enter algorithm switch.\n", __func__);
	}
}


static void himax_debug_mode_set(
				struct chip_data_hx83112a_nf *chip_info, bool on_off)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	char buf[80] = {0};
	tmp_addr[3] = 0x10;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x7F;
	tmp_addr[0] = 0xF8;

	if (on_off) {
		chip_info->switch_algo = buf[0];
		chip_info->check_point_format = 0;
		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		TPD_INFO("%s: open FW enter algorithm switch.\n", __func__);

	} else {
		chip_info->switch_algo = 0;
		chip_info->check_point_format = 0;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;

		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		TPD_INFO("%s: close FW enter algorithm switch.\n", __func__);
	}
}

static void himax_debug_sta_judge(struct chip_data_hx83112a_nf *chip_info)
{
	static struct himax_fw_debug_info last_sta;
	struct himax_fw_debug_info sta;

	memcpy(&sta, &chip_info->hx_touch_data->hx_state_info[3], sizeof(sta));

	if (last_sta.recal0 != sta.recal0) {
		if (sta.recal0) {
			log_buf_write(private_ts, 1);

		} else {
			log_buf_write(private_ts, 2);
		}
	}

	if (last_sta.recal1 != sta.recal1) {
		if (sta.recal1) {
			log_buf_write(private_ts, 4);

		} else {
			log_buf_write(private_ts, 3);
		}
	}

	if (last_sta.paseline != sta.paseline) {
		if (sta.paseline) {
			log_buf_write(private_ts, 5);

		} else {
			/*log_buf_write(private_ts, 4);*/
		}
	}

	if (last_sta.palm != sta.palm) {
		if (sta.palm) {
			log_buf_write(private_ts, 7);

		} else {
			log_buf_write(private_ts, 6);
		}
	}

	if (last_sta.idle != sta.idle) {
		if (sta.idle) {
			log_buf_write(private_ts, 9);

		} else {
			log_buf_write(private_ts, 8);
		}
	}

	if (last_sta.water != sta.water) {
		if (sta.water) {
			log_buf_write(private_ts, 11);

		} else {
			log_buf_write(private_ts, 10);
		}
	}

	if (last_sta.hopping != sta.hopping) {
		if (sta.hopping) {
			log_buf_write(private_ts, 13);

		} else {
			log_buf_write(private_ts, 12);
		}
	}

	if (last_sta.noise != sta.noise) {
		if (sta.noise) {
			log_buf_write(private_ts, 15);

		} else {
			log_buf_write(private_ts, 14);
		}
	}

	if (last_sta.glove != sta.glove) {
		if (sta.glove) {
			log_buf_write(private_ts, 17);

		} else {
			log_buf_write(private_ts, 16);
		}
	}

	if (last_sta.border != sta.border) {
		if (sta.border) {
			log_buf_write(private_ts, 19);

		} else {
			log_buf_write(private_ts, 18);
		}
	}

	if (last_sta.vr != sta.vr) {
		if (sta.vr) {
			log_buf_write(private_ts, 21);

		} else {
			log_buf_write(private_ts, 20);
		}
	}

	if (last_sta.big_small != sta.big_small) {
		if (sta.big_small) {
			log_buf_write(private_ts, 23);

		} else {
			log_buf_write(private_ts, 22);
		}
	}

	if (last_sta.one_block != sta.one_block) {
		if (sta.one_block) {
			log_buf_write(private_ts, 25);

		} else {
			log_buf_write(private_ts, 24);
		}
	}

	if (last_sta.blewing != sta.blewing) {
		if (sta.blewing) {
			log_buf_write(private_ts, 27);

		} else {
			log_buf_write(private_ts, 26);
		}
	}

	if (last_sta.thumb_flying != sta.thumb_flying) {
		if (sta.thumb_flying) {
			log_buf_write(private_ts, 29);

		} else {
			log_buf_write(private_ts, 28);
		}
	}

	if (last_sta.border_extend != sta.border_extend) {
		if (sta.border_extend) {
			log_buf_write(private_ts, 31);

		} else {
			log_buf_write(private_ts, 30);
		}
	}

	memcpy(&last_sta, &sta, sizeof(last_sta));

	if (tp_debug > 0) {
		TPD_INFO("The sta  is = 0x%02X,0x%02X\n",
			 chip_info->hx_touch_data->hx_state_info[3],
			 chip_info->hx_touch_data->hx_state_info[4]);
	}

	return;
}


#endif

static int hx83112a_nf_get_touch_points(void *chip_data,
					struct point_info *points, int max_num)
{
	int i, x, y, z = 1, obj_attention = 0;

	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	char buf[128];
	uint16_t mutual_num;
	uint16_t self_num;
	int ret = 0;
	int check_sum_cal;
	int ts_status = HX_REPORT_COORD;
	int hx_point_num;
	uint8_t hx_state_info_pos;

	if (!chip_info->hx_touch_data) {
		TPD_INFO("%s:%d chip_info->hx_touch_data is NULL\n", __func__, __LINE__);
	}

	if (!chip_info->hx_touch_data->hx_coord_buf) {
		TPD_INFO("%s:%d chip_info->hx_touch_data->hx_coord_buf is NULL\n", __func__,
			 __LINE__);
		return 0;
	}

	himax_burst_enable(chip_info, 0);

	if (chip_info->diag_command) {
		ret = himax_read_event_stack(chip_info, buf, 128);

	} else {
		ret = himax_read_event_stack(chip_info, buf,
					     chip_info->hx_touch_data->touch_info_size);
	}

	if (!ret) {
		TPD_INFO("%s: can't read data from chip in normal!\n", __func__);
		goto checksum_fail;
	}

	if (LEVEL_DEBUG == tp_debug) {
		himax_log_touch_data(buf, chip_info->hx_touch_data);
	}

	check_sum_cal = himax_checksum_cal(chip_info, buf, ts_status);/*????checksum*/

	if (check_sum_cal == CHECKSUM_FAIL) {
		goto checksum_fail;

	} else if (check_sum_cal == ERR_WORK_OUT) {
		goto err_workqueue_out;

	} else if (check_sum_cal == WORK_OUT) {
		goto workqueue_out;
	}

	/*himax_assign_touch_data(buf,ts_status); ??buf??, ??hx_coord_buf*/

	hx_state_info_pos = chip_info->hx_touch_data->touch_info_size - 6;

	if (ts_status == HX_REPORT_COORD) {
		memcpy(chip_info->hx_touch_data->hx_coord_buf, &buf[0],
		       chip_info->hx_touch_data->touch_info_size);

		if (buf[hx_state_info_pos] != 0xFF && buf[hx_state_info_pos + 1] != 0xFF) {
			memcpy(chip_info->hx_touch_data->hx_state_info, &buf[hx_state_info_pos], 5);
#ifdef CONFIG_OPLUS_TP_APK

			if (chip_info->debug_mode_sta) {
				himax_debug_sta_judge(chip_info);
			}

#endif

		} else {
			memset(chip_info->hx_touch_data->hx_state_info, 0x00,
			       sizeof(chip_info->hx_touch_data->hx_state_info));
		}
	}

	if (chip_info->diag_command) {
		mutual_num = chip_info->hw_res->tx_num * chip_info->hw_res->rx_num;
		self_num = chip_info->hw_res->tx_num + chip_info->hw_res->rx_num;
		TPD_INFO("chip_info->hx_touch_data->touch_all_size= %d chip_info->hx_touch_data->touch_info_size = %d, %d\n",
			 \
			 chip_info->hx_touch_data->touch_all_size,
			 chip_info->hx_touch_data->touch_info_size,
			 chip_info->hx_touch_data->touch_all_size -
			 chip_info->hx_touch_data->touch_info_size);
		memcpy(chip_info->hx_touch_data->hx_rawdata_buf,
		       &buf[chip_info->hx_touch_data->touch_info_size],
		       chip_info->hx_touch_data->touch_all_size -
		       chip_info->hx_touch_data->touch_info_size);

		if (!diag_check_sum(chip_info->hx_touch_data)) {
			goto err_workqueue_out;
		}

		diag_parse_raw_data(chip_info->hx_touch_data, mutual_num, self_num,
				    chip_info->diag_command, chip_info->hx_touch_data->diag_mutual,
				    chip_info->diag_self);
	}

	if (chip_info->hx_touch_data->hx_coord_buf[chip_info->touch_info_point_cnt] ==
			0xff) { /*touch_info_point_cnt buf???????*/
		hx_point_num = 0;

	} else {
		hx_point_num =
			chip_info->hx_touch_data->hx_coord_buf[chip_info->touch_info_point_cnt] & 0x0f;
	}


	for (i = 0; i < 10; i++) {
		x = chip_info->hx_touch_data->hx_coord_buf[i * 4] << 8 |
		    chip_info->hx_touch_data->hx_coord_buf[i * 4 + 1];
		y = (chip_info->hx_touch_data->hx_coord_buf[i * 4 + 2] << 8 |
		     chip_info->hx_touch_data->hx_coord_buf[i * 4 + 3]);
		z = chip_info->hx_touch_data->hx_coord_buf[i + 40];

		if (x >= 0 && x <= private_ts->resolution_info.max_x && y >= 0
				&& y <= private_ts->resolution_info.max_y) {
			points[i].x = x;
			points[i].y = y;
			points[i].width_major = z;
			points[i].touch_major = z;
			points[i].status = 1;
			obj_attention = obj_attention | (0x0001 << i);
		}
	}

	/*TPD_DEBUG("%s:%d  obj_attention = 0x%x\n", __func__, __LINE__, obj_attention);*/

checksum_fail:
	return obj_attention;
err_workqueue_out:
workqueue_out:
	/*himax_ic_reset(chip_info, false, true);*/
	return -EINVAL;
}

static int hx83112a_nf_ftm_process(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
	return 0;
}

static int hx83112a_nf_get_vendor(void *chip_data,
				  struct panel_info *panel_data)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	chip_info->tp_type = panel_data->tp_type;
	chip_info->p_tp_fw = &panel_data->tp_fw;
	TPD_INFO("chip_info->tp_type = %d, panel_data->test_limit_name = %s, panel_data->fw_name = %s\n",
		 chip_info->tp_type, panel_data->test_limit_name, panel_data->fw_name);
	return 0;
}


static int hx83112a_nf_get_chip_info(void *chip_data)
{
	return 1;
}

/**
 * hx83112a_nf_get_fw_id -   get device fw id.
 * @chip_info: struct include i2c resource.
 * Return fw version result.
 */
static uint32_t hx83112a_nf_get_fw_id(struct chip_data_hx83112a_nf *chip_info)
{
	uint32_t current_firmware = 0;
	uint8_t cmd[4];
	uint8_t data[64];

	cmd[3] = 0x10;  /* oplus fw id bin address : 0xc014   , 49172    Tp ic address : 0x 10007014*/
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x14;
	himax_register_read(chip_info, cmd, 4, data, false);

	TPD_DEBUG("%s : data[0] = 0x%2.2X, data[1] = 0x%2.2X, data[2] = 0x%2.2X, data[3] = 0x%2.2X\n",
		  __func__, data[0], data[1], data[2], data[3]);

	current_firmware = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	TPD_INFO("CURRENT_FIRMWARE_ID = 0x%x\n", current_firmware);

	return current_firmware;
}

static fw_check_state hx83112a_nf_fw_check(void *chip_data,
		struct resolution_info *resolution_info, struct panel_info *panel_data)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	/*fw check normal need update tp_fw  && device info*/
	panel_data->tp_fw = hx83112a_nf_get_fw_id(chip_info);

	if (panel_data->manufacture_info.version) {
		sprintf(panel_data->manufacture_info.version, "0x%x", panel_data->tp_fw);
	}

	return FW_NORMAL;
}

static u32 hx83112a_nf_trigger_reason(void *chip_data, int gesture_enable,
				      int is_suspended)
{
	if ((gesture_enable == 1) && is_suspended) {
		return IRQ_GESTURE;

	} else {
		return IRQ_TOUCH;
	}
}
/*
static int hx83112a_nf_reset_for_prepare(void *chip_data)
{
    int ret = -1;
    //int i2c_error_number = 0;
    //struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)chip_data;

    TPD_INFO("%s.\n", __func__);
    ///hx83112a_nf_resetgpio_set(chip_info->hw_res, true); // reset gpio

    return ret;
}
*/
/*
static void hx83112a_nf_resume_prepare(void *chip_data)
{
    //hx83112a_nf_reset_for_prepare(chip_data);
    #ifdef HX_ZERO_FLASH
    TPD_DETAIL("It will update fw,if there is power-off in suspend!\n");

    g_zero_event_count = 0;

    hx83112a_nf_enable_interrupt(chip_info, false);

    // trigger reset
    //hx83112a_nf_resetgpio_set(chip_info->hw_res, false); // reset gpio
    //hx83112a_nf_resetgpio_set(chip_info->hw_res, true); // reset gpio

    g_core_fp.fp_0f_operation_dirly(chip_info);
    g_core_fp.fp_reload_disable(chip_info, 0);
    himax_sense_on(chip_info, 0x00);
    // need_modify
    // report all leave event
    //himax_report_all_leave_event(private_ts);

    hx83112a_nf_enable_interrupt(chip_info, true);
    #endif
}
*/
static void hx83112a_nf_exit_esd_mode(void *chip_data)
{
	TPD_INFO("exit esd mode ok\n");
	return;
}

/*
 * return success: 0; fail : negative
 */
static int hx83112a_nf_reset(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	int ret = 0;
	int load_fw_times = 10;

	TPD_INFO("%s.\n", __func__);

	if (!chip_info->first_download_finished) {
		TPD_INFO("%s:First download has not finished, don't do reset.\n", __func__);
		return 0;
	}

	g_zero_event_count = 0;

	clear_view_touchdown_flag(chip_info->tp_index); /*clear touch download flag*/
	/*esd hw reset*/
	chip_info->esd_reset_activate = 0;

	/*hx83112a_nf_enable_interrupt(chip_info, false);*/
	disable_irq_nosync(chip_info->hx_irq);


	do {
		load_fw_times--;
		g_core_fp.fp_0f_operation_dirly(chip_info);
		ret = g_core_fp.fp_reload_disable(chip_info,
						  0);/*success return 1, fail return 0*/
	} while (!ret && load_fw_times > 0);

	if (!load_fw_times) {
		TPD_INFO("%s: load_fw_times over 10 times\n", __func__);
	}

	himax_sense_on(chip_info, 0x00);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	enable_irq(chip_info->hx_irq);
#endif
	/*hx83112a_nf_enable_interrupt(chip_info, true);*/
	/*esd hw reset*/
	return ret;
}

void himax_ultra_enter(struct chip_data_hx83112a_nf *chip_info)
{
	uint8_t tmp_data[4];
	int rtimes = 0;

	TPD_INFO("%s:entering\n", __func__);

	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			TPD_INFO("%s:1/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0x11;

		if (himax_bus_write(chip_info, 0x34, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x34, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x34, correct 0x11 = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0x11);

	/* 33 -> 33 */
	rtimes = 0;

	do {
		if (rtimes > 10) {
			TPD_INFO("%s:2/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0x33;

		if (himax_bus_write(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0x33);

	/* 34 -> 22 */
	rtimes = 0;

	do {
		if (rtimes > 10) {
			TPD_INFO("%s:3/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0x22;

		if (himax_bus_write(chip_info, 0x34, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x34, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x34, correct 0x22 = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0x22);

	/* 33 -> AA */
	rtimes = 0;

	do {
		if (rtimes > 10) {
			TPD_INFO("%s:4/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0xAA;

		if (himax_bus_write(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0xAA);

	/* 33 -> 33 */
	rtimes = 0;

	do {
		if (rtimes > 10) {
			TPD_INFO("%s:5/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0x33;

		if (himax_bus_write(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x33, correct 0x33 = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0x33);

	/* 33 -> AA */
	rtimes = 0;

	do {
		if (rtimes > 10) {
			TPD_INFO("%s:6/6 retry over 10 times!\n", __func__);
			return;
		}

		tmp_data[0] = 0xAA;

		if (himax_bus_write(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi write fail!\n", __func__);
			continue;
		}

		tmp_data[0] = 0x00;

		if (himax_bus_read(chip_info, 0x33, 1, tmp_data) < 0) {
			TPD_INFO("%s: spi read fail!\n", __func__);
			continue;
		}

		TPD_INFO("%s:retry times %d, addr = 0x33, correct 0xAA = current 0x%2.2X\n",
			 __func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != 0xAA);

	TPD_INFO("%s:END\n", __func__);
}

static int hx83112a_nf_enable_black_gesture(struct chip_data_hx83112a_nf *chip_info,
	                                                   bool enable)
{
	int ret = 0;
	struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);

	TPD_INFO("%s:enable=%d, ts->is_suspended=%d \n", __func__, enable,
		 ts->is_suspended);

	if (ts->is_suspended) {
		if (enable) {
			hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/
			hx83112a_nf_resetgpio_set(chip_info->hw_res, false); /* reset gpio*/
			hx83112a_nf_resetgpio_set(chip_info->hw_res, true); /* reset gpio*/


#ifdef CONFIG_OPLUS_TP_APK

			if (chip_info->debug_gesture_sta) {
				himax_gesture_debug_mode_set(chip_info, true);
			}

#endif /* end of CONFIG_OPLUS_TP_APK*/
			/*himax_sense_on(chip_info, 0);*/
			/*if (!HX_RESET_STATE) {
			    ret =  hx83112a_nf_resetgpio_set(chip_info->hw_res, true); // reset gpio
			    if (ret < 0) {
			        TPD_INFO("%s: hx83112a_nf reset gpio failed.\n", __func__);
			        return ret;
			    }
			}*/
			/*ret = hx83112a_nf_enable_interrupt(chip_info, true);
			if (ret < 0) {
			    TPD_INFO("%s: hx83112a_nf enable interrupt failed.\n", __func__);
			    return ret;
			}*/

		} else {
			/*ret = hx83112a_nf_enable_interrupt(chip_info, false);
			if (ret < 0) {
			    TPD_INFO("%s: hx83112a_nf enable interrupt failed.\n", __func__);
			    return ret;
			}*/
			/*if (HX_RESET_STATE) {
			    ret =  hx83112a_nf_resetgpio_set(chip_info->hw_res, false); // reset gpio
			    if (ret < 0) {
			        TPD_INFO("%s: hx83112a_nf reset gpio failed.\n", __func__);
			        return ret;
			    }
			}*/
			/*himax_sense_off(chip_info);*/
			himax_ultra_enter(chip_info);
		}
	} else {
		himax_sense_on(chip_info, 0);
	}

	return ret;
}
/*
static int hx83112a_nf_enable_edge_limit(struct chip_data_hx83112a_nf *chip_info, bool enable)
{
    int ret = 0;
    return ret;
}
*/

static int hx83112a_nf_enable_charge_mode(
				struct chip_data_hx83112a_nf  *chip_info, bool enable)
{
	int ret = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	TPD_INFO("%s, charge mode enable = %d\n", __func__, enable);

	/*Enable:0x10007F38 = 0xA55AA55A  */
	if (enable) {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x38;
		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

	} else {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x38;
		tmp_data[3] = 0x77;
		tmp_data[2] = 0x88;
		tmp_data[1] = 0x77;
		tmp_data[0] = 0x88;
		himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
	}

	return ret;
}

/*on = 1:on   0:off */
static int hx83112a_nf_jitter_switch(
				struct chip_data_hx83112a_nf *chip_info, bool on)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int rtimes = 0;
	int ret = 0;

	TPD_INFO("%s:entering\n", __func__);

	if (!on) { /*jitter off*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:retry over 10, jitter off failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x5A, 0xA5, 0x5A, 0xA5\n",
					 __func__);
				ret = -1;
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0xE0;
			tmp_data[3] = 0xA5;
			tmp_data[2] = 0x5A;
			tmp_data[1] = 0xA5;
			tmp_data[0] = 0x5A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

			TPD_INFO("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
				 __func__,
				 rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A
				|| tmp_data[1] != 0xA5 || tmp_data[0] != 0x5A);

		TPD_INFO("%s:jitter off success!\n", __func__);

	} else { /*jitter on*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:retry over 10, jitter on failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x00, 0x00, 0x00, 0x00\n",
					 __func__);
				ret = -1;
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0xE0;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

			TPD_INFO("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
				 __func__,
				 rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] == 0xA5 && tmp_data[2] == 0x5A
				&& tmp_data[1] == 0xA5 && tmp_data[0] == 0x5A);

		TPD_INFO("%s:jitter on success!\n", __func__);
	}

	TPD_INFO("%s:END\n", __func__);
	return ret;
}

static int hx83112a_nf_enable_headset_mode(struct chip_data_hx83112a_nf *chip_info,
	                                                    bool enable)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int rtimes = 0;
	int ret = 0;
	struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);

	if (ts->headset_pump_support) {
		if (enable) { /* insert headset */
			do {
				if (rtimes > 10) {
					TPD_INFO("%s:insert headset failed!\n", __func__);
					TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x5A, 0xA5, 0x5A, 0xA5\n",
						 __func__);
					ret = -1;
					break;
				}

				tmp_addr[3] = 0x10;
				tmp_addr[2] = 0x00;
				tmp_addr[1] = 0x7F;
				tmp_addr[0] = 0xE8;
				tmp_data[3] = 0xA5;
				tmp_data[2] = 0x5A;
				tmp_data[1] = 0xA5;
				tmp_data[0] = 0x5A;
				himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

				himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
				TPD_DETAIL("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
					   __func__,
					   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
				rtimes++;
			} while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A
					|| tmp_data[1] != 0xA5 || tmp_data[0] != 0x5A);

			TPD_INFO("%s:insert headset success!\n", __func__);

		} else { /* remove headset  */
			do {
				if (rtimes > 10) {
					TPD_INFO("%s:remove headset failed!\n", __func__);
					TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x5A, 0xA5, 0x5A, 0xA5\n",
						 __func__);
					ret = -1;
					break;
				}

				tmp_addr[3] = 0x10;
				tmp_addr[2] = 0x00;
				tmp_addr[1] = 0x7F;
				tmp_addr[0] = 0xE8;
				tmp_data[3] = 0x00;
				tmp_data[2] = 0x00;
				tmp_data[1] = 0x00;
				tmp_data[0] = 0x00;
				himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

				himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
				TPD_DETAIL("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
					   __func__,
					   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
				rtimes++;
			} while (tmp_data[3] != 0x00 || tmp_data[2] != 0x00
					|| tmp_data[1] != 0x00 || tmp_data[0] != 0x00);

			TPD_INFO("%s:remove headset success!\n", __func__);
		}
	}

	return ret;
}

/*mode = 0:off   1:normal   2:turn right    3:turn left*/
static int hx83112a_nf_rotative_switch(struct chip_data_hx83112a_nf *chip_info,
				       int mode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int rtimes = 0;
	int ret = 0;
	/*struct touchpanel_data *ts = spi_get_drvdata(chip_info->hx_spi);*/

	TPD_DETAIL("%s:entering\n", __func__);

	if (mode == VERTICAL_SCREEN) { /* vertical */
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:rotative normal failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x5A, 0xA5, 0x5A, 0xA5\n",
					 __func__);
				ret = -1;
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0x3C;
			tmp_data[3] = 0xA5;
			tmp_data[2] = 0x5A;
			tmp_data[1] = 0xA5;
			tmp_data[0] = 0x5A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
			TPD_DETAIL("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
				   __func__,
				   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A
				|| tmp_data[1] != 0xA5 || tmp_data[0] != 0x5A);

		TPD_INFO("%s:rotative normal success!\n", __func__);

	} else if (mode == LANDSCAPE_SCREEN_270) { /*turn right*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:rotative right failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x3A, 0xA3, 0x3A, 0xA3\n",
					 __func__);
				ret = -1;
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0x3C;
			tmp_data[3] = 0xA3;
			tmp_data[2] = 0x3A;
			tmp_data[1] = 0xA3;
			tmp_data[0] = 0x3A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

			TPD_DETAIL("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
				   __func__,
				   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA3 || tmp_data[2] != 0x3A
				|| tmp_data[1] != 0xA3 || tmp_data[0] != 0x3A);

		TPD_INFO("%s:rotative right success!\n", __func__);

	} else if (mode == LANDSCAPE_SCREEN_90) { /*turn left*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:rotative left failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0, 1, 2, 3] = 0x1A, 0xA1, 0x1A, 0xA1\n",
					 __func__);
				ret = -1;
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0x3C;
			tmp_data[3] = 0xA1;
			tmp_data[2] = 0x1A;
			tmp_data[1] = 0xA1;
			tmp_data[0] = 0x1A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);

			TPD_DETAIL("%s:retry times %d, current tmp_data[0, 1, 2, 3] = 0x%2.2X, 0x%2.2X, 0x%2.2X, 0x%2.2X\n",
				   __func__,
				   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA1 || tmp_data[2] != 0x1A
				|| tmp_data[1] != 0xA1 || tmp_data[0] != 0x1A);

		TPD_INFO("%s:rotative left success!\n", __func__);
	}

	TPD_DETAIL("%s:END\n", __func__);
	return ret;
}

static int hx83112a_nf_mode_switch(void *chip_data, work_mode mode, int flag)
{
	int ret = -1;
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	switch (mode) {
	case MODE_NORMAL:
		ret = hx83112a_nf_configuration_init(chip_info, true);

		if (ret < 0) {
			TPD_INFO("%s: hx83112a_nf configuration init failed.\n", __func__);
			return ret;
		}

		break;

	case MODE_SLEEP:
		/*device control: sleep mode*/
		ret = hx83112a_nf_configuration_init(chip_info, false);

		if (ret < 0) {
			TPD_INFO("%s: hx83112a_nf configuration init failed.\n", __func__);
			return ret;
		}

		break;

	case MODE_GESTURE:
		ret = hx83112a_nf_enable_black_gesture(chip_info, flag);

		if (ret < 0) {
			TPD_INFO("%s: hx83112a_nf enable gesture failed.\n", __func__);
			return ret;
		}

		break;

	case MODE_EDGE:
		/*ret = hx83112a_nf_enable_edge_limit(chip_info, flag);*/
		ret = hx83112a_nf_rotative_switch(chip_info, flag);

		if (ret < 0) {
			TPD_INFO("%s: hx83112a_nf enable edg & corner limit failed.\n", __func__);
			return ret;
		}

		break;

	case MODE_CHARGE:
		ret = hx83112a_nf_enable_charge_mode(chip_info, flag);

		if (ret < 0) {
			TPD_INFO("%s: enable charge mode : %d failed\n", __func__, flag);
		}

		break;

	case MODE_HEADSET:
		ret = hx83112a_nf_enable_headset_mode(chip_info, flag);

		if (ret < 0) {
			TPD_INFO("%s: enable headset mode : %d failed\n", __func__, flag);
		}

		break;

	case MODE_GAME:
		ret = hx83112a_nf_jitter_switch(chip_info, !flag);

		if (ret < 0) {
			TPD_INFO("%s: enable game mode : %d failed\n", __func__, !flag);
		}

		break;

	default:
		TPD_INFO("%s: Wrong mode.\n", __func__);
	}

	return ret;
}

void himax_set_SMWP_enable(struct chip_data_hx83112a_nf *chip_info,
			   uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t back_data[4];
	uint8_t retry_cnt = 0;

	himax_sense_off(chip_info);

	/* Enable:0x10007F10 = 0xA55AA55A */
	do {
		if (SMWP_enable) {
			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0x10;
			tmp_data[3] = 0xA5;
			tmp_data[2] = 0x5A;
			tmp_data[1] = 0xA5;
			tmp_data[0] = 0x5A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			back_data[3] = 0XA5;
			back_data[2] = 0X5A;
			back_data[1] = 0XA5;
			back_data[0] = 0X5A;

		} else {
			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0x10;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x00;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);
			back_data[3] = 0X00;
			back_data[2] = 0X00;
			back_data[1] = 0X00;
			back_data[0] = 0x00;
		}

		himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
		TPD_INFO("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d \n", __func__,
			 tmp_data[0], SMWP_enable, retry_cnt);
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2]
			|| tmp_data[1] != back_data[1] || tmp_data[0] != back_data[0])
			&& retry_cnt < 10);

	himax_sense_on(chip_info, 0);
}


static int hx83112a_nf_get_gesture_info(void *chip_data,
					struct gesture_info *gesture)
{
	int i = 0;
	int gesture_sign = 0;
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	uint8_t *buf;
	int gest_len;
	int check_fc = 0;

	int check_sum_cal;
	int ts_status = HX_REPORT_SMWP_EVENT;

	/*TPD_DEBUG("%s:%d\n", __func__, __LINE__);*/

	buf = tp_devm_kzalloc(&chip_info->hx_spi->dev,
			      chip_info->hx_touch_data->event_size * sizeof(uint8_t), GFP_KERNEL);

	if (!buf) {
		TPD_INFO("%s:%d kzalloc buf error\n", __func__, __LINE__);
		return -1;
	}

	himax_burst_enable(chip_info, 0);

	if (!himax_read_event_stack(chip_info, buf,
				    chip_info->hx_touch_data->event_size)) {
		TPD_INFO("%s: can't read data from chip in gesture!\n", __func__);
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)buf,
			      chip_info->hx_touch_data->event_size * sizeof(uint8_t));
		return -1;
	}

	for (i = 0; i < 128; i++) {
		if (!i) {
			printk("%s: gesture buf data\n", __func__);
		}

		printk("%02d ", buf[i]);

		if ((i + 1) % 8 == 0) {
			printk("\n");
		}

		if (i == (128 - 1)) {
			printk("\n");
		}
	}

	check_sum_cal = himax_checksum_cal(chip_info, buf, ts_status);

	if (check_sum_cal == CHECKSUM_FAIL) {
		return -1;

	} else if (check_sum_cal == ERR_WORK_OUT) {
		goto err_workqueue_out;
	}

	for (i = 0; i < 4; i++) {
		if (check_fc == 0) {
			if ((buf[0] != 0x00) && ((buf[0] < 0x0E))) {
				check_fc = 1;
				gesture_sign = buf[i];

			} else {
				check_fc = 0;
				/*TPD_DEBUG("ID START at %x , value = %x skip the event\n", i, buf[i]);*/
				break;
			}

		} else {
			if (buf[i] != gesture_sign) {
				check_fc = 0;
				/*TPD_DEBUG("ID NOT the same %x != %x So STOP parse event\n", buf[i], gesture_sign);*/
				break;
			}
		}

		/*TPD_DEBUG("0x%2.2X ", buf[i]);*/
	}

	/*TPD_DEBUG("Himax gesture_sign= %x\n",gesture_sign );*/
	/*TPD_DEBUG("Himax check_fc is %d\n", check_fc);*/

	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1 ||
			buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2) {
		goto RET_OUT;
	}

	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1 &&
			buf[GEST_PTLG_ID_LEN + 1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];

		if (gest_len > 52) {
			gest_len = 52;
		}


		i = 0;
		chip_info->gest_pt_cnt = 0;
		/*TPD_DEBUG("gest doornidate start  %s\n",__func__);*/
#ifdef CONFIG_OPLUS_TP_APK

		if (chip_info->check_point_format == 0) {
#endif

			while (i < (gest_len + 1) / 2) {
				if (i == 6) {
					chip_info->gest_pt_x[chip_info->gest_pt_cnt] = buf[GEST_PTLG_ID_LEN + 4 + i *
							2];

				} else {
					chip_info->gest_pt_x[chip_info->gest_pt_cnt] = buf[GEST_PTLG_ID_LEN + 4 + i * 2]
							* private_ts->resolution_info.max_x / 255;
				}

				chip_info->gest_pt_y[chip_info->gest_pt_cnt] = buf[GEST_PTLG_ID_LEN + 4 + i * 2
						+ 1] * private_ts->resolution_info.max_y / 255;
				i++;
				/*TPD_DEBUG("chip_info->gest_pt_x[%d]=%d \n",chip_info->gest_pt_cnt,chip_info->gest_pt_x[chip_info->gest_pt_cnt]);*/
				/*TPD_DEBUG("chip_info->gest_pt_y[%d]=%d \n",chip_info->gest_pt_cnt,chip_info->gest_pt_y[chip_info->gest_pt_cnt]);*/
				chip_info->gest_pt_cnt += 1;
			}

#ifdef CONFIG_OPLUS_TP_APK

		} else {
			int j = 0;
			int nn;
			int n = 24;
			int m = 26;
			int pt_num;
			chip_info->gest_pt_cnt = 40;

			if (private_ts->gesture_buf) {
				pt_num = gest_len + buf[126];

				if (pt_num > 104) {
					pt_num = 104;
				}

				private_ts->gesture_buf[0] = gesture_sign;
				private_ts->gesture_buf[1] = buf[127];

				if (private_ts->gesture_buf[0] == 0x07) {
					for (j = 0; j < gest_len * 2; j = j + 2) {
						private_ts->gesture_buf[3 + j] = buf[n];
						private_ts->gesture_buf[3 + j + 1] = buf[n + 1];
						n = n + 4;
					}

					for (nn = 0; nn < (pt_num - gest_len)   * 2; nn = nn + 2) {
						private_ts->gesture_buf[3 + j + nn] = buf[m];
						private_ts->gesture_buf[3 + j + nn + 1] = buf[m + 1];
						m = m + 4;
					}

					private_ts->gesture_buf[2] = pt_num;

				} else {
					private_ts->gesture_buf[2] = gest_len;
					memcpy(&private_ts->gesture_buf[3], &buf[24], 80);
				}
			}
		}

#endif

		if (chip_info->gest_pt_cnt) {
			gesture->gesture_type = gesture_sign;/* id */
			gesture->Point_start.x = chip_info->gest_pt_x[0];/* start x */
			gesture->Point_start.y = chip_info->gest_pt_y[0];/* start y */
			gesture->Point_end.x = chip_info->gest_pt_x[1];/* end x */
			gesture->Point_end.y = chip_info->gest_pt_y[1];/* end y */
			gesture->Point_1st.x = chip_info->gest_pt_x[2]; /* 1 */
			gesture->Point_1st.y = chip_info->gest_pt_y[2];
			gesture->Point_2nd.x = chip_info->gest_pt_x[3];/* 2 */
			gesture->Point_2nd.y = chip_info->gest_pt_y[3];
			gesture->Point_3rd.x = chip_info->gest_pt_x[4];/* 3 */
			gesture->Point_3rd.y = chip_info->gest_pt_y[4];
			gesture->Point_4th.x = chip_info->gest_pt_x[5];/* 4 */
			gesture->Point_4th.y = chip_info->gest_pt_y[5];
			gesture->clockwise = chip_info->gest_pt_x[6]; /*  1, 0 */
			/*TPD_DEBUG("gesture->gesture_type = %d \n", gesture->gesture_type);*/
			/*for (i = 0; i < 6; i++)
			   TPD_DEBUG("%d [ %d  %d ]\n", i, chip_info->gest_pt_x[i], chip_info->gest_pt_y[i]);*/
		}
	}

	/*TPD_DETAIL("%s, gesture_type = %d\n", __func__, gesture->gesture_type);*/

RET_OUT:

	if (buf) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)buf,
			      chip_info->hx_touch_data->event_size * sizeof(uint8_t));
	}

	return 0;

err_workqueue_out:
	/*himax_ic_reset(chip_info, false, true);*/
	return -1;
}

static int hx83112a_nf_power_control(void *chip_data, bool enable)
{
	int ret = 0;
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	if (true == enable) {
		ret = tp_powercontrol_avdd(chip_info->hw_res, true);

		if (ret) {
			return -1;
		}

		ret = tp_powercontrol_vddi(chip_info->hw_res, true);

		if (ret) {
			return -1;
		}

		ret = hx83112a_nf_resetgpio_set(chip_info->hw_res, true);

		if (ret) {
			return -1;
		}

	} else {
		ret = hx83112a_nf_resetgpio_set(chip_info->hw_res, false);

		if (ret) {
			return -1;
		}

		ret = tp_powercontrol_vddi(chip_info->hw_res, false);

		if (ret) {
			return -1;
		}

		ret = tp_powercontrol_avdd(chip_info->hw_res, false);

		if (ret) {
			return -1;
		}
	}

	return ret;
}

static int hx83112a_nf_int_pin_test(struct seq_file *s, void *chip_data,
				    struct auto_testdata *hx_testdata)
{
	int eint_status, eint_count = 0, read_gpio_num = 10;

	TPD_INFO("%s, step 0: begin to check INT-GND short item\n", __func__);

	while (read_gpio_num--) {
		msleep(5);
		eint_status = gpio_get_value(hx_testdata->irq_gpio);

		if (eint_status == 1) {
			eint_count--;

		} else {
			eint_count++;
		}

		TPD_INFO("%s eint_count = %d  eint_status = %d\n", __func__, eint_count,
			 eint_status);
	}

	TPD_INFO("TP EINT PIN direct short! eint_count = %d\n", eint_count);

	if (eint_count == 10) {
		TPD_INFO("error :  TP EINT PIN direct short!\n");
		seq_printf(s, "TP EINT direct stort\n");
		hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
						 hx_testdata->list_write_count, 45,
						 "eint_status is low, TP EINT direct stort, \n");
		/*store_to_file(hx_testdata->fd, "eint_status is low, TP EINT direct stort, \n");*/
		eint_count = 0;
		return TEST_FAIL;
	}

	return TEST_PASS;
}

static int hx83112a_nf_test_prepare(void *chip_data,
				    struct auto_testdata *hx_testdata)
{
	int ret = 0;

	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	/*init criteria data*/
	hx_testdata->tp_fw = chip_info->fw_id;
	hx_testdata->dev_tp_fw = chip_info->fw_ver;
	/*init criteria data*/

	if (hx_testdata->fw_test) {
		himax_mcu_0f_operation_test_dirly(chip_info, hx_testdata->fw_test);
		msleep(5);
		g_core_fp.fp_reload_disable(chip_info, 0);
		msleep(5);
		himax_sense_on(chip_info, 0x00);
	}

	himax_read_OPLUS_FW_ver(chip_info);

	ret = hx83112a_nf_enable_interrupt(chip_info, false);

	return 0;
}

static void hx83112a_nf_test_finish(void *chip_data)
{
#ifndef HX_ZERO_FLASH
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	himax_sense_off(chip_info);
	/*himax_set_N_frame(chip_info, 1, HIMAX_INSPECTION_NOISE);*/

	himax_reload_disable(chip_info, 0);

	himax_sense_on(chip_info, 0);
#endif

	/*himax_mcu_0f_operation_dirly();
	msleep(5);
	g_core_fp.fp_reload_disable(chip_info, 0);
	msleep(5);
	himax_sense_on(chip_info, 0x00);
	himax_read_OPLUS_FW_ver(chip_info);*/
}

static int hx83112a_nf_black_screen_test(void *chip_data, char *message,
		int msg_size, struct auto_testdata *hx_testdata)
{
	int error = 0;
	int error_num = 0;
	int retry_cnt = 3;
	int msg_count = 0;
	char buf[128] = {0};
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	TPD_INFO("%s\n", __func__);

	/*6. LPWUG RAWDATA*/
	TPD_INFO("[MP_LPWUG_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_RAWDATA,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 42, "6. MP_LPWUG_TEST_RAW: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 15, "test Item:\n");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 42, "6. MP_LPWUG_TEST_RAW: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);

	if (msg_size > 42) {
		msg_count = snprintf(message, 42, "%s", buf);
	}

	if (error != 0) {
		error_num++;
	}

	/*7. LPWUG NOISE*/
	retry_cnt = 3;
	TPD_INFO("[MP_LPWUG_TEST_NOISE]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_NOISE,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "7. MP_LPWUG_TEST_NOISE: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 42, "7. MP_LPWUG_TEST_NOISE: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);

	if (msg_size > msg_count + 42) {
		msg_count += snprintf(message + msg_count, 42, "%s", buf);
	}

	if (error != 0) {
		error_num++;
	}

	/*8. LPWUG IDLE RAWDATA*/
	retry_cnt = 3;
	TPD_INFO("[MP_LPWUG_IDLE_TEST_RAW]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "8. MP_LPWUG_IDLE_TEST_RAW: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 42, "8. MP_LPWUG_IDLE_TEST_RAW: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);

	if (msg_size > msg_count + 42) {
		msg_count += snprintf(message + msg_count, 42, "%s", buf);
	}

	if (error != 0) {
		error_num++;
	}

	/*9. LPWUG IDLE RAWDATA*/
	retry_cnt = 3;
	TPD_INFO("[MP_LPWUG_IDLE_TEST_NOISE]\n");

	do {
		error = mpTestFunc(chip_info, HIMAX_INSPECTION_LPWUG_IDLE_NOISE,
				   (chip_info->hw_res->tx_num * chip_info->hw_res->rx_num) +
				   chip_info->hw_res->tx_num + chip_info->hw_res->rx_num, hx_testdata);
		retry_cnt--;
	} while ((error == RESULT_RETRY) && (retry_cnt > 0));

	snprintf(buf, 128, "9. MP_LPWUG_IDLE_TEST_NOISE: %s\n", error ? "Error" : "Ok");
	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 42, "9. MP_LPWUG_IDLE_TEST_NOISE: %s\n",
					 error ? "NG" : "Ok");
	TPD_INFO("%s", buf);

	if (msg_size > msg_count + 42) {
		msg_count += snprintf(message + msg_count, 42, "%s", buf);
	}

	if (error != 0) {
		error_num++;
	}

	hx_testdata->list_write_count += snprintf(hx_testdata->test_list_log +
					 hx_testdata->list_write_count, 22, "Final_result: %s\n",
					 error_num ? "Fail" : "Pass");

	return error_num;
}

static void hx83112a_nf_read_debug_data(struct seq_file *s, void *chip_data,
					int debug_data_type)
{
	uint16_t mutual_num;
	uint16_t self_num;
	uint16_t width;
	int i = 0;
	int j = 0;
	int k = 0;
	int32_t *data_mutual_sram;
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};

	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	if (!chip_info) {
		return;
	}

	data_mutual_sram = tp_devm_kzalloc(&chip_info->hx_spi->dev,
					   chip_info->hw_res->tx_num * chip_info->hw_res->rx_num * sizeof(int32_t),
					   GFP_KERNEL);

	if (!data_mutual_sram) {
		goto RET_OUT;
	}

	mutual_num = chip_info->hw_res->tx_num * chip_info->hw_res->rx_num;
	self_num = chip_info->hw_res->tx_num +
		   chip_info->hw_res->rx_num; /*don't add KEY_COUNT*/
	width = chip_info->hw_res->rx_num;
	seq_printf(s, "ChannelStart (rx tx) : %4d, %4d\n\n", chip_info->hw_res->rx_num,
		   chip_info->hw_res->tx_num);

	/*start to show debug data*/
	switch (debug_data_type) {
	case DEBUG_DATA_BASELINE:
		seq_printf(s, "Baseline data: \n");
		TPD_INFO("Baseline data: \n");
		break;

	case DEBUG_DATA_RAW:
		seq_printf(s, "Raw data: \n");
		TPD_INFO("Raw data: \n");
		break;

	case DEBUG_DATA_DELTA:
		seq_printf(s, "Delta data: \n");
		TPD_INFO("Delta data: \n");
		break;

	case DEBUG_DATA_DOWN:
		seq_printf(s, "Finger down data: \n");
		TPD_INFO("Finger down data: \n");
		break;

	default :
		seq_printf(s, "No this debug datatype \n");
		TPD_INFO("No this debug datatype \n");
		goto RET_OUT;
		break;
	}

	if (debug_data_type == DEBUG_DATA_DOWN) {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xF8;

		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		himax_diag_register_set(chip_info, DEBUG_DATA_DELTA);

	} else {
		himax_diag_register_set(chip_info, debug_data_type);
	}

	TPD_INFO("%s: Start get debug data in DSRAM\n", __func__);
	chip_info->dsram_flag = true;

	himax_ts_diag_func(chip_info, data_mutual_sram);

	for (j = 0; j < chip_info->hw_res->rx_num; j++) {
		for (i = 0; i < chip_info->hw_res->tx_num; i++) {
			k = ((mutual_num - j) - chip_info->hw_res->rx_num * i) - 1;
			seq_printf(s, "%6d", data_mutual_sram[k]);
		}

		seq_printf(s, " %6d\n", chip_info->diag_self[j]);
	}

	seq_printf(s, "\n");

	for (i = 0; i < chip_info->hw_res->tx_num; i++) {
		seq_printf(s, "%6d", chip_info->diag_self[i]);
	}

	/*Clear DSRAM flag*/
	himax_diag_register_set(chip_info, 0x00);
	chip_info->dsram_flag = false;
	himax_return_event_stack(chip_info);

	seq_printf(s, "\n");
	seq_printf(s, "ChannelEnd");
	seq_printf(s, "\n");

	TPD_INFO("%s, here:%d\n", __func__, __LINE__);

	if (debug_data_type == DEBUG_DATA_DOWN) {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xF8;

		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
	}

RET_OUT:

	if (data_mutual_sram) {
		tp_devm_kfree(&chip_info->hx_spi->dev, (void **)data_mutual_sram,
			      chip_info->hw_res->tx_num * chip_info->hw_res->rx_num * sizeof(int32_t));
	}

	return;
}

static void hx83112a_nf_baseline_read(struct seq_file *s, void *chip_data)
{
	hx83112a_nf_read_debug_data(s, chip_data, DEBUG_DATA_BASELINE);
	hx83112a_nf_read_debug_data(s, chip_data, DEBUG_DATA_RAW);
	return;
}

static void hx83112a_nf_delta_read(struct seq_file *s, void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	hx83112a_nf_read_debug_data(s, chip_data, DEBUG_DATA_DELTA);
#ifdef CONFIG_OPLUS_TP_APK

	if (chip_info->debug_mode_sta) {
		hx83112a_nf_read_debug_data(s, chip_data, DEBUG_DATA_DOWN);
	}

#endif /* end of CONFIG_OPLUS_TP_APK*/
	return;
}

static void hx83112a_nf_main_register_read(struct seq_file *s, void *chip_data)
{
	return;
}

/*Reserved node*/
static void hx83112a_nf_reserve_read(struct seq_file *s, void *chip_data)
{
	return;
}

static fw_update_state hx83112a_nf_fw_update(void *chip_data,
		const struct firmware *fw, bool force)
{
	uint32_t CURRENT_FIRMWARE_ID = 0, FIRMWARE_ID = 0;
	uint8_t cmd[4];
	uint8_t data[64];
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	const uint8_t *p_fw_id = NULL;

	if (fw) {
		if (chip_info->g_fw_buf) {
			chip_info->g_fw_len = fw->size;
			memcpy(chip_info->g_fw_buf, fw->data, fw->size);
			chip_info->g_fw_sta = true;
		}
	}

	if (fw == NULL) {
		TPD_INFO("fw is NULL\n");
		return FW_NO_NEED_UPDATE;
	}

	p_fw_id = fw->data + 49172;

	if (!chip_info) {
		TPD_INFO("Chip info is NULL\n");
		return 0;
	}

	TPD_INFO("%s is called\n", __func__);

	/*step 1:fill Fw related header, get all data.*/


	/*step 2:Get FW version from IC && determine whether we need get into update flow.*/

	CURRENT_FIRMWARE_ID = (*p_fw_id << 24) | (*(p_fw_id + 1) << 16) | (*
			      (p_fw_id + 2) << 8) | *(p_fw_id + 3);


	cmd[3] = 0x10;  /* oplus fw id bin address : 0xc014   , 49172    Tp ic address : 0x 10007014*/
	cmd[2] = 0x00;
	cmd[1] = 0x70;
	cmd[0] = 0x14;
	himax_register_read(chip_info, cmd, 4, data, false);
	FIRMWARE_ID = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	TPD_INFO("CURRENT TP FIRMWARE ID is 0x%x, FIRMWARE IMAGE ID is 0x%x\n",
		 CURRENT_FIRMWARE_ID, FIRMWARE_ID);

	/*disable_irq_nosync(chip_info->hx_irq);*/

	/*step 3:Get into program mode*/
	/********************get into prog end************/
	/*step 4:flash firmware zone*/
	TPD_INFO("update-----------------firmware ------------------update!\n");
	/* fts_ctpm_fw_upgrade_with_sys_fs_64k(chip_info, (unsigned char *)fw->data, fw->size, false);*/
	g_core_fp.fp_firmware_update_0f(chip_info, fw);
	g_core_fp.fp_reload_disable(chip_info, 0);
	msleep(10);

	TPD_INFO("Firmware && configuration flash over\n");
	himax_read_OPLUS_FW_ver(chip_info);
	himax_sense_on(chip_info, 0x00);
	msleep(10);

	enable_irq(chip_info->hx_irq);
	/*   hx83112a_nf_reset(chip_info);*/
	/*   msleep(200);*/

	chip_info->first_download_finished = true;
	return FW_UPDATE_SUCCESS;
}

static int hx83112a_nf_reset_gpio_control(void *chip_data, bool enable)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;

	if (gpio_is_valid(chip_info->hw_res->reset_gpio)) {
		TPD_INFO("%s: set reset state %d\n", __func__, enable);
		hx83112a_nf_resetgpio_set(chip_info->hw_res, enable);
		TPD_DETAIL("%s: set reset state END\n", __func__);
	}

	return 0;
}

int hx83112a_nf_freq_point = 0;
void hx83112a_nf_freq_hop_trigger(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info = (struct chip_data_hx83112a_nf *)
			chip_data;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	int rtimes = 0;

	TPD_INFO("send cmd to tigger frequency hopping here!!!\n");
	hx83112a_nf_freq_point = 1 - hx83112a_nf_freq_point;

	if (hx83112a_nf_freq_point) { /*hop to frequency 130K*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:frequency hopping failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x5A,0xA5,0x5A,0xA5\n", __func__);
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0xC4;
			tmp_data[3] = 0xA5;
			tmp_data[2] = 0x5A;
			tmp_data[1] = 0xA5;
			tmp_data[0] = 0x5A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
			TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
				   __func__,
				   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA5 || tmp_data[2] != 0x5A
				|| tmp_data[1] != 0xA5 || tmp_data[0] != 0x5A);

		if (rtimes <= 10) {
			TPD_INFO("%s:hopping frequency to 130K success!\n", __func__);
		}

	} else { /*hop to frequency 75K*/
		do {
			if (rtimes > 10) {
				TPD_INFO("%s:frequency hopping failed!\n", __func__);
				TPD_INFO("%s:correct tmp_data[0,1,2,3] = 0x3A,0xA3,0x3A,0xA3\n", __func__);
				break;
			}

			tmp_addr[3] = 0x10;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x7F;
			tmp_addr[0] = 0xC4;
			tmp_data[3] = 0xA3;
			tmp_data[2] = 0x3A;
			tmp_data[1] = 0xA3;
			tmp_data[0] = 0x3A;
			himax_flash_write_burst(chip_info, tmp_addr, tmp_data);

			himax_register_read(chip_info, tmp_addr, 4, tmp_data, false);
			TPD_DETAIL("%s:retry times %d, current tmp_data[0,1,2,3] = 0x%2.2X,0x%2.2X,0x%2.2X,0x%2.2X\n",
				   __func__,
				   rtimes, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
			rtimes++;
		} while (tmp_data[3] != 0xA3 || tmp_data[2] != 0x3A
				|| tmp_data[1] != 0xA3 || tmp_data[0] != 0x3A);

		if (rtimes <= 10) {
			TPD_INFO("%s:hopping frequency to 75K success!\n", __func__);
		}
	}
}

static struct oplus_touchpanel_operations hx83112a_nf_ops = {
	.ftm_process      = hx83112a_nf_ftm_process,
	.get_vendor       = hx83112a_nf_get_vendor,
	.get_chip_info    = hx83112a_nf_get_chip_info,
	.reset            = hx83112a_nf_reset,
	.power_control    = hx83112a_nf_power_control,
	.fw_check         = hx83112a_nf_fw_check,
	.fw_update        = hx83112a_nf_fw_update,
	.trigger_reason   = hx83112a_nf_trigger_reason,
	.get_touch_points = hx83112a_nf_get_touch_points,
	.get_gesture_info = hx83112a_nf_get_gesture_info,
	.mode_switch      = hx83112a_nf_mode_switch,
	.exit_esd_mode    = hx83112a_nf_exit_esd_mode,
	/*.resume_prepare = hx83112a_nf_resume_prepare,*/
	.reset_gpio_control = hx83112a_nf_reset_gpio_control,
	.freq_hop_trigger = hx83112a_nf_freq_hop_trigger,
};

static struct himax_proc_operations hx83112a_nf_proc_ops = {
	.test_prepare = hx83112a_nf_test_prepare,
	.test_finish = hx83112a_nf_test_finish,
	.int_pin_test = hx83112a_nf_int_pin_test,
	.self_test = himax_chip_self_test,
	.blackscreen_test = hx83112a_nf_black_screen_test,
	.himax_proc_register_write =  hx83112a_nf_proc_register_write,
	.himax_proc_register_read =  hx83112a_nf_proc_register_read,
	.himax_proc_diag_write =  hx83112a_nf_proc_diag_write,
	.himax_proc_diag_read =  hx83112a_nf_proc_diag_read,
	.himax_proc_DD_debug_read =  hx83112a_nf_proc_DD_debug_read,
	.himax_proc_DD_debug_write =  hx83112a_nf_proc_DD_debug_write,
	.himax_proc_FW_debug_read =  hx83112a_nf_proc_FW_debug_read,
	.himax_proc_reset_write =  hx83112a_nf_proc_reset_write,
	.himax_proc_sense_on_off_write =  hx83112a_nf_proc_sense_on_off_write,
#ifdef HX_ENTER_ALGORITHM_NUMBER
	/*.himax_proc_enter_algorithm_switch_write = himax_enter_algorithm_number_write,*/
	/*.himax_proc_enter_algorithm_switch_read  = himax_enter_algorithm_number_read,*/
#endif
};

static struct debug_info_proc_operations debug_info_proc_ops = {
	.delta_read    = hx83112a_nf_delta_read,
	.baseline_read = hx83112a_nf_baseline_read,
	.main_register_read = hx83112a_nf_main_register_read,
	.reserve_read = hx83112a_nf_reserve_read,
};

struct engineer_test_operations hx_engineer_test_ops = {
	.auto_test                  = hx_auto_test,
	.black_screen_test          = hx_black_screen_test,
};

#ifdef CONFIG_OPLUS_TP_APK

static void himax_enter_hopping_write(struct chip_data_hx83112a_nf *chip_info,
				      bool on_off)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};


	if (on_off) {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xF8;

		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xC4;

		tmp_data[3] = 0xA1;
		tmp_data[2] = 0x1A;
		tmp_data[1] = 0xA1;
		tmp_data[0] = 0x1A;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);
		TPD_INFO("%s: open himax enter hopping write.\n", __func__);

	} else {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xF8;

		tmp_data[3] = 0;
		tmp_data[2] = 0;
		tmp_data[1] = 0;
		tmp_data[0] = 0;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);

		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xC4;

		tmp_data[3] = 0;
		tmp_data[2] = 0;
		tmp_data[1] = 0;
		tmp_data[0] = 0;
		himax_register_write(chip_info, tmp_addr, 4, tmp_data, 0);

		TPD_INFO("%s: close himax hopping write.\n", __func__);
	}
}


static void himax_apk_game_set(void *chip_data, bool on_off)
{
	hx83112a_nf_mode_switch(chip_data, MODE_GAME, on_off);
}

static bool himax_apk_game_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	return chip_info->lock_point_status;
}

static void himax_apk_debug_set(void *chip_data, bool on_off)
{
	/*u8 cmd[1];*/
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;

	himax_debug_mode_set(chip_info, on_off);
	chip_info->debug_mode_sta = on_off;
}

static bool himax_apk_debug_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;

	return chip_info->debug_mode_sta;
}

static void himax_apk_gesture_debug(void *chip_data, bool on_off)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	/*get_gesture_fail_reason(on_off);*/
	chip_info->debug_gesture_sta = on_off;
}

static bool  himax_apk_gesture_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	return chip_info->debug_gesture_sta;
}

static int  himax_apk_gesture_info(void *chip_data, char *buf, int len)
{
	int ret = 0;
	int i;
	int num;
	u8 temp;
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;

	if (len < 2) {
		return 0;
	}

	buf[0] = 255;

	temp = private_ts->gesture_buf[0];

	if (temp == 0x00) {
		temp = private_ts->gesture_buf[1] | 0x80;
	}

	buf[0] = temp;

	/*buf[0] = gesture_buf[0];*/
	num = private_ts->gesture_buf[2];

	if (num > 40) {
		num = 40;
	}

	ret = 2;

	buf[1] = num;

	/*print all data*/
	for (i = 0; i < num; i++) {
		int x;
		int y;
		x = private_ts->gesture_buf[i * 2 + 3];
		x = x * private_ts->resolution_info.max_x / 255;

		y = private_ts->gesture_buf[i * 2 + 4];
		y = y * private_ts->resolution_info.max_y / 255;


		/*TPD_INFO("nova_apk_gesture_info:gesture x is %d,y is %d.\n", x, y);*/

		if (len < i * 4 + 2) {
			break;
		}

		buf[i * 4 + 2] = x & 0xFF;
		buf[i * 4 + 3] = (x >> 8) & 0xFF;
		buf[i * 4 + 4] = y & 0xFF;
		buf[i * 4 + 5] = (y >> 8) & 0xFF;
		ret += 4;
	}

	return ret;
}


static void himax_apk_earphone_set(void *chip_data, bool on_off)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	hx83112a_nf_mode_switch(chip_data, MODE_HEADSET, on_off);
	chip_info->earphone_sta = on_off;
}

static bool himax_apk_earphone_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	return chip_info->earphone_sta;
}

static void himax_apk_charger_set(void *chip_data, bool on_off)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	hx83112a_nf_mode_switch(chip_data, MODE_CHARGE, on_off);
	chip_info->plug_status = on_off;
}

static bool himax_apk_charger_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;

	return chip_info->plug_status;
}

static void himax_apk_noise_set(void *chip_data, bool on_off)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	himax_enter_hopping_write(chip_info, on_off);
	chip_info->noise_sta = on_off;
}

static bool himax_apk_noise_get(void *chip_data)
{
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;

	return chip_info->noise_sta;
}


static int  himax_apk_tp_info_get(void *chip_data, char *buf, int len)
{
	int ret;
	struct chip_data_hx83112a_nf *chip_info;
	chip_info = (struct chip_data_hx83112a_nf *)chip_data;
	ret = snprintf(buf, len, "IC:HIMAX%06X\nFW_VER:0x%04X\nCH:%dX%d\n",
		       0x83112A,
		       chip_info->fw_ver,
		       chip_info->hw_res->tx_num,
		       chip_info->hw_res->rx_num);

	if (ret > len) {
		ret = len;
	}

	return ret;
}

static void himax_init_oplus_apk_op(struct touchpanel_data *ts)
{
	ts->apk_op = kzalloc(sizeof(APK_OPERATION), GFP_KERNEL);

	if (ts->apk_op) {
		ts->apk_op->apk_game_set = himax_apk_game_set;
		ts->apk_op->apk_game_get = himax_apk_game_get;
		ts->apk_op->apk_debug_set = himax_apk_debug_set;
		ts->apk_op->apk_debug_get = himax_apk_debug_get;
		/*apk_op->apk_proximity_set = ili_apk_proximity_set;*/
		/*apk_op->apk_proximity_dis = ili_apk_proximity_dis;*/
		ts->apk_op->apk_noise_set = himax_apk_noise_set;
		ts->apk_op->apk_noise_get = himax_apk_noise_get;
		ts->apk_op->apk_gesture_debug = himax_apk_gesture_debug;
		ts->apk_op->apk_gesture_get = himax_apk_gesture_get;
		ts->apk_op->apk_gesture_info = himax_apk_gesture_info;
		ts->apk_op->apk_earphone_set = himax_apk_earphone_set;
		ts->apk_op->apk_earphone_get = himax_apk_earphone_get;
		ts->apk_op->apk_charger_set = himax_apk_charger_set;
		ts->apk_op->apk_charger_get = himax_apk_charger_get;
		ts->apk_op->apk_tp_info_get = himax_apk_tp_info_get;
		/*apk_op->apk_data_type_set = ili_apk_data_type_set;*/
		/*apk_op->apk_rawdata_get = ili_apk_rawdata_get;*/
		/*apk_op->apk_diffdata_get = ili_apk_diffdata_get;*/
		/*apk_op->apk_basedata_get = ili_apk_basedata_get;*/
		/*ts->apk_op->apk_backdata_get = nova_apk_backdata_get;*/
		/*apk_op->apk_debug_info = ili_apk_debug_info;*/

	} else {
		TPD_INFO("Can not kzalloc apk op.\n");
	}
}
#endif /* end of CONFIG_OPLUS_TP_APK*/


static int hx83112a_nf_tp_probe(struct spi_device *spi)
{
	struct chip_data_hx83112a_nf *chip_info = NULL;
	struct touchpanel_data *ts = NULL;
	int ret = -1;
	u64 time_counter = 0;

	TPD_INFO("%s  is called\n", __func__);
	reset_healthinfo_time_counter(&time_counter);

	/*step1:Alloc chip_info*/
	chip_info = kzalloc(sizeof(struct chip_data_hx83112a_nf), GFP_KERNEL);

	if (chip_info == NULL) {
		TPD_INFO("chip info kzalloc error\n");
		ret = -ENOMEM;
		return ret;
	}

	memset(chip_info, 0, sizeof(*chip_info));

	/* allocate himax report data */
	chip_info->hx_touch_data = kzalloc(sizeof(struct himax_report_data),
					   GFP_KERNEL);

	if (chip_info->hx_touch_data == NULL) {
		goto err_register_driver;
	}

	/*step2:Alloc common ts*/
	ts = common_touch_data_alloc();

	if (ts == NULL) {
		TPD_INFO("ts kzalloc error\n");
		goto err_register_driver;
	}

	memset(ts, 0, sizeof(*ts));

	chip_info->g_fw_buf = vmalloc(128 * 1024);

	if (chip_info->g_fw_buf == NULL) {
		TPD_INFO("fw buf vmalloc error\n");
		/*ret = -ENOMEM;*/
		goto err_g_fw_buf;
	}

	/*step3:binding dev for easy operate*/
	chip_info->hx_spi = spi;
	chip_info->hx_ops = &hx83112a_nf_proc_ops;
	ts->debug_info_ops = &debug_info_proc_ops;
	ts->s_client = spi;
	chip_info->hx_irq = spi->irq;
	ts->irq = spi->irq;
	spi_set_drvdata(spi, ts);
	ts->dev = &spi->dev;
	ts->chip_data = chip_info;
	chip_info->hw_res = &ts->hw_res;
	mutex_init(&(chip_info->spi_lock));
	chip_info->touch_direction = VERTICAL_SCREEN;
	chip_info->using_headfile = false;
	chip_info->first_download_finished = false;
	chip_info->cfg_crc = -1;
	chip_info->max_mutual = 0;
	chip_info->min_mutual = 255;
	chip_info->max_self = 0;
	chip_info->min_self = 255;

	if (ts->s_client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		TPD_INFO("Full duplex not supported by master\n");
		ret = -EIO;
		goto err_spi_setup;
	}

	ts->s_client->bits_per_word = 8;
	ts->s_client->mode = SPI_MODE_3;
	ts->s_client->chip_select = 0;

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
	/* new usage of MTK spi API */
	memcpy(&chip_info->hx_spi_mcc, &hx_spi_ctrdata, sizeof(struct mtk_chip_config));
	ts->s_client->controller_data = (void *)&chip_info->hx_spi_mcc;
#else
	/* old usage of MTK spi API */
	memcpy(&chip_info->hx_spi_mcc, &hx_spi_ctrdata, sizeof(struct mt_chip_conf));
	ts->s_client->controller_data = (void *)&chip_info->hx_spi_mcc;

	ret = spi_setup(ts->s_client);

	if (ret < 0) {
		TPD_INFO("Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#endif
	/*disable_irq_nosync(chip_info->hx_irq);*/

	/*step4:file_operations callback binding*/
	ts->ts_ops = &hx83112a_nf_ops;
	ts->engineer_ops = &hx_engineer_test_ops;

	private_ts = ts;

#ifdef CONFIG_OPLUS_TP_APK
	himax_init_oplus_apk_op(ts);
#endif /* end of CONFIG_OPLUS_TP_APK*/

	/*step5:register common touch*/
	ret = register_common_touch_device(ts);

	if (ret < 0) {
		goto err_register_driver;
	}

	disable_irq_nosync(chip_info->hx_irq);

	if (himax_ic_package_check(chip_info) == false) {
		TPD_INFO("Himax chip doesn NOT EXIST");
		goto err_register_driver;
	}

	chip_info->monitor_data = &ts->monitor_data;
	chip_info->test_limit_name = ts->panel_data.test_limit_name;
#ifdef HX_ZERO_FLASH
	chip_info->p_firmware_headfile = &ts->panel_data.firmware_headfile;
	g_auto_update_flag = true;
	chip_info->himax_0f_update_wq =
		create_singlethread_workqueue("HMX_0f_update_reuqest");
	INIT_DELAYED_WORK(&chip_info->work_0f_update, himax_mcu_0f_operation);
	/*queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update, msecs_to_jiffies(2000));*/
#else
	himax_read_FW_ver(chip_data);
	himax_calculateChecksum(chip_info, false);
#endif

	himax_power_on_init(chip_info);

	/*touch data init*/
	ret = himax_report_data_init(chip_info, ts->max_num, ts->hw_res.tx_num,
				     ts->hw_res.rx_num);

	if (ret) {
		goto err_register_driver;
	}

	ts->tp_suspend_order = TP_LCD_SUSPEND;
	ts->tp_resume_order = LCD_TP_RESUME;
	ts->skip_suspend_operate = true;
	ts->skip_reset_in_resume = true;

	/*step7:create hx83112a_nf related proc files*/
	himax_create_proc(ts, chip_info->hx_ops);
	chip_info->irq_en_cnt = 1;
	TPD_INFO("%s, probe normal end\n", __func__);

#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (ts->boot_mode == RECOVERY_BOOT)
#else
	if (ts->boot_mode == MSM_BOOT_MODE__RECOVERY)
#endif
	{
		enable_irq(chip_info->hx_irq);
		TPD_INFO("In Recovery mode, no-flash download fw by headfile\n");
		queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update,
				   msecs_to_jiffies(500));

	} else if (is_oem_unlocked()) {
		enable_irq(chip_info->hx_irq);
		TPD_INFO("It's oem unlocked, download fw in probe\n");
		queue_delayed_work(chip_info->himax_0f_update_wq, &chip_info->work_0f_update,
				   msecs_to_jiffies(5000));

	} else {
		/*disable_irq_nosync(chip_info->hx_irq);*/

		/* update fw in probe*/
		if (ts->fw_update_in_probe_with_headfile) {
			const struct firmware *fw = NULL;
			himax_mcu_firmware_update_0f(chip_info, fw);
		}
	}

	if (ts->health_monitor_support) {
		tp_healthinfo_report(&ts->monitor_data, HEALTH_PROBE, &time_counter);
	}

	return 0;
err_spi_setup:

	if (chip_info->g_fw_buf) {
		vfree(chip_info->g_fw_buf);
	}

err_g_fw_buf:
err_register_driver:
	disable_irq_nosync(chip_info->hx_irq);

	common_touch_data_free(ts);
	ts = NULL;

	if (chip_info->hx_touch_data) {
		kfree(chip_info->hx_touch_data);
	}

	if (chip_info) {
		kfree(chip_info);
	}

	ret = -1;

	TPD_INFO("%s, probe error\n", __func__);

	return ret;
}

static int hx83112a_nf_tp_remove(struct spi_device *spi)
{
	struct touchpanel_data *ts = spi_get_drvdata(spi);

	ts->s_client = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);

	TPD_INFO("%s is called\n", __func__);
	kfree(ts);

	return 0;
}

static int hx83112a_nf_i2c_suspend(struct device *dev)
{
	struct touchpanel_data *ts = dev_get_drvdata(dev);

	TPD_INFO("%s: is called gesture_enable =%d\n", __func__, ts->gesture_enable);
	tp_pm_suspend(ts);

	return 0;
}

static int hx83112a_nf_i2c_resume(struct device *dev)
{
	struct touchpanel_data *ts = dev_get_drvdata(dev);

	tp_pm_resume(ts);
	TPD_INFO("%s is called\n", __func__);

	/* if (ts->black_gesture_support) {
	     if (ts->gesture_enable == 1) {
	         TPD_INFO("himax_set_SMWP_enable 1\n");
	         himax_set_SMWP_enable(1, ts->is_suspended);
	     }
	 }*/
	return 0;
}

static const struct spi_device_id tp_id[] = {
#ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
	{ "oplus,tp_noflash", 0 },
#else
	{ TPD_DEVICE, 0 },
#endif
	{ }
};

static struct of_device_id tp_match_table[] = {
#ifdef CONFIG_TOUCHPANEL_MULTI_NOFLASH
	{ .compatible = "oplus,tp_noflash", },
#else
	{ .compatible = TPD_DEVICE, },
#endif
	{ },
};

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = hx83112a_nf_i2c_suspend,
	.resume = hx83112a_nf_i2c_resume,
};


static struct spi_driver himax_common_driver = {
	.probe      = hx83112a_nf_tp_probe,
	.remove     = hx83112a_nf_tp_remove,
	.id_table   = tp_id,
	.driver = {
		.name = TPD_DEVICE,
		.owner = THIS_MODULE,
		.of_match_table = tp_match_table,
		.pm = &tp_pm_ops,
	},
};

int __init tp_driver_init_hx83112a_nf(void)
{
	int status = 0;

	TPD_INFO("%s is called\n", __func__);

	if (!tp_judge_ic_match(TPD_DEVICE)) {
		return -1;
	}

	/*get_lcd_vendor();*/
	/*get_oem_verified_boot_state();*/

	status = spi_register_driver(&himax_common_driver);

	if (status < 0) {
		TPD_INFO("%s, Failed to register SPI driver.\n", __func__);
		return -EINVAL;
	}

	return status;
}

/* should never be called */
void __exit tp_driver_exit_hx83112a_nf(void)
{
	spi_unregister_driver(&himax_common_driver);
	return;
}

#ifdef CONFIG_TOUCHPANEL_LATE_INIT
late_initcall(tp_driver_init_hx83112a_nf);
#else
module_init(tp_driver_init_hx83112a_nf);
#endif
module_exit(tp_driver_exit_hx83112a_nf);

MODULE_DESCRIPTION("Touchscreen hx83112a noflash Driver");
MODULE_LICENSE("GPL");
