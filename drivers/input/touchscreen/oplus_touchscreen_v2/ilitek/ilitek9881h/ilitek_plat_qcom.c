// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/module.h>
#include "ilitek.h"
#ifndef REMOVE_OPLUS_FUNCTION
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM
#include<mt-plat/mtk_boot_common.h>
#else
#include <soc/oplus/system/boot_mode.h>
#endif
#endif


#define DTS_OF_NAME    "tchip,ilitek"
#define DEVICE_ID        "ILITEK_TDDI"

#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
extern void disable_esd_thread(void);
#endif /* end of CONFIG_FB_MSM*/
#endif

/* Debug level */
s32 ipio_debug_level = DEBUG_OUTPUT;
/*EXPORT_SYMBOL(ipio_debug_level);*/

struct ilitek_tddi_dev *idev;

/*******Part1:spi  Function implement*******/
static int core_rx_lock_check(void *chip_data, int *ret_size)
{
	int i, count = 1;
	u8 txbuf[5] = {SPI_WRITE, 0x25, 0x94, 0x0, 0x2};
	u8 rxbuf[4] = {0};
	u16 status = 0, lock = 0x5AA5;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	for (i = 0; i < count; i++) {
		txbuf[0] = SPI_WRITE;

		if (spi_write_then_read(chip_info->spi, txbuf, 5, txbuf, 0) < 0) {
			TPD_INFO("spi write (0x25,0x94,0x0,0x2) error\n");
			goto out;
		}

		txbuf[0] = SPI_READ;

		if (spi_write_then_read(chip_info->spi, txbuf, 1, rxbuf, 4) < 0) {
			TPD_INFO("spi read error\n");
			goto out;
		}

		status = (rxbuf[2] << 8) + rxbuf[3];
		*ret_size = (rxbuf[0] << 8) + rxbuf[1];

		/*TPD_DEBUG("Rx lock = 0x%x, size = %d\n", status, *ret_size);*/

		if (status == lock) {
			return 0;
		}

		mdelay(1);
	}

out:
	TPD_INFO("Rx check lock error, lock = 0x%x, size = %d\n", status, *ret_size);
	return -EIO;
}

static int core_tx_unlock_check(void *chip_data)
{
	int i, count = 100;
	u8 txbuf[5] = {SPI_WRITE, 0x25, 0x0, 0x0, 0x2};
	u8 rxbuf[4] = {0};
	u16 status = 0, unlock = 0x9881;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	for (i = 0; i < count; i++) {
		txbuf[0] = SPI_WRITE;

		if (spi_write_then_read(chip_info->spi, txbuf, 5, txbuf, 0) < 0) {
			TPD_INFO("spi write (0x25,0x0,0x0,0x2) error\n");
			goto out;
		}

		txbuf[0] = SPI_READ;

		if (spi_write_then_read(chip_info->spi, txbuf, 1, rxbuf, 4) < 0) {
			TPD_INFO("spi read error\n");
			goto out;
		}

		status = (rxbuf[2] << 8) + rxbuf[3];

		/*TPD_DEBUG("Tx unlock = 0x%x\n", status);*/

		if (status == unlock) {
			return 0;
		}

		mdelay(1);
	}

out:
	TPD_INFO("Tx check unlock error, unlock = 0x%x\n", status);
	return -EIO;
}

static int core_spi_ice_mode_unlock_read(void *chip_data, u8 *data, int size)
{
	int ret = 0;
	u8 txbuf[64] = { 0 };
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* set read address */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x98;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	if (spi_write_then_read(chip_info->spi, txbuf, 5, txbuf, 0) < 0) {
		TPD_INFO("spi write (0x25,0x98,0x00,0x2) error\n");
		ret = -EIO;
		return ret;
	}

	/* read data */
	txbuf[0] = SPI_READ;

	if (spi_write_then_read(chip_info->spi, txbuf, 1, data, size) < 0) {
		ret = -EIO;
		return ret;
	}

	/* write data unlock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x94;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x98;
	txbuf[8] = (char)0x81;

	if (spi_write_then_read(chip_info->spi, txbuf, 9, txbuf, 0) < 0) {
		TPD_INFO("spi write unlock (0x9881) error, ret = %d\n", ret);
		ret = -EIO;
	}

	return ret;
}

static u8 ilitek_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++) {
		sum += packet[i];
	}

	return (u8)((-sum) & 0xFF);
}


static int core_spi_ice_mode_lock_write(void *chip_data, u8 *data, int size)
{
	int ret = 0;
	int safe_size = size;
	u8 check_sum = 0, wsize = 0;
	u8 *txbuf = NULL;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	int zalloc_size = 0;

	txbuf = tp_devm_kzalloc(chip_info->dev, (size + 9) * sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(txbuf)) {
		TPD_INFO("Failed to allocate txbuf\n");
		ret = -ENOMEM;
		goto out;
	}

	zalloc_size = (size + 9) * sizeof(u8);

	/* Write data */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x4;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	/* Calcuate checsum and fill it in the last byte */
	check_sum = ilitek_calc_packet_checksum(data, size);
	tp_memcpy(txbuf + 5, safe_size + 9 - 5, data, size, size);
	txbuf[5 + size] = check_sum;
	size++;
	wsize = size;

	if (wsize % 4 != 0) {
		wsize += 4 - (wsize % 4);
	}

	if (spi_write_then_read(chip_info->spi, txbuf, wsize + 5, txbuf, 0) < 0) {
		TPD_INFO("spi write (0x25,0x4,0x00,0x2) error\n");
		ret = -EIO;
		goto out;
	}

	/* write data lock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x0;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x5A;
	txbuf[8] = (char)0xA5;

	if (spi_write_then_read(chip_info->spi, txbuf, 9, txbuf, 0) < 0) {
		TPD_INFO("spi write lock (0x5AA5) error, ret = %d\n", ret);
		ret = -EIO;
	}

out:
	tp_devm_kfree(chip_info->dev, (void **)&txbuf, zalloc_size);
	return ret;
}

static int core_spi_ice_mode_disable(void *chip_data)
{
	u8 txbuf[5] = {0x82, 0x1B, 0x62, 0x10, 0x18};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (spi_write_then_read(chip_info->spi, txbuf, 5, txbuf, 0) < 0) {
		TPD_INFO("spi write ice mode disable failed\n");
		return -EIO;
	}

	return 0;
}

static int core_spi_ice_mode_enable(void *chip_data)
{
	u8 txbuf[5] = {0x82, 0x1F, 0x62, 0x10, 0x18};
	u8 rxbuf[2] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (spi_write_then_read(chip_info->spi, txbuf, 1, rxbuf, 1) < 0) {
		TPD_INFO("spi write 0x82 error\n");
		return -EIO;
	}

	/* check recover data */
	if (rxbuf[0] != SPI_ACK) {
		TPD_INFO("Check SPI_ACK failed (0x%x)\n", rxbuf[0]);
		return DO_SPI_RECOVER;
	}

	if (spi_write_then_read(chip_info->spi, txbuf, 5, rxbuf, 0) < 0) {
		TPD_INFO("spi write ice mode enable failed\n");
		return -EIO;
	}

	return 0;
}

static int core_spi_ice_mode_write(void *chip_data, u8 *data, int len)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (core_spi_ice_mode_enable(chip_info) < 0) {
		return -EIO;
	}

	/* send data and change lock status to 0x5AA5. */
	ret = core_spi_ice_mode_lock_write(chip_info, data, len);

	if (ret < 0) {
		goto out;
	}

	/*
	 * Check FW if they already received the data we sent.
	 * They change lock status from 0x5AA5 to 0x9881 if they did.
	 */
	ret = core_tx_unlock_check(chip_info);

	if (ret < 0) {
		goto out;
	}

out:

	if (core_spi_ice_mode_disable(chip_info) < 0) {
		return -EIO;
	}

	return ret;
}

static int core_spi_ice_mode_read(void *chip_data, u8 *data, int len)
{
	int size = 0, ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ret = core_spi_ice_mode_enable(chip_info);

	if (ret < 0) {
		return ret;
	}

	/*
	 * Check FW if they already send their data to rxbuf.
	 * They change lock status from 0x9881 to 0x5AA5 if they did.
	 */
	ret = core_rx_lock_check(chip_info, &size);

	if (ret < 0) {
		goto out;
	}

	if (len < size) {
		TPD_INFO("WARRING! size(%d) > len(%d), use len to get data\n", size, len);
		size = len;
	}

	/* receive data from rxbuf and change lock status to 0x9881. */
	ret = core_spi_ice_mode_unlock_read(chip_info, data, size);

	if (ret < 0) {
		goto out;
	}

out:

	if (core_spi_ice_mode_disable(chip_info) < 0) {
		return -EIO;
	}

	if (ret >= 0) {
		return size;
	}

	return ret;
}

int core_spi_check_read_size(void *chip_data)
{
	int size = 0, ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ret = core_spi_ice_mode_enable(chip_info);

	if (ret < 0) {
		return ret;
	}

	/*
	 * Check FW if they already send their data to rxbuf.
	 * They change lock status from 0x9881 to 0x5AA5 if they did.
	 */
	ret = core_rx_lock_check(chip_info, &size);

	if (ret < 0) {
		goto out;
	}

	return size;
out:
	core_spi_ice_mode_disable(chip_info);
	return -EIO;
}

static int core_spi_read_data_after_checksize(void *chip_data, uint8_t *data,
		int size)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ret = core_spi_ice_mode_unlock_read(chip_info, data, size);

	if (ret < 0) {
		goto out;
	}

out:

	if (core_spi_ice_mode_disable(chip_info) < 0) {
		return -EIO;
	}

	if (ret < 0) {
		return ret;
	}

	return size;
}

static int core_spi_write(void *chip_data, u8 *data, int len)
{
	int ret = 0, count = 2;
	u8 *txbuf = NULL;
	int safe_size = len;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	int zalloc_size = 0;

	if (atomic_read(&chip_info->ice_stat) == DISABLE) {
		do {
			ret = core_spi_ice_mode_write(chip_info, data, len);

			if (ret >= 0) {
				break;
			}
		} while (--count > 0);

		goto out;
	}

	txbuf = tp_devm_kzalloc(chip_info->dev, (len + 1) * sizeof(u8), GFP_KERNEL);

	if (ERR_ALLOC_MEM(txbuf)) {
		TPD_INFO("Failed to allocate txbuf\n");
		return -ENOMEM;
	}

	zalloc_size = (len + 1) * sizeof(u8);

	txbuf[0] = SPI_WRITE;
	tp_memcpy(txbuf + 1, len, data,  safe_size, len);

	if (spi_write_then_read(chip_info->spi, txbuf, len + 1, txbuf, 0) < 0) {
		TPD_INFO("spi write data error in ice mode\n");
		ret = -EIO;
		goto out;
	}

out:
	tp_devm_kfree(chip_info->dev, (void **)&txbuf, zalloc_size);
	return ret;
}

static int core_spi_read(void *chip_data, u8 *rxbuf, int len)
{
	int ret = 0, count = 2;
	u8 txbuf[1] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	txbuf[0] = SPI_READ;

	if (atomic_read(&chip_info->ice_stat) == DISABLE) {
		do {
			ret = core_spi_ice_mode_read(chip_info, rxbuf, len);

			if (ret >= 0) {
				break;
			}
		} while (--count > 0);

		goto out;
	}

	if (spi_write_then_read(chip_info->spi, txbuf, 1, rxbuf, len) < 0) {
		TPD_INFO("spi read data error in ice mode\n");
		ret = -EIO;
		goto out;
	}

out:
	return ret;
}

static int ilitek_spi_write(void *chip_data, void *buf, int len)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!len) {
		TPD_INFO("spi write len is invaild\n");
		return -EINVAL;
	}

	ret = core_spi_write(chip_info, buf, len);

	if (ret < 0) {
		if (atomic_read(&chip_info->tp_reset) == START) {
			ret = 0;
			goto out;
		}

		TPD_INFO("spi write error, ret = %d\n", ret);
	}

out:
	return ret;
}

/* If ilitek_spi_read success ,this function will return read length */
static int ilitek_spi_read(void *chip_data, void *buf, int len)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!len) {
		TPD_INFO("spi read len is invaild\n");
		return -EINVAL;
	}

	ret = core_spi_read(chip_info, buf, len);

	if (ret < 0) {
		if (atomic_read(&chip_info->tp_reset) == START) {
			ret = 0;
			goto out;
		}

		TPD_INFO("spi read error, ret = %d\n", ret);
	}

out:
	return ret;
}

static int core_spi_setup(void *chip_data, u32 freq)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("spi clock = %d\n", freq);

	chip_info->spi->mode = SPI_MODE_0;
	chip_info->spi->bits_per_word = 8;

	if (chip_info->spi->max_speed_hz == 0) {
		chip_info->spi->max_speed_hz = freq;
	}

	chip_info->spi->chip_select =
		0; /*modify reg=0 for more tp vendor share same spi interface*/

	if (spi_setup(chip_info->spi) < 0) {
		TPD_INFO("Failed to setup spi device\n");
		return -ENODEV;
	}

	TPD_INFO("name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n",
		 chip_info->spi->modalias,
		 chip_info->spi->master->bus_num,
		 chip_info->spi->chip_select,
		 chip_info->spi->mode,
		 chip_info->spi->max_speed_hz);
	return 0;
}

/*******Part2:comom Function implement*******/

static void ilitek_plat_tp_reset(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("edge delay = %d\n", RST_EDGE_DELAY);
	gpio_direction_output(chip_info->hw_res->reset_gpio, 1);
	mdelay(1);
	gpio_set_value(chip_info->hw_res->reset_gpio, 0);
	mdelay(5);
	gpio_set_value(chip_info->hw_res->reset_gpio, 1);
	mdelay(RST_EDGE_DELAY);
}

/*******Part3:main flow  Function implement*******/


static int ilitek_tddi_move_mp_code_iram(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("Download MP code to iram\n");
	return ilitek_tddi_fw_upgrade(chip_info);
}


int ilitek_tddi_switch_mode(void *chip_data, u8 *data)
{
	int ret = 0, mode;
	u8 cmd[4] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!data) {
		TPD_INFO("data is null\n");
		return -EINVAL;
	}

	atomic_set(&chip_info->tp_sw_mode, START);

	mode = data[0];

	if (mode == P5_X_FW_DEMO_MODE
			|| mode == P5_X_FW_GESTURE_MODE
			|| mode == P5_X_FW_TEST_MODE) {
		chip_info->actual_tp_mode = mode;
	}

	switch (mode) {
	case P5_X_FW_I2CUART_MODE:
		TPD_INFO("Not implemented yet\n");
		break;

	case P5_X_FW_DEMO_MODE:
		TPD_INFO("Switch to Demo mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = chip_info->write(chip_info, cmd, 2);

		if (ret < 0) {
			msleep(100);
			ret = chip_info->write(chip_info, cmd, 2);

			if (ret < 0) {
				TPD_INFO("Failed to switch demo mode, do reset/reload instead\n");
				ret = ilitek_tddi_fw_upgrade(chip_info);
			}
		}

		break;

	case P5_X_FW_DEBUG_MODE:
		TPD_INFO("Switch to Debug mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = chip_info->write(chip_info, cmd, 2);

		if (ret < 0) {
			msleep(100);
			ret = chip_info->write(chip_info, cmd, 2);

			if (ret < 0) {
				TPD_INFO("Failed to switch Debug mode\n");
			}
		}

		break;

	case P5_X_FW_GESTURE_MODE:
		TPD_INFO("Switch to Gesture mode, lpwg cmd = %d\n",  chip_info->gesture_mode);
		ret = ilitek_tddi_ic_func_ctrl(chip_info, "lpwg", chip_info->gesture_mode);
		break;

	case P5_X_FW_TEST_MODE:
		TPD_INFO("Switch to Test mode\n");
		ret = ilitek_tddi_move_mp_code_iram(chip_info);
		break;

	case P5_X_FW_DEMO_DEBUG_INFO_MODE:
		TPD_INFO("Switch to demo debug info mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = chip_info->write(chip_info, cmd, 2);

		if (ret < 0) {
			TPD_INFO("Failed to switch debug info mode\n");
		}

		break;

	case P5_X_FW_SOP_FLOW_MODE:
		TPD_INFO("Not implemented SOP flow mode yet\n");
		break;

	case P5_X_FW_ESD_MODE:
		TPD_INFO("Not implemented ESD mode yet\n");
		break;

	default:
		TPD_INFO("Unknown TP mode: %x\n", mode);
		ret = -1;
		break;
	}

	if (ret < 0) {
		TPD_INFO("Switch mode failed\n");
	}

	TPD_DEBUG("Actual TP mode = %d\n", chip_info->actual_tp_mode);
	atomic_set(&chip_info->tp_sw_mode, END);
	return ret;
}

static int ilitek_tddi_move_gesture_code_iram(void *chip_data, int mode)
{
	int i;
	u8 tp_mode = P5_X_FW_GESTURE_MODE;
	u8 cmd[3] = {0};
	int retry = 10;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("In %s\n", __func__);

	if (ilitek_tddi_ic_func_ctrl(chip_info, "lpwg", 0x3) < 0) {
		TPD_INFO("write gesture flag failed\n");
	}

	ilitek_tddi_switch_mode(chip_info, &tp_mode);

	for (i = 0; i < retry; i++) {
		/* Prepare Check Ready */
		cmd[0] = P5_X_READ_DATA_CTRL;
		cmd[1] = 0xA;
		cmd[2] = 0x5;
		chip_info->write(chip_info, cmd, 2);

		mdelay(10);

		/* Check ready for load code */
		cmd[0] = 0x1;
		cmd[1] = 0xA;
		cmd[2] = 0x5;

		if ((chip_info->write(chip_info, cmd, 3)) < 0) {
			TPD_INFO("write 0x1,0xA,0x5 error");
		}

		if ((chip_info->read(chip_info, cmd, 1)) < 0) {
			TPD_INFO("read gesture ready byte error\n");
		}

		TPD_DEBUG("gesture ready byte = 0x%x\n", cmd[0]);

		if (cmd[0] == 0x91) {
			TPD_INFO("Gesture check fw ready\n");
			break;
		}
	}

	if (i >= retry) {
		TPD_INFO("Gesture is not ready, 0x%x running gesture esd flow\n", cmd[0]);
		return -1;
	}

	ilitek_tddi_fw_upgrade(chip_info);

	/* FW star run gestrue code cmd */
	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x6;

	if ((chip_info->write(chip_info, cmd, 3)) < 0) {
		TPD_INFO("write 0x1,0xA,0x6 error");
	}

	return 0;
}

static void ilitek_tddi_touch_esd_gesture_iram(void *chip_data)
{
	int retry = 50;
	u32 answer = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* start to download AP code with host download */
	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ilitek_tddi_fw_upgrade(chip_info);

	ilitek_ice_mode_ctrl(chip_info, ENABLE, OFF);

	TPD_INFO("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
		 SPI_ESD_GESTURE_PWD_ADDR, SPI_ESD_GESTURE_RUN);

	/* write a special password to inform FW go back into gesture mode */
	if (ilitek_ice_mode_write(chip_info, SPI_ESD_GESTURE_PWD_ADDR, ESD_GESTURE_PWD,
				  4) < 0) {
		TPD_INFO("write password failed\n");
	}

	/* Host download gives effect to FW receives password successed */
	ilitek_tddi_fw_upgrade(chip_info);
	/* waiting for FW reloading code */
	msleep(100);

	ilitek_ice_mode_ctrl(chip_info, ENABLE, ON);

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ilitek_ice_mode_read(chip_info, SPI_ESD_GESTURE_PWD_ADDR, &answer, sizeof(u32));

		if (answer != SPI_ESD_GESTURE_RUN) {
			TPD_INFO("answer = 0x%x != (0x%x)\n", answer, SPI_ESD_GESTURE_RUN);
		}

		msleep(10);
	} while (answer != SPI_ESD_GESTURE_RUN && --retry > 0);

	if (retry <= 0) {
		TPD_INFO("Enter gesture failed\n");

	} else {
		TPD_INFO("Enter gesture successfully\n");
	}

	ilitek_ice_mode_ctrl(chip_info, DISABLE, ON);

	ilitek_tddi_move_gesture_code_iram(chip_info, chip_info->gesture_mode);
}


void ilitek_tddi_wq_ges_recover(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	atomic_set(&chip_info->esd_stat, START);
	ilitek_tddi_touch_esd_gesture_iram(chip_info);
	chip_info->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	atomic_set(&chip_info->esd_stat, END);
}

static void ilitek_tddi_wq_spi_recover(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	chip_info->esd_check_enabled = false;
	atomic_set(&chip_info->esd_stat, START);
	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ilitek_tddi_fw_upgrade(chip_info);
	atomic_set(&chip_info->esd_stat, END);
	chip_info->esd_check_enabled = true;
}

static int ilitek_tddi_wq_esd_spi_check(void *chip_data)
{
	int ret = 0;
	u8 tx = SPI_WRITE;
	u8 rx = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (spi_write_then_read(chip_info->spi, &tx, 1, &rx, 1) < 0) {
		return -EIO;
	}

	TPD_DEBUG("spi esd check = 0x%x\n", rx);

	if (rx != SPI_ACK) {
		TPD_INFO("rx = 0x%x\n", rx);
		ret = -1;
	}

	return ret;
}

static int ilitek_tddi_wq_esd_check(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (ilitek_tddi_wq_esd_spi_check(chip_info) < 0) {
		TPD_INFO("SPI ACK failed, doing spi recovery\n");
		tp_touch_btnkey_release(chip_info->tp_index);
		ilitek_tddi_wq_spi_recover(chip_info);
		return -1;
	}

	return 0;
}

static int ilitek_tddi_sleep_handler(void *chip_data, enum TP_SLEEP_STATUS mode)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	atomic_set(&chip_info->tp_sleep, START);

	if (atomic_read(&chip_info->fw_stat) ||
			atomic_read(&chip_info->mp_stat)) {
		TPD_INFO("fw upgrade or mp still running, ignore sleep requst\n");
		atomic_set(&chip_info->tp_sleep, END);
		return 0;
	}

	TPD_INFO("Sleep Mode = %d\n", mode);
	chip_info->esd_check_enabled = false;

	switch (mode) {
	case TP_SUSPEND:
		TPD_INFO("TP normal suspend start\n");
		ilitek_tddi_ic_func_ctrl(chip_info, "sense", DISABLE);
		ilitek_tddi_ic_check_busy(chip_info, 5, 35);

		if (chip_info->gesture) {
			ret = ilitek_tddi_move_gesture_code_iram(chip_info, chip_info->gesture_mode);

			if (ret < 0) {
				atomic_set(&chip_info->esd_stat, START);
				ilitek_tddi_touch_esd_gesture_iram(chip_info);
				atomic_set(&chip_info->esd_stat, END);
			}

		} else {
			ilitek_tddi_ic_func_ctrl(chip_info, "sleep", DEEP_SLEEP_IN);
		}

		msleep(35);
		TPD_INFO("TP normal suspend end\n");
		break;

	case TP_DEEP_SLEEP:
		TPD_INFO("TP deep suspend start\n");
		ilitek_tddi_ic_func_ctrl(chip_info, "sense", DISABLE);
		ilitek_tddi_ic_check_busy(chip_info, 5, 50);
		ilitek_tddi_ic_func_ctrl(chip_info, "sleep", DEEP_SLEEP_IN);
		msleep(35);
		TPD_INFO("TP deep suspend end\n");
		break;

	default:
		TPD_INFO("Unknown sleep mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	tp_touch_btnkey_release(chip_info->tp_index);
	atomic_set(&chip_info->tp_sleep, END);
	return ret;
}

static int ilitek_tddi_get_tp_recore_data(void *chip_data, u16 *out_buf,
		u32 out_len)
{
	u8 buf[8] = {0};
	u8 record_case = 0;
	s8 index;
	u16 *raw = NULL;
	u16 *raw_ptr = NULL;
	u16 frame_len;
	u32 base_addr = 0x20000;
	u32 addr;
	u32 len;
	u32 i;
	u8 frame_cnt;
	int ret = 0;
	bool ice;
	struct record_state record_stat;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ice = atomic_read(&chip_info->ice_stat);

	if (chip_info->read(chip_info, buf, sizeof(buf)) < 0) {
		TPD_INFO("Get info fail\n");
		return -1;
	}

	addr = ((buf[0] << 8) | buf[1]) + base_addr;
	len = ((buf[2] << 8) | buf[3]);
	index = buf[4];
	frame_cnt = buf[5];
	record_case = buf[6];
	tp_memcpy(&record_stat, 1, &buf[7], 1, 1);
	TPD_INFO("addr = 0x%x, len = %d, lndex = %d, frame_cnt = %d, record_case = 0x%x\n",
		 addr, len, index, frame_cnt, record_case);
	ilitek_dump_data(buf, 8, sizeof(buf), 0, "all record bytes");

	if (len > 4096) {
		TPD_INFO("ilitek_tddi_get_tp_recore_data len is %d.\n", len);
		return -1;
	}

	raw = kzalloc(len * sizeof(u8), GFP_ATOMIC);

	if (ERR_ALLOC_MEM(raw)) {
		TPD_INFO("Failed to allocate packet memory, %ld\n", PTR_ERR(raw));
		return -1;
	}

	if (!ice) {
		ilitek_ice_mode_ctrl(chip_info, ENABLE, ON);
	}

	/*for (i = 0, j = 0; i < len; i += 4, j++) {*/
	/*    ilitek_ice_mode_read((addr + i), &ptr[j], sizeof(u32));*/
	/*}*/
	buf[0] = 0x25;
	buf[3] = (char)((addr & 0x00FF0000) >> 16);
	buf[2] = (char)((addr & 0x0000FF00) >> 8);
	buf[1] = (char)((addr & 0x000000FF));

	if (chip_info->write(chip_info, buf, 4)) {
		TPD_INFO("Failed to write iram data\n");
		return -ENODEV;
	}

	if (chip_info->read(chip_info, (u8 *)raw, len)) {
		TPD_INFO("Failed to Read iram data\n");
		return -ENODEV;
	}

	if (frame_cnt > 0 && index >= 0 && index < 3) {
		frame_len = (len / (frame_cnt * 2));

		if (out_buf) {
			if (out_len > frame_len * 2) {
				TPD_INFO("out_len is %d, frame_len is %d\n", out_len, frame_len);
				out_len = frame_len * 2;
			}

			if (out_len > 0) {
				memcpy(out_buf, &raw[index * frame_len], out_len);
			}
		}

		for (i = 0; i < frame_cnt; i++) {
			raw_ptr = raw + (index * frame_len);

			ilitek_dump_data(raw_ptr, 16, frame_len, chip_info->xch_num, "recore_data");
			index--;

			if (index < 0) {
				index = frame_cnt - 1;
			}
		}

	} else {
		ret = -1;
	}



	if (!ice) {
		ilitek_ice_mode_ctrl(chip_info, DISABLE, ON);
	}

	if (record_case == 2) {
		TPD_INFO("tp_palm_stat = %d\n", record_stat.touch_palm_state_e);
		TPD_INFO("app_an_stat = %d\n", record_stat.app_an_statu_e);
		TPD_INFO("app_check_abnor = %d\n", record_stat.app_sys_check_bg_abnormal);
		TPD_INFO("wrong_bg = %d\n", record_stat.g_b_wrong_bg);
	}

	tp_kfree((void **)&raw);

	return ret;
}

int ilitek_tddi_get_tp_recore_ctrl(void *chip_data, int data)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	switch ((int)data) {
	case 0:
		TPD_INFO("recore disable");
		ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 0);
		break;

	case 1:
		TPD_INFO("recore enable");
		ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 1);
		mdelay(200);
		break;

	case 2:
		mdelay(50);
		TPD_INFO("Get data");
		ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 2);

		if (ret < 0) {
			TPD_INFO("cmd fail\n");
			goto out;
		}

		ret = ilitek_tddi_get_tp_recore_data(chip_info, NULL, 0);

		if (ret < 0) {
			TPD_INFO("get data fail\n");
		}

		TPD_INFO("recore reset");
		ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 3);

		if (ret < 0) {
			TPD_INFO("cmd fail\n");
			goto out;
		}

		break;

	default:
		TPD_INFO("Unknown get_tp_recore_ctrl case, %d\n", data);
	}

out:
	return ret;
}

static void ilitek_tddi_touch_send_debug_data(void *chip_data, u8 *buf, int len)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!chip_info->netlink && !chip_info->debug_node_open) {
		return;
	}

	mutex_lock(&chip_info->debug_mutex);

	/* Send data to netlink */
	if (chip_info->netlink) {
		netlink_reply_msg(buf, len);
		goto out;
	}

	/* Sending data to apk via the node of debug_message node */
	if (chip_info->debug_node_open && !(ERR_ALLOC_MEM(chip_info->debug_buf))
			&& !(ERR_ALLOC_MEM(chip_info->debug_buf[chip_info->debug_data_frame]))) {
		memset(chip_info->debug_buf[chip_info->debug_data_frame], 0x00,
		       (u8)sizeof(u8) * DEBUG_DATA_MAX_LENGTH);
		tp_memcpy(chip_info->debug_buf[chip_info->debug_data_frame],
			  (u8)sizeof(u8) * DEBUG_DATA_MAX_LENGTH,
			  buf, len, len);
		chip_info->debug_data_frame++;

		if (chip_info->debug_data_frame > 1) {
			TPD_DEBUG("chip_info->debug_data_frame = %d\n", chip_info->debug_data_frame);
		}

		if (chip_info->debug_data_frame > (DEBUG_DATA_SAVE_MAX_FRAME - 1)) {
			TPD_INFO("chip_info->debug_data_frame = %d > 1023\n",
				 chip_info->debug_data_frame);
			chip_info->debug_data_frame = (DEBUG_DATA_SAVE_MAX_FRAME - 1);
		}

		wake_up(&(chip_info->inq));
		goto out;
	}

out:
	mutex_unlock(&chip_info->debug_mutex);
}

static void ilitek_tddi_report_ap_mode(void *chip_data, u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF)
				&& (buf[(4 * i) + 3] == 0xFF)) {
			continue;
		}

		xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
		yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));
		TPD_DEBUG("original x = %d, y = %d\n", xop, yop);
		chip_info->pointid_info = chip_info->pointid_info | (1 << i);
		chip_info->points[i].x = xop * chip_info->panel_wid / TPD_WIDTH;
		chip_info->points[i].y = yop * chip_info->panel_hei / TPD_HEIGHT;
		chip_info->points[i].z = buf[(4 * i) + 4];
		chip_info->points[i].width_major = buf[(4 * i) + 4];
		chip_info->points[i].touch_major = buf[(4 * i) + 4];
		chip_info->points[i].status = 1;
		TPD_DEBUG("scale x = %d, y = %d, p = %d\n",
			  chip_info->points[i].x, chip_info->points[i].y, chip_info->points[i].z);
	}

	ilitek_tddi_touch_send_debug_data(chip_info, buf, len);
}

static void ilitek_tddi_report_debug_mode(void *chip_data, u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(3 * i) + 5] == 0xFF) && (buf[(3 * i) + 6] == 0xFF)
				&& (buf[(3 * i) + 7] == 0xFF)) {
			continue;
		}

		xop = (((buf[(3 * i) + 5] & 0xF0) << 4) | (buf[(3 * i) + 6]));
		yop = (((buf[(3 * i) + 5] & 0x0F) << 8) | (buf[(3 * i) + 7]));
		TPD_DEBUG("original x = %d, y = %d\n", xop, yop);
		chip_info->pointid_info = chip_info->pointid_info | (1 << i);
		chip_info->points[i].x = xop * chip_info->panel_wid / TPD_WIDTH;
		chip_info->points[i].y = yop * chip_info->panel_hei / TPD_HEIGHT;
		chip_info->points[i].z = buf[(4 * i) + 4];
		chip_info->points[i].width_major = buf[(4 * i) + 4];
		chip_info->points[i].touch_major = buf[(4 * i) + 4];
		chip_info->points[i].status = 1;
		TPD_DEBUG("scale x = %d, y = %d, p = %d\n",
			  chip_info->points[i].x, chip_info->points[i].y, chip_info->points[i].z);
	}

	ilitek_tddi_touch_send_debug_data(chip_info, buf, len);
}

static void ilitek_tddi_report_gesture_mode(void *chip_data, u8 *buf, int len)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("gesture code = 0x%x\n", buf[1]);
	tp_memcpy(chip_info->gesture_data, P5_X_GESTURE_INFO_LENGTH, buf, len, len);
#ifdef CONFIG_OPLUS_TP_APK

	if (chip_info->debug_gesture_sta) {
		if (chip_info->ts->gesture_buf) {
			int tmp_len = len;

			if (tmp_len > P5_X_GESTURE_INFO_LENGTH) {
				tmp_len = P5_X_GESTURE_INFO_LENGTH;
			}

			memcpy(chip_info->ts->gesture_buf, buf, tmp_len);
		}
	}

#endif /* end of CONFIG_OPLUS_TP_APK*/
	ilitek_tddi_touch_send_debug_data(chip_info, buf, len);
}

static void ilitek_tddi_demo_debug_info_mode(void *chip_data, u8 *buf, int len)
{
	u8 *info_ptr;
	u8 info_id, info_len;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	ilitek_tddi_report_ap_mode(chip_info, buf, P5_X_DEMO_MODE_PACKET_LENGTH);
	info_ptr = buf + P5_X_DEMO_MODE_PACKET_LENGTH;
	info_len = info_ptr[0];
	info_id = info_ptr[1];

	TPD_INFO("info len = %d ,id = %d\n", info_len, info_id);

	if (info_id == 0) {
#ifdef CONFIG_OPLUS_TP_APK
		chip_info->demo_debug_info[info_id](chip_info, &info_ptr[1], info_len);
#endif /* end of CONFIG_OPLUS_TP_APK*/

	} else {
		TPD_INFO("not support this id %d\n", info_id);
	}
}

static int ilitek_tddi_report_handler(void *chip_data)
{
	int ret = 0, pid = 0;
	u8 *buf = NULL, *data_buf = NULL, checksum = 0;
	int rlen = 0;
	int tmp = ipio_debug_level;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	int size = 0;

	/* Just in case these stats couldn't be blocked in top half context */
	if (!chip_info->report || atomic_read(&chip_info->tp_reset) ||
			atomic_read(&chip_info->fw_stat) || atomic_read(&chip_info->tp_sw_mode) ||
			atomic_read(&chip_info->mp_stat) || atomic_read(&chip_info->tp_sleep)) {
		TPD_INFO("ignore report request\n");
		return -1;
	}

	rlen = core_spi_check_read_size(chip_info);
	TPD_DEBUG("Packget length = %d\n", rlen);

	if (rlen < 0 || rlen >= 2048) {
		TPD_INFO("Length of packet is invaild\n");

		if (rlen == DO_SPI_RECOVER && chip_info->actual_tp_mode == P5_X_FW_GESTURE_MODE
				&& chip_info->gesture) {
			TPD_INFO("Gesture failed, doing gesture recovery\n");
			tp_touch_btnkey_release(chip_info->tp_index);
			ilitek_tddi_wq_ges_recover(chip_info);
			goto out;

		} else if (rlen == DO_SPI_RECOVER) {
			TPD_INFO("SPI ACK failed, doing spi recovery\n");
			tp_touch_btnkey_release(chip_info->tp_index);
			ilitek_tddi_wq_spi_recover(chip_info);
			goto out;
		}

		ret = -1;
		goto out;
	}

	buf = tp_devm_kzalloc(chip_info->dev, rlen * sizeof(u8), GFP_ATOMIC);

	if (ERR_ALLOC_MEM(buf)) {
		TPD_INFO("Failed to allocate packet memory, %ld\n", PTR_ERR(buf));
		return -1;
	}

	/*add for memery health monitor*/
	size = rlen * sizeof(u8);
	ret = core_spi_read_data_after_checksize(chip_info, buf, rlen);

	if (ret < 0) {
		TPD_INFO("Read report packet failed ret = %d\n", ret);
		goto out;
	}

	TPD_DEBUG("Read length = %d\n", (ret));

	rlen = ret;

	ilitek_dump_data(buf, 8, rlen, 0, "finger report");

	checksum = ilitek_calc_packet_checksum(buf, rlen - 1);

	if (checksum != buf[rlen - 1]) {
		ret = -1;
		TPD_INFO("Wrong checksum, checksum = %x, buf = %x\n", checksum, buf[rlen - 1]);
		ipio_debug_level = 1;
		ilitek_dump_data(buf, 8, rlen, 0, "finger report");
		ipio_debug_level = tmp;

		if (P5_X_I2CUART_PACKET_ID != buf[0]) {
			goto out;
		}
	}

	pid = buf[0];
	data_buf = buf;

	if (pid == P5_X_INFO_HEADER_PACKET_ID) {
		TPD_DEBUG("Have header PID = %x\n", pid);
		data_buf = buf + P5_X_INFO_HEADER_LENGTH;
		pid = data_buf[0];
	}

	TPD_DEBUG("Packet ID = %x\n", pid);

	switch (pid) {
	case P5_X_DEMO_PACKET_ID:
		if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH) {
			TPD_INFO("read ap mode data len < 43 len = %d\n", rlen);
			ret = -1;

		} else {
			ilitek_tddi_report_ap_mode(chip_info, data_buf, rlen);
		}

		break;

	case P5_X_DEBUG_PACKET_ID:
		if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH) {
			TPD_INFO("read debug mode data len < 43 len = %d\n", rlen);
			ret = -1;

		} else {
			ilitek_tddi_report_debug_mode(chip_info, data_buf, rlen);
		}

		break;

	case P5_X_I2CUART_PACKET_ID:
		ret = -1;
		ilitek_tddi_touch_send_debug_data(chip_info, data_buf, rlen);
		break;

	case P5_X_GESTURE_PACKET_ID:
	case P5_X_GESTURE_FAIL_ID:
		ilitek_tddi_report_gesture_mode(chip_info, data_buf, rlen);
		break;

	case P5_X_DEMO_DEBUG_INFO_PACKET_ID:
		if (rlen < P5_X_DEMO_MODE_PACKET_LENGTH + 2) {
			TPD_INFO("read demo debug info mode data len < 45 len = %d\n", rlen);
			ret = -1;

		} else {
			ilitek_tddi_demo_debug_info_mode(chip_info, data_buf, rlen);
		}

		break;

	default:
		TPD_INFO("Unknown packet id, %x\n", pid);
		break;
	}

out:
	tp_devm_kfree(chip_info->dev, (void **)&buf, size);
	return ret;
}

int ilitek_tddi_reset_ctrl(void *chip_data, enum TP_RST_METHOD mode)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	atomic_set(&chip_info->tp_reset, START);

	if (mode != TP_IC_CODE_RST) {
		ilitek_tddi_ic_check_otp_prog_mode(chip_info);
	}

	switch (mode) {
	case TP_IC_CODE_RST:
		TPD_INFO("TP IC Code RST \n");
		ret = ilitek_tddi_ic_code_reset(chip_info);
		break;

	case TP_IC_WHOLE_RST:
		TPD_INFO("TP IC whole RST\n");
		ret = ilitek_tddi_ic_whole_reset(chip_info);
		break;

	case TP_HW_RST_ONLY:
		TPD_INFO("TP HW RST\n");
		ilitek_plat_tp_reset(chip_info);
		break;

	default:
		TPD_INFO("Unknown reset mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	/*
	 * Since OTP must be folloing with reset, except for code rest,
	 * the stat of ice mode should be set as 0.
	 */
	if (mode != TP_IC_CODE_RST) {
		atomic_set(&chip_info->ice_stat, DISABLE);
	}

	atomic_set(&chip_info->tp_reset, END);
	return ret;
}


/*******Part3:irq hander Function implement*******/

void ilitek_dump_data(void *data, int type, int len, int row_len,
		      const char *name)
{
	int i, row = 31;
	u8 *p8 = NULL;
	s32 *p32 = NULL;
	s16 *p16 = NULL;

	if (row_len > 0) {
		row = row_len;
	}

	if (1) {
		if (data == NULL) {
			TPD_INFO("The data going to dump is NULL\n");
			return;
		}

		pr_cont("\n\n");
		pr_cont("ILITEK: Dump %s data\n", name);
		pr_cont("ILITEK: ");

		if (type == 8) {
			p8 = (u8 *) data;
		}

		if (type == 32 || type == 10) {
			p32 = (s32 *) data;
		}

		if (type == 16) {
			p16 = (s16 *) data;
		}

		for (i = 0; i < len; i++) {
			if (type == 8) {
				pr_cont(" %4x ", p8[i]);

			} else if (type == 32) {
				pr_cont(" %4x ", p32[i]);

			} else if (type == 10) {
				pr_cont(" %4d ", p32[i]);

			} else if (type == 16) {
				pr_cont(" %5d ", p16[i]);
			}

			if ((i % row) == row - 1) {
				pr_cont("\n");
				pr_cont("ILITEK: ");
			}
		}

		pr_cont("\n\n");
	}
}

void ilitek_set_gesture_fail_reason(void *chip_data, bool enable)
{
	u8 cmd[24] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	/* set symbol */
	if (ilitek_tddi_ic_func_ctrl(chip_info, "knock_en", 0x8) < 0) {
		TPD_INFO("set symbol failed");
	}

	/* enable gesture fail reason */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x10;

	if (enable) {
		cmd[3] = 0x01;

	} else {
		cmd[3] = 0x00;
	}

	cmd[4] = 0xFF;
	cmd[5] = 0xFF;

	if ((chip_info->write(chip_info, cmd, 6)) < 0) {
		TPD_INFO("enable gesture fail reason failed");
	}

	/* set gesture parameters */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x12;
	cmd[3] = 0x01;
	memset(cmd + 4, 0xFF, 20);

	if ((chip_info->write(chip_info, cmd, 24)) < 0) {
		TPD_INFO("set gesture parameters failed");
	}

	/* get gesture parameters */
	cmd[0] = 0x01;
	cmd[1] = 0x0A;
	cmd[2] = 0x11;
	cmd[3] = 0x01;

	if ((chip_info->write(chip_info, cmd, 4)) < 0) {
		TPD_INFO("get gesture parameters failed");
	}
}

#ifdef CONFIG_OPLUS_TP_APK
static void ili_write_log_buf(void *chip_data, u8 main_id, u8 sec_id)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	log_buf_write(chip_info->ts, main_id);
	sec_id = sec_id | 0x80;
	log_buf_write(chip_info->ts, sec_id);
}


static void ilitek_tddi_demo_debug_info_id0(void *chip_data, u8 *buf, int len)
{
	static struct demo_debug_info_id0 id_last;
	struct demo_debug_info_id0 id0;
	/*TPD_INFO("id0 len = %d,strucy len = %ld", (int)len, sizeof(id0));*/
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	tp_memcpy(&id0, sizeof(id0), buf, len, len);

	if (id_last.sys_powr_state_e != id0.sys_powr_state_e) {
		ili_write_log_buf(chip_info, 1, id0.sys_powr_state_e);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("app_sys_powr_state_e = %d\n", id0.sys_powr_state_e);
		}
	}

	if (id_last.sys_state_e != id0.sys_state_e) {
		ili_write_log_buf(chip_info, 2, id0.sys_state_e);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("app_sys_state_e = %d\n", id0.sys_state_e);
		}
	}

	if (id_last.tp_state_e != id0.tp_state_e) {
		ili_write_log_buf(chip_info, 3, id0.tp_state_e);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("tp_state_e = %d\n", id0.tp_state_e);
		}
	}

	if (id_last.touch_palm_state != id0.touch_palm_state) {
		ili_write_log_buf(chip_info, 4, id0.touch_palm_state);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("touch_palm_state_e = %d\n", id0.touch_palm_state);
		}
	}

	if (id_last.app_an_statu_e != id0.app_an_statu_e) {
		ili_write_log_buf(chip_info, 5, id0.app_an_statu_e);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("app_an_statu_e = %d\n", id0.app_an_statu_e);
		}
	}

	if (id_last.app_sys_bg_err != id0.app_sys_bg_err) {
		ili_write_log_buf(chip_info, 6, id0.app_sys_bg_err);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("app_sys_check_bg_abnormal = %d\n", id0.app_sys_bg_err);
		}
	}

	if (id_last.g_b_wrong_bg != id0.g_b_wrong_bg) {
		ili_write_log_buf(chip_info, 7, id0.g_b_wrong_bg);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("g_b_wrong_bg = %d\n", id0.g_b_wrong_bg);
		}
	}

	if (id_last.reserved0 != id0.reserved0) {
		ili_write_log_buf(chip_info, 8, id0.reserved0);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("reserved0 = %d\n", id0.reserved0);
		}
	}

	if (id_last.normal_mode != id0.normal_mode) {
		if (id0.normal_mode) {
			log_buf_write(chip_info->ts, 9);

		} else {
			log_buf_write(chip_info->ts, 10);
		}
	}

	if (id_last.charger_mode != id0.charger_mode) {
		if (id0.charger_mode) {
			log_buf_write(chip_info->ts, 11);

		} else {
			log_buf_write(chip_info->ts, 12);
		}
	}

	if (id_last.glove_mode != id0.glove_mode) {
		if (id0.glove_mode) {
			log_buf_write(chip_info->ts, 13);

		} else {
			log_buf_write(chip_info->ts, 14);
		}
	}

	if (id_last.stylus_mode != id0.stylus_mode) {
		if (id0.stylus_mode) {
			log_buf_write(chip_info->ts, 15);

		} else {
			log_buf_write(chip_info->ts, 16);
		}
	}

	if (id_last.multi_mode != id0.multi_mode) {
		if (id0.multi_mode) {
			log_buf_write(chip_info->ts, 17);

		} else {
			log_buf_write(chip_info->ts, 18);
		}
	}

	if (id_last.noise_mode != id0.noise_mode) {
		if (id0.noise_mode) {
			log_buf_write(chip_info->ts, 19);

		} else {
			log_buf_write(chip_info->ts, 20);
		}
	}

	if (id_last.palm_plus_mode != id0.palm_plus_mode) {
		if (id0.palm_plus_mode) {
			log_buf_write(chip_info->ts, 21);

		} else {
			log_buf_write(chip_info->ts, 22);
		}
	}

	if (id_last.floating_mode != id0.floating_mode) {
		if (id0.floating_mode) {
			log_buf_write(chip_info->ts, 23);

		} else {
			log_buf_write(chip_info->ts, 24);
		}
	}

	if (tp_debug > 0 || ipio_debug_level) {
		TPD_INFO("debug state is 0x%02X.\n", buf[3]);
	}

	if (id_last.algo_pt_status0 != id0.algo_pt_status0) {
		ili_write_log_buf(chip_info, 25, id0.algo_pt_status0);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status0 = %d\n", id0.algo_pt_status0);
		}
	}

	if (id_last.algo_pt_status1 != id0.algo_pt_status1) {
		ili_write_log_buf(chip_info, 26, id0.algo_pt_status1);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status1 = %d\n", id0.algo_pt_status1);
		}
	}

	if (id_last.algo_pt_status2 != id0.algo_pt_status2) {
		ili_write_log_buf(chip_info, 27, id0.algo_pt_status2);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status2 = %d\n", id0.algo_pt_status2);
		}
	}

	if (id_last.algo_pt_status3 != id0.algo_pt_status3) {
		ili_write_log_buf(chip_info, 28, id0.algo_pt_status3);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status3 = %d\n", id0.algo_pt_status3);
		}
	}

	if (id_last.algo_pt_status4 != id0.algo_pt_status4) {
		ili_write_log_buf(chip_info, 29, id0.algo_pt_status4);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status4 = %d\n", id0.algo_pt_status4);
		}
	}

	if (id_last.algo_pt_status5 != id0.algo_pt_status5) {
		ili_write_log_buf(chip_info, 30, id0.algo_pt_status5);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status5 = %d\n", id0.algo_pt_status5);
		}
	}

	if (id_last.algo_pt_status6 != id0.algo_pt_status6) {
		ili_write_log_buf(chip_info, 31, id0.algo_pt_status6);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status6 = %d\n", id0.algo_pt_status6);
		}
	}

	if (id_last.algo_pt_status7 != id0.algo_pt_status7) {
		ili_write_log_buf(chip_info, 32, id0.algo_pt_status7);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status7 = %d\n", id0.algo_pt_status7);
		}
	}

	if (id_last.algo_pt_status8 != id0.algo_pt_status8) {
		ili_write_log_buf(chip_info, 33, id0.algo_pt_status8);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status8 = %d\n", id0.algo_pt_status8);
		}
	}

	if (id_last.algo_pt_status9 != id0.algo_pt_status9) {
		ili_write_log_buf(chip_info, 34, id0.algo_pt_status9);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status9 = %d\n", id0.algo_pt_status9);
		}
	}

	if (id_last.reserved2 != id0.reserved2) {
		ili_write_log_buf(chip_info, 35, id0.reserved2);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("algo_pt_status9 = %d\n", id0.reserved2);
		}
	}

	if (id0.hopping_flg) {
		if (id_last.hopping_index != id0.hopping_index) {
			ili_write_log_buf(chip_info, 36, id0.hopping_index);

			if (tp_debug > 0 || ipio_debug_level) {
				TPD_INFO("hopping_index = %d\n", id0.hopping_index);
				TPD_INFO("hopping_flg = %d\n", id0.hopping_flg);
				TPD_INFO("freq = %dK\n",
					 (id0.frequency_h << 8) + id0.frequency_l);
			}
		}
	}


	if (id_last.reserved3 != id0.reserved3) {
		ili_write_log_buf(chip_info, 37, id0.reserved3);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("reserved3 = %d\n", id0.reserved3);
		}
	}

	if (id_last.reserved4 != id0.reserved4) {
		ili_write_log_buf(chip_info, 38, id0.reserved4);

		if (tp_debug > 0 || ipio_debug_level) {
			TPD_INFO("reserved4 = %d\n", id0.reserved4);
		}
	}

	tp_memcpy(&id_last, sizeof(id_last), &id0, sizeof(id0), sizeof(id0));

	/*msleep(2000);*/
}

#endif /* end of CONFIG_OPLUS_TP_APK*/

/*******Part4:Call Back Function implement*******/
static int ilitek_ftm_process(void *chip_data)
{
	int ret = -1;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("\n");
	chip_info->hw_res->reset_gpio = chip_info->ts->hw_res.reset_gpio;
	mutex_lock(&chip_info->touch_mutex);
	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ret = ilitek_tddi_fw_upgrade(chip_info);

	if (ret < 0) {
		TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
	}

	TPD_INFO("FTM tp enter sleep\n");
	/*ftm sleep in */
	ilitek_tddi_ic_func_ctrl(chip_info, "sleep", SLEEP_IN_FTM_BEGIN);
	mutex_unlock(&chip_info->touch_mutex);
	return ret;
}

void tp_goto_sleep_ftm(void *chip_data)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (chip_info != NULL && chip_info->ts != NULL) {
		TPD_INFO("chip_info->ts->boot_mode = %d\n", chip_info->ts->boot_mode);

		if ((chip_info->ts->boot_mode == MSM_BOOT_MODE__FACTORY
				|| chip_info->ts->boot_mode == MSM_BOOT_MODE__RF
				|| chip_info->ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
			mutex_lock(&chip_info->touch_mutex);
			chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
			ret = ilitek_tddi_fw_upgrade(chip_info);
			mutex_unlock(&chip_info->touch_mutex);

			if (ret < 0) {
				TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
			}

			/*lcd will goto sleep when tp suspend, close lcd esd check*/
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
			TPD_INFO("disable_esd_thread by tp driver\n");
			disable_esd_thread();
#endif /* end of CONFIG_FB_MSM*/
#endif

			mutex_lock(&chip_info->touch_mutex);
			ilitek_tddi_ic_func_ctrl(chip_info, "sense", DISABLE);
			ilitek_tddi_ic_check_busy(chip_info, 5, 35);
			ilitek_tddi_ic_func_ctrl(chip_info, "sleep", SLEEP_IN_FTM_END);
			mutex_unlock(&chip_info->touch_mutex);
			msleep(60);
			TPD_INFO("mdelay 60 ms test for ftm wait sleep\n");
		}
	}
}

static void ilitek_reset_queue_work_prepare(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	mutex_lock(&chip_info->touch_mutex);
	atomic_set(&chip_info->fw_stat, ENABLE);
	ilitek_tddi_reset_ctrl(chip_info, ILITEK_RESET_METHOD);
	chip_info->ignore_first_irq = true;
	ilitek_ice_mode_ctrl(chip_info, ENABLE, OFF);
	chip_info->already_reset = true;
	mdelay(5);
	mutex_unlock(&chip_info->touch_mutex);
}

static int ilitek_reset(void *chip_data)
{
	int ret = -1;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (chip_info->actual_tp_mode == P5_X_FW_GESTURE_MODE) {
		chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	}

	mutex_lock(&chip_info->touch_mutex);
	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ret = ilitek_tddi_fw_upgrade(chip_info);

	if (ret < 0) {
		TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
	}

	mutex_unlock(&chip_info->touch_mutex);
	return 0;
}

static int ilitek_power_control(void *chip_data, bool enable)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("set reset pin low\n");

	if (gpio_is_valid(chip_info->hw_res->reset_gpio)) {
		gpio_direction_output(chip_info->hw_res->reset_gpio, 0);
	}

	return 0;
}

static int ilitek_get_chip_info(void *chip_data)
{
	TPD_INFO("\n");
	return 0;
}

static u32 ilitek_trigger_reason(void *chip_data, int gesture_enable,
				 int is_suspended)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	chip_info->pointid_info = 0;
	chip_info->irq_timer = jiffies;    /*reset esd check trigger base time*/

	memset(chip_info->points, 0, sizeof(struct point_info) * 10);
	memset(chip_info->gesture_data, 0, P5_X_GESTURE_INFO_LENGTH);

	if (atomic_read(&chip_info->mp_int_check) == ENABLE) {
		atomic_set(&chip_info->mp_int_check, DISABLE);
		TPD_INFO("Get an INT for mp, ignore\n");
		return IRQ_IGNORE;
	}

	/*ignore first irq after hw rst pin reset*/
	if (chip_info->ignore_first_irq) {
		TPD_INFO("ignore_first_irq\n");
		chip_info->ignore_first_irq = false;
		return IRQ_IGNORE;
	}

	if (!chip_info->report || atomic_read(&chip_info->tp_reset) ||
			atomic_read(&chip_info->fw_stat) || atomic_read(&chip_info->tp_sw_mode) ||
			atomic_read(&chip_info->mp_stat) || atomic_read(&chip_info->tp_sleep) ||
			atomic_read(&chip_info->esd_stat)) {
		TPD_INFO("ignore interrupt !\n");
		return IRQ_IGNORE;
	}

	if ((gesture_enable == 1) && is_suspended) {
		if (chip_info->gesture_process_ws) {
			TPD_INFO("black gesture process wake lock\n");
			__pm_stay_awake(chip_info->gesture_process_ws);
		}

		/*mdelay(40);*/

		mutex_lock(&chip_info->touch_mutex);
		ret = ilitek_tddi_report_handler(chip_info);
		mutex_unlock(&chip_info->touch_mutex);

		if (chip_info->gesture_process_ws) {
			TPD_INFO("black gesture process wake unlock\n");
			__pm_relax(chip_info->gesture_process_ws);
		}

		if (ret < 0 || (chip_info->gesture_data[0] == 0)) {
			return IRQ_IGNORE;
		}

		return IRQ_GESTURE;

	} else if (is_suspended) {
		return IRQ_IGNORE;
	}

	mutex_lock(&chip_info->touch_mutex);
	ret = ilitek_tddi_report_handler(chip_info);
	mutex_unlock(&chip_info->touch_mutex);

	if (ret < 0) {
		TPD_INFO("get point info error ignore\n");
		return IRQ_IGNORE;
	}

	return IRQ_TOUCH;
}

static int ilitek_get_touch_points(void *chip_data, struct point_info *points,
				   int max_num)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	memcpy(points, chip_info->points, sizeof(struct point_info) * MAX_TOUCH_NUM);
	return chip_info->pointid_info;
}

static int ilitek_get_gesture_info(void *chip_data,
				   struct gesture_info *gesture)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	uint8_t gesture_id = 0;
	uint8_t score = 0;
	int lu_x = 0;
	int lu_y = 0;
	int rd_x = 0;
	int rd_y = 0;
	uint8_t point_data[P5_X_GESTURE_INFO_LENGTH] = {0};

	memset(point_data, 0, sizeof(point_data));
	memcpy(point_data, chip_info->gesture_data, P5_X_GESTURE_INFO_LENGTH);

	if (point_data[0] != P5_X_GESTURE_PACKET_ID) {
		TPD_INFO("read gesture data failed point_data[0] = 0x%X\n", point_data[0]);
		return -1;
	}

	gesture_id = (uint8_t)(point_data[1]);
	score = point_data[36];

	gesture->Point_start.x = (((point_data[4] & 0xF0) << 4) | (point_data[5]));
	gesture->Point_start.y = (((point_data[4] & 0x0F) << 8) | (point_data[6]));
	gesture->Point_end.x = (((point_data[7] & 0xF0) << 4) | (point_data[8]));
	gesture->Point_end.y = (((point_data[7] & 0x0F) << 8) | (point_data[9]));

	gesture->Point_1st.x = (((point_data[16] & 0xF0) << 4) | (point_data[17]));
	gesture->Point_1st.y = (((point_data[16] & 0x0F) << 8) | (point_data[18]));
	gesture->Point_2nd.x = (((point_data[19] & 0xF0) << 4) | (point_data[20]));
	gesture->Point_2nd.y = (((point_data[19] & 0x0F) << 8) | (point_data[21]));
	gesture->Point_3rd.x = (((point_data[22] & 0xF0) << 4) | (point_data[23]));
	gesture->Point_3rd.y = (((point_data[22] & 0x0F) << 8) | (point_data[24]));

	switch (gesture_id) {   /*judge gesture type*/
	case GESTURE_RIGHT :
		gesture->gesture_type  = LEFT2RIGHT_SWIP;
		break;

	case GESTURE_LEFT :
		gesture->gesture_type  = RIGHT2LEFT_SWIP;
		break;

	case GESTURE_DOWN  :
		gesture->gesture_type  = UP2DOWN_SWIP;
		break;

	case GESTURE_UP :
		gesture->gesture_type  = DOWN2UP_SWIP;
		break;

	case GESTURE_DOUBLECLICK:
		gesture->gesture_type  = DOU_TAP;
		gesture->Point_end     = gesture->Point_start;
		break;

	case GESTURE_V :
		gesture->gesture_type  = UP_VEE;
		break;

	case GESTURE_V_DOWN :
		gesture->gesture_type  = DOWN_VEE;
		break;

	case GESTURE_V_LEFT :
		gesture->gesture_type  = LEFT_VEE;
		break;

	case GESTURE_V_RIGHT :
		gesture->gesture_type  = RIGHT_VEE;
		break;

	case GESTURE_O  :
		gesture->gesture_type = CIRCLE_GESTURE;
		gesture->clockwise = (point_data[34] > 1) ? 0 : point_data[34];

		lu_x = (((point_data[28] & 0xF0) << 4) | (point_data[29]));
		lu_y = (((point_data[28] & 0x0F) << 8) | (point_data[30]));
		rd_x = (((point_data[31] & 0xF0) << 4) | (point_data[32]));
		rd_y = (((point_data[31] & 0x0F) << 8) | (point_data[33]));

		gesture->Point_1st.x = ((rd_x + lu_x) / 2);  /*ymain*/
		gesture->Point_1st.y = lu_y;
		gesture->Point_2nd.x = lu_x;  /*xmin*/
		gesture->Point_2nd.y = ((rd_y + lu_y) / 2);
		gesture->Point_3rd.x = ((rd_x + lu_x) / 2);  /*ymax*/
		gesture->Point_3rd.y = rd_y;
		gesture->Point_4th.x = rd_x;  /*xmax*/
		gesture->Point_4th.y = ((rd_y + lu_y) / 2);
		break;

	case GESTURE_M  :
		gesture->gesture_type  = M_GESTRUE;
		break;

	case GESTURE_W :
		gesture->gesture_type  = W_GESTURE;
		break;

	case GESTURE_TWOLINE_DOWN :
		gesture->gesture_type  = DOU_SWIP;
		gesture->Point_1st.x   = (((point_data[10] & 0xF0) << 4) | (point_data[11]));
		gesture->Point_1st.y   = (((point_data[10] & 0x0F) << 8) | (point_data[12]));
		gesture->Point_2nd.x   = (((point_data[13] & 0xF0) << 4) | (point_data[14]));
		gesture->Point_2nd.y   = (((point_data[13] & 0x0F) << 8) | (point_data[15]));
		break;

	default:
		gesture->gesture_type = UNKOWN_GESTURE;
		break;
	}

	TPD_DEBUG("gesture data 0-17 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
		  "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
		  "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		  point_data[0], point_data[1], point_data[2], point_data[3],
		  point_data[4], point_data[5], point_data[6], point_data[7],
		  point_data[8], point_data[9], point_data[10], point_data[11],
		  point_data[12], point_data[13], point_data[14], point_data[15],
		  point_data[16], point_data[17]);

	TPD_DEBUG("gesture data 18-35 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
		  "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X "
		  "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		  point_data[18], point_data[19], point_data[20], point_data[21],
		  point_data[22], point_data[23], point_data[24], point_data[25],
		  point_data[26], point_data[27], point_data[28], point_data[29],
		  point_data[30], point_data[31], point_data[32], point_data[33],
		  point_data[34], point_data[35]);

	TPD_INFO("gesture debug data 160-168 0x%02X 0x%02X "
		 "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		 point_data[160], point_data[161], point_data[162], point_data[163],
		 point_data[164], point_data[165], point_data[166], point_data[167],
		 point_data[168]);

	TPD_DEBUG("before scale gesture_id: 0x%x, score: %d, "
		  "gesture_type: %d, clockwise: %d,"
		  "points: (%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n",
		  gesture_id, score, gesture->gesture_type, gesture->clockwise,
		  gesture->Point_start.x, gesture->Point_start.y,
		  gesture->Point_end.x, gesture->Point_end.y,
		  gesture->Point_1st.x, gesture->Point_1st.y,
		  gesture->Point_2nd.x, gesture->Point_2nd.y,
		  gesture->Point_3rd.x, gesture->Point_3rd.y,
		  gesture->Point_4th.x, gesture->Point_4th.y);

	gesture->Point_start.x = gesture->Point_start.x * chip_info->panel_wid /
				 TPD_WIDTH;
	gesture->Point_start.y = gesture->Point_start.y * chip_info->panel_hei /
				 TPD_HEIGHT;
	gesture->Point_end.x = gesture->Point_end.x * chip_info->panel_wid / TPD_WIDTH;
	gesture->Point_end.y = gesture->Point_end.y * chip_info->panel_hei / TPD_HEIGHT;
	gesture->Point_1st.x = gesture->Point_1st.x * chip_info->panel_wid / TPD_WIDTH;
	gesture->Point_1st.y = gesture->Point_1st.y * chip_info->panel_hei / TPD_HEIGHT;

	gesture->Point_2nd.x = gesture->Point_2nd.x * chip_info->panel_wid / TPD_WIDTH;
	gesture->Point_2nd.y = gesture->Point_2nd.y * chip_info->panel_hei / TPD_HEIGHT;

	gesture->Point_3rd.x = gesture->Point_3rd.x * chip_info->panel_wid / TPD_WIDTH;
	gesture->Point_3rd.y = gesture->Point_3rd.y * chip_info->panel_hei / TPD_HEIGHT;

	gesture->Point_4th.x = gesture->Point_4th.x * chip_info->panel_wid / TPD_WIDTH;
	gesture->Point_4th.y = gesture->Point_4th.y * chip_info->panel_hei / TPD_HEIGHT;

	TPD_INFO("gesture_id: 0x%x, score: %d, gesture_type: %d, "
		 "clockwise: %d, points:"
		 "(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n",
		 gesture_id, score, gesture->gesture_type, gesture->clockwise,
		 gesture->Point_start.x, gesture->Point_start.y,
		 gesture->Point_end.x, gesture->Point_end.y,
		 gesture->Point_1st.x, gesture->Point_1st.y,
		 gesture->Point_2nd.x, gesture->Point_2nd.y,
		 gesture->Point_3rd.x, gesture->Point_3rd.y,
		 gesture->Point_4th.x, gesture->Point_4th.y);

	return 0;
}

static int ilitek_mode_switch(void *chip_data, work_mode mode, int flag)
{
	int ret = 0;
	uint8_t temp[64] = {0};
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (!chip_info->already_upgrade || atomic_read(&chip_info->tp_reset)
			|| atomic_read(&chip_info->fw_stat)
			|| atomic_read(&chip_info->tp_sw_mode)
			|| atomic_read(&chip_info->mp_stat)
			|| atomic_read(&chip_info->tp_sleep)
			|| atomic_read(&chip_info->esd_stat)) {
		TPD_INFO("doing other process!\n");
		return ret;
	}

	mutex_lock(&chip_info->touch_mutex);

	switch (mode) {
	case MODE_NORMAL:
		TPD_DEBUG("MODE_NORMAL flag = %d\n", flag);
		ret = 0;
		break;

	case MODE_SLEEP:
		TPD_INFO("MODE_SLEEP flag = %d\n", flag);

		/*lcd will goto sleep when tp suspend, close lcd esd check*/
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
		TPD_INFO("disable_esd_thread by tp driver\n");
		disable_esd_thread();
#endif /* end of CONFIG_FB_MSM*/
#endif

		if (chip_info->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
			ilitek_tddi_sleep_handler(chip_info, TP_DEEP_SLEEP);

		} else {
			ilitek_tddi_ic_func_ctrl(chip_info, "sleep", SLEEP_IN);
		}

		/*ilitek_platform_tp_hw_reset(false);*/
		break;

	case MODE_GESTURE:
		TPD_INFO("MODE_GESTURE flag = %d\n", flag);

		if (chip_info->sleep_type == DEEP_SLEEP_IN) {
			TPD_INFO("TP in deep sleep not support gesture flag = %d\n", flag);
			break;
		}

		chip_info->gesture = flag;

		if (flag) {
			/*lcd will goto sleep when tp suspend, close lcd esd check*/
#ifndef CONFIG_TOUCHPANEL_MTK_PLATFORM
#ifdef CONFIG_FB_MSM
			TPD_INFO("disable_esd_thread by tp driver\n");
			disable_esd_thread();
#endif /* end of CONFIG_FB_MSM*/
#endif

			if (chip_info->actual_tp_mode != P5_X_FW_GESTURE_MODE) {
				ilitek_tddi_sleep_handler(chip_info, TP_SUSPEND);

			} else {
				temp[0] = 0xF6;
				temp[1] = 0x0A;
				TPD_INFO("write prepare gesture command 0xF6 0x0A \n");

				if ((chip_info->write(chip_info, temp, 2)) < 0) {
					TPD_INFO("write prepare gesture command error\n");
				}

				if (ilitek_tddi_ic_func_ctrl(chip_info, "lpwg", 0x2) < 0) {
					TPD_INFO("write gesture flag failed\n");
				}
			}

#ifdef CONFIG_OPLUS_TP_APK

			if (chip_info->debug_gesture_sta) {
				ilitek_set_gesture_fail_reason(chip_info, ENABLE);
			}

#endif /* end of CONFIG_OPLUS_TP_APK*/
			chip_info->actual_tp_mode = P5_X_FW_GESTURE_MODE;
		}

		break;

	case MODE_EDGE:
		TPD_INFO("MODE_EDGE flag = %d\n", flag);

		if (VERTICAL_SCREEN == flag) {
			temp[0] = 0x01;

		} else if (LANDSCAPE_SCREEN_270 == flag) {
			temp[0] = 0x00;

		} else if (LANDSCAPE_SCREEN_90 == flag) {
			temp[0] = 0x02;
		}

		if (ilitek_tddi_ic_func_ctrl(chip_info, "edge_palm", temp[0]) < 0) {
			TPD_INFO("write edge_palm flag failed\n");
		}

		break;

	case MODE_HEADSET:
		TPD_INFO("MODE_HEADSET flag = %d\n", flag);

		if (ilitek_tddi_ic_func_ctrl(chip_info, "ear_phone", flag) < 0) {
			TPD_INFO("write ear_phone flag failed\n");
		}

		break;

	case MODE_CHARGE:
		TPD_INFO("MODE_CHARGE flag = %d\n", flag);

		if (ilitek_tddi_ic_func_ctrl(chip_info, "plug", !flag) < 0) {
			TPD_INFO("write plug flag failed\n");
		}

		break;

	case MODE_GAME:
		TPD_INFO("MODE_GAME flag = %d\n", flag);

		if (ilitek_tddi_ic_func_ctrl(chip_info, "lock_point", !flag) < 0) {
			TPD_INFO("write lock_point flag failed\n");
		}

		break;

	default:
		TPD_INFO("%s: Wrong mode.\n", __func__);
	}

	mutex_unlock(&chip_info->touch_mutex);
	return ret;
}

static fw_check_state ilitek_fw_check(void *chip_data,
				      struct resolution_info *resolution_info,
				      struct              panel_info *panel_data)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("%s: call\n", __func__);
	mutex_lock(&chip_info->touch_mutex);
	ret = ilitek_tddi_ic_get_fw_ver(chip_info);
	mutex_unlock(&chip_info->touch_mutex);

	if (ret < 0) {
		TPD_INFO("%s: get fw info failed\n", __func__);

	} else {
		panel_data->tp_fw = (chip_info->chip->fw_ver >> 8) & 0xFF;
		TPD_INFO("firmware_ver = %02X\n", panel_data->tp_fw);
	}

	return FW_NORMAL;
}

static void copy_fw_to_buffer(struct ilitek_tddi_dev *chip_info,
			      const struct firmware *fw)
{
	if (fw) {
		/*free already exist fw data buffer*/
		tp_vfree((void **) & (chip_info->tp_firmware.data));
		chip_info->tp_firmware.size = 0;
		/*new fw data buffer*/
		chip_info->tp_firmware.data = vmalloc(fw->size);

		if (chip_info->tp_firmware.data == NULL) {
			TPD_INFO("kmalloc tp firmware data error\n");

			chip_info->tp_firmware.data = vmalloc(fw->size);

			if (chip_info->tp_firmware.data == NULL) {
				TPD_INFO("retry kmalloc tp firmware data error\n");
				return;
			}
		}

		/*copy bin fw to data buffer*/
		memcpy((u8 *)chip_info->tp_firmware.data, (u8 *)(fw->data), fw->size);
		TPD_INFO("copy_fw_to_buffer fw->size=%zu\n", fw->size);
		chip_info->tp_firmware.size = fw->size;
	}

	return;
}

static fw_update_state ilitek_fw_update(void *chip_data,
					const struct firmware *fw,
					bool force)
{
	int ret = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	struct monitor_data *monitor_data = chip_info->monitor_data;

	TPD_INFO("%s start\n", __func__);

	/*request firmware failed, get from headfile*/
	if (fw == NULL) {
		TPD_INFO("request firmware failed\n");
	}

	copy_fw_to_buffer(chip_info, fw);

	mutex_lock(&chip_info->touch_mutex);

	if (!chip_info->get_ic_info_flag) {
		/* Must do hw reset once in first time for work normally if tp reset is avaliable */
		if (!TDDI_RST_BIND) {
			ilitek_tddi_reset_ctrl(chip_info, ILITEK_RESET_METHOD);
		}

		chip_info->do_otp_check = ENABLE;

		ilitek_ice_mode_ctrl(chip_info, ENABLE, OFF);

		if (ilitek_tddi_ic_get_info(chip_info) < 0) {
			TPD_INFO("Not found ilitek chips\n");
		}

		chip_info->get_ic_info_flag = true;
	}

	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ret = ilitek_tddi_fw_upgrade(chip_info);
	mutex_unlock(&chip_info->touch_mutex);

	if (ret < 0) {
		TPD_INFO("Failed to upgrade firmware, ret = %d\n", ret);
		tp_healthinfo_report(monitor_data, HEALTH_FW_UPDATE,
				     "Failed to upgrade firmware");
		return ret;
	}

	return FW_UPDATE_SUCCESS;
}

static int ilitek_get_vendor(void *chip_data, struct panel_info *panel_data)
{
	int len = 0;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	len = strlen(panel_data->fw_name);

	if (len > 3) {
		panel_data->fw_name[len - 3] = 'b';
		panel_data->fw_name[len - 2] = 'i';
		panel_data->fw_name[len - 1] = 'n';
	}

	chip_info->tp_type = panel_data->tp_type;
	/*get ftm firmware ini from touch.h*/
	chip_info->p_firmware_headfile = &panel_data->firmware_headfile;

	TPD_INFO("chip_info->tp_type = %d, "
		 "panel_data->test_limit_name = %s, panel_data->fw_name = %s\n",
		 chip_info->tp_type,
		 panel_data->test_limit_name, panel_data->fw_name);

	return 0;
}


static int ilitek_esd_handle(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;
	unsigned int timer = jiffies_to_msecs(jiffies - chip_info->irq_timer);
	int ret = 0;

	chip_info->irq_timer = jiffies;
	mutex_lock(&chip_info->touch_mutex);

	if ((chip_info->esd_check_enabled)
			&& (timer >= WQ_ESD_DELAY)
			&& chip_info->already_upgrade) {
		ret = ilitek_tddi_wq_esd_check(chip_info);

	} else {
		TPD_DEBUG("Undo esd_check = %d\n", chip_info->esd_check_enabled);
	}

	mutex_unlock(&chip_info->touch_mutex);
	return ret;
}

static void ilitek_set_touch_direction(void *chip_data, u8 dir)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	chip_info->touch_direction = dir;
}

static u8 ilitek_get_touch_direction(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	return chip_info->touch_direction;
}

static DECLARE_WAIT_QUEUE_HEAD(irq_throw_away_waiter);
/*init_waitqueue_head(&ss->wait);*/
bool ilitek_check_wake_up_state(u32 msecs)
{
	TPD_INFO("wait_event_timeout start!\n");
	wait_event_interruptible_timeout(irq_throw_away_waiter,
					 idev->irq_wake_up_state,
					 msecs_to_jiffies(msecs));

	TPD_INFO("wait_event_timeout end!\n");
	return idev->irq_wake_up_state;
}

static bool ilitek_irq_throw_away(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("ilitek_irq_throw_away!\n");

	if (chip_info->need_judge_irq_throw) {
		chip_info->irq_wake_up_state = true;
		wake_up_interruptible(&irq_throw_away_waiter);
		TPD_INFO("wake up the throw away irq!\n");
		return true;
	}

	return false;
}

static struct oplus_touchpanel_operations ilitek_ops = {
	.ftm_process                = ilitek_ftm_process,
	.ftm_process_extra          = tp_goto_sleep_ftm,
	.reset                      = ilitek_reset,
	.power_control              = ilitek_power_control,
	.get_chip_info              = ilitek_get_chip_info,
	.trigger_reason             = ilitek_trigger_reason,
	.get_touch_points           = ilitek_get_touch_points,
	.get_gesture_info           = ilitek_get_gesture_info,
	.mode_switch                = ilitek_mode_switch,
	.fw_check                   = ilitek_fw_check,
	.fw_update                  = ilitek_fw_update,
	.get_vendor                 = ilitek_get_vendor,
	.esd_handle                 = ilitek_esd_handle,
	.set_touch_direction        = ilitek_set_touch_direction,
	.get_touch_direction        = ilitek_get_touch_direction,
	.tp_queue_work_prepare      = ilitek_reset_queue_work_prepare,
	.tp_irq_throw_away          = ilitek_irq_throw_away,
};

static int ilitek_read_debug_data(struct seq_file *s,
				  struct ilitek_tddi_dev *chip_info,
				  u8 read_type)
{
	int ret;
	u8 tp_mode;
	u8 test_cmd[4] = { 0 };
	int i = 0;
	int j = 0;
	int xch = chip_info->xch_num;
	int ych = chip_info->ych_num;
	u8 *buf = NULL;

	buf = kzalloc(DEBUG_DATA_MAX_LENGTH, GFP_ATOMIC);

	if (ERR_ALLOC_MEM(buf)) {
		TPD_INFO("Failed to allocate packet memory, %ld\n", PTR_ERR(buf));
		return -1;
	}

	mutex_lock(&chip_info->touch_mutex);
	tp_mode = P5_X_FW_DEBUG_MODE;
	ret = ilitek_tddi_switch_mode(chip_info, &tp_mode);

	if (ret < 0) {
		TPD_INFO("Failed to switch debug mode\n");
		seq_printf(s, "get data failed\n");
		mutex_unlock(&chip_info->touch_mutex);
		tp_kfree((void **)&buf);
		return -1;
	}

	test_cmd[0] = 0xFA;
	test_cmd[1] = read_type;
	TPD_INFO("debug cmd 0x%X, 0x%X", test_cmd[0], test_cmd[1]);

	chip_info->need_judge_irq_throw = true;
	chip_info->irq_wake_up_state = false;

	ret = chip_info->write(chip_info, test_cmd, 2);
	chip_info->debug_node_open = true;

	/*mutex_unlock(&chip_info->ts->mutex);for common driver lock ts->mutex*/

	enable_irq(chip_info->irq_num);/*because oplus disable*/

	for (i = 0; i < 10; i++) {
		int rlen = 0;
		ilitek_check_wake_up_state(10);


		rlen = core_spi_check_read_size(chip_info);
		TPD_DEBUG("Packget length = %d\n", rlen);

		if (rlen < 0 || rlen >= DEBUG_DATA_MAX_LENGTH) {
			TPD_INFO("Length of packet is invaild\n");
			continue;
		}


		ret = core_spi_read_data_after_checksize(chip_info, buf, rlen);

		if (ret < 0) {
			TPD_INFO("Read report packet failed ret = %d\n", ret);
			continue;
		}

		if (buf[0] == 0xA7) {
			break;
		}

		chip_info->irq_wake_up_state = false;
	}

	chip_info->need_judge_irq_throw = false;
	/*chip_info->debug_node_open = false;*/
	disable_irq_nosync(chip_info->irq_num);

	switch (read_type) {
	case P5_X_FW_RAW_DATA_MODE:
		seq_printf(s, "raw_data:\n");
		break;

	case P5_X_FW_DELTA_DATA_MODE:
		seq_printf(s, "diff_data:\n");
		break;

	default:
		seq_printf(s, "read type not support\n");
		break;
	}

	if (i < 10) {
		for (i = 0; i < ych; i++) {
			seq_printf(s, "[%2d]", i);

			for (j = 0; j < xch; j++) {
				s16 temp;
				temp = (s16)((buf[(i * xch + j) * 2 + 35] << 8)
					     + buf[(i * xch + j) * 2 + 35 + 1]);
				seq_printf(s, "%5d,", temp);
			}

			seq_printf(s, "\n");
		}

		for (i = 0; i < MAX_TOUCH_NUM; i++) {
			if ((buf[(3 * i) + 5] != 0xFF) || (buf[(3 * i) + 6] != 0xFF)
					|| (buf[(3 * i) + 7] != 0xFF)) {
				break; /* has touch*/
			}
		}

		if (i == MAX_TOUCH_NUM) {
			tp_touch_btnkey_release(chip_info->tp_index);
		}

	} else {
		seq_printf(s, "get data failed\n");
	}

	/* change to demo mode */
	tp_mode = P5_X_FW_DEMO_MODE;
	ilitek_tddi_switch_mode(chip_info, &tp_mode);
	mutex_unlock(&chip_info->touch_mutex);
	tp_kfree((void **)&buf);
	return 0;
}

#ifdef CONFIG_OPLUS_TP_APK
static int ilitek_read_debug_diff(struct seq_file *s,
				  struct ilitek_tddi_dev *chip_info)
{
	int ret;
	int i = 0;
	int j = 0;
	int xch = chip_info->xch_num;
	int ych = chip_info->ych_num;
	u16 *debug_buf = NULL;
	mutex_lock(&chip_info->touch_mutex);
	TPD_INFO("Get data");
	ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 2);

	if (ret < 0) {
		TPD_INFO("cmd fail\n");
		goto out;
	}

	debug_buf = kzalloc(xch * ych * 2, GFP_KERNEL);

	if (debug_buf == NULL) {
		goto out;
	}

	ret = ilitek_tddi_get_tp_recore_data(chip_info, debug_buf, xch * ych * 2);

	if (ret < 0) {
		TPD_INFO("get data fail\n");
	}

	TPD_INFO("recore reset");
	ret = ilitek_tddi_ic_func_ctrl(chip_info, "tp_recore", 3);

	if (ret < 0) {
		TPD_INFO("cmd fail\n");
		goto out;
	}

	seq_printf(s, "debug finger down diff data:\n");

	for (i = 0; i < ych; i++) {
		seq_printf(s, "[%2d]", i);

		for (j = 0; j < xch; j++) {
			seq_printf(s, "%5d,", (s16)(debug_buf[(i * xch + j) ]));
		}

		seq_printf(s, "\n");
	}

out:
	tp_kfree((void **)&debug_buf);
	mutex_unlock(&chip_info->touch_mutex);
	return 0;
}
#endif /* end of CONFIG_OPLUS_TP_APK*/

static void ilitek_delta_read(struct seq_file *s, void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
	ilitek_read_debug_data(s, chip_info, P5_X_FW_DELTA_DATA_MODE);
#ifdef CONFIG_OPLUS_TP_APK

	if (chip_info->debug_mode_sta) {
		ilitek_read_debug_diff(s, chip_info);
	}

#endif /* end of CONFIG_OPLUS_TP_APK*/
}

static void ilitek_baseline_read(struct seq_file *s, void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	TPD_INFO("s->size = %d  s->count = %d\n", (int)s->size, (int)s->count);
	ilitek_read_debug_data(s, chip_info, P5_X_FW_RAW_DATA_MODE);
}

static void ilitek_main_register_read(struct seq_file *s, void *chip_data)
{
	TPD_INFO("\n");
}

static struct debug_info_proc_operations ilitek_debug_info_proc_ops = {
	.baseline_read = ilitek_baseline_read,
	.delta_read = ilitek_delta_read,
	.main_register_read = ilitek_main_register_read,
};

#ifdef CONFIG_OPLUS_TP_APK

static void ili_apk_game_set(void *chip_data, bool on_off)
{
	struct ilitek_tddi_dev *chip_info;

	chip_info = (struct ilitek_tddi_dev *)chip_data;
	ilitek_mode_switch(chip_data, MODE_GAME, on_off);
	chip_info->lock_point_status = on_off;
}

static bool ili_apk_game_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	return chip_info->lock_point_status;
}

static void ili_apk_debug_set(void *chip_data, bool on_off)
{
	u8 cmd[1];
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;


	if (on_off) {
		cmd[0] = P5_X_FW_DEMO_DEBUG_INFO_MODE;
		ilitek_tddi_switch_mode(chip_info, cmd);
		ilitek_tddi_get_tp_recore_ctrl(chip_info, 1);

	} else {
		cmd[0] = P5_X_FW_DEMO_MODE;
		ilitek_tddi_switch_mode(chip_info, cmd);
		ilitek_tddi_get_tp_recore_ctrl(chip_info, 0);
	}

	chip_info->debug_mode_sta = on_off;
}

static bool ili_apk_debug_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;

	return chip_info->debug_mode_sta;
}

static void ili_apk_gesture_debug(void *chip_data, bool on_off)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	/*get_gesture_fail_reason(on_off);*/
	chip_info->debug_gesture_sta = on_off;
}

static bool  ili_apk_gesture_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	return chip_info->debug_gesture_sta;
}

static int  ili_apk_gesture_info(void *chip_data, char *buf, int len)
{
	int ret = 0;
	int i;
	int num;
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	if (len < 2) {
		return 0;
	}

	buf[0] = 255;

	if (chip_info->ts->gesture_buf[0] == 0xAA) {
		switch (chip_info->ts->gesture_buf[1]) {   /*judge gesture type*/
		case GESTURE_RIGHT :
			buf[0]  = LEFT2RIGHT_SWIP;
			break;

		case GESTURE_LEFT :
			buf[0]  = RIGHT2LEFT_SWIP;
			break;

		case GESTURE_DOWN  :
			buf[0]  = UP2DOWN_SWIP;
			break;

		case GESTURE_UP :
			buf[0]  = DOWN2UP_SWIP;
			break;

		case GESTURE_DOUBLECLICK:
			buf[0]  = DOU_TAP;
			break;

		case GESTURE_V :
			buf[0]  = UP_VEE;
			break;

		case GESTURE_V_DOWN :
			buf[0]  = DOWN_VEE;
			break;

		case GESTURE_V_LEFT :
			buf[0]  = LEFT_VEE;
			break;

		case GESTURE_V_RIGHT :
			buf[0]  = RIGHT_VEE;
			break;

		case GESTURE_O  :
			buf[0] = CIRCLE_GESTURE;

			break;

		case GESTURE_M  :
			buf[0]  = M_GESTRUE;
			break;

		case GESTURE_W :
			buf[0]  = W_GESTURE;
			break;

		case GESTURE_TWOLINE_DOWN :
			buf[0]  = DOU_SWIP;
			break;

		default:
			buf[0] = UNKOWN_GESTURE;
			break;
		}

	} else if (chip_info->ts->gesture_buf[0] == 0xAE) {
		buf[0] = chip_info->ts->gesture_buf[1] + 128;
	}

	/*buf[0] = gesture_buf[0];*/
	num = chip_info->ts->gesture_buf[35];

	if (num > 40) {
		num = 40;
	}

	ret = 2;
	buf[1] = num;

	for (i = 0; i < num; i++) {
		int x;
		int y;
		x = (chip_info->ts->gesture_buf[40 + i * 3] & 0xF0) << 4;
		x = x | (chip_info->ts->gesture_buf[40 + i * 3 + 1]);
		x = x * (chip_info->resolution_x) / TPD_WIDTH;

		y = (chip_info->ts->gesture_buf[40 + i * 3] & 0x0F) << 8;
		y = y | chip_info->ts->gesture_buf[40 + i * 3 + 2];
		y = y * (chip_info->resolution_y) / TPD_HEIGHT;

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


static void ili_apk_earphone_set(void *chip_data, bool on_off)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	ilitek_mode_switch(chip_data, MODE_HEADSET, on_off);
	chip_info->earphone_sta = on_off;
}

static bool ili_apk_earphone_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	return chip_info->earphone_sta;
}

static void ili_apk_charger_set(void *chip_data, bool on_off)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	ilitek_mode_switch(chip_data, MODE_CHARGE, on_off);
	chip_info->plug_status = on_off;
}

static bool ili_apk_charger_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;

	return chip_info->plug_status;
}

static void ili_apk_noise_set(void *chip_data, bool on_off)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;
	/*ilitek_mode_switch(chip_data, MODE_CHARGE, on_off);*/
	ilitek_tddi_ic_func_ctrl(chip_info, "freq_scan", on_off);

	chip_info->noise_sta = on_off;
}

static bool ili_apk_noise_get(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;

	return chip_info->noise_sta;
}


static int  ili_apk_tp_info_get(void *chip_data, char *buf, int len)
{
	int ret;
	struct ilitek_tddi_dev *chip_info;
	chip_info = (struct ilitek_tddi_dev *)chip_data;

	ret = snprintf(buf, len, "IC:ILI%04X\nFW_VER:0x%02X\nCH:%dX%d\n",
		       chip_info->chip->id,
		       chip_info->chip->fw_ver >> 8 & 0xFF,
		       chip_info->hw_res->tx_num,
		       chip_info->hw_res->rx_num);

	if (ret > len) {
		ret = len;
	}

	return ret;
}

static void ili_init_oplus_apk_op(struct touchpanel_data *ts)
{
	ts->apk_op = kzalloc(sizeof(APK_OPERATION), GFP_KERNEL);

	if (ts->apk_op) {
		ts->apk_op->apk_game_set = ili_apk_game_set;
		ts->apk_op->apk_game_get = ili_apk_game_get;
		ts->apk_op->apk_debug_set = ili_apk_debug_set;
		ts->apk_op->apk_debug_get = ili_apk_debug_get;
		/*apk_op->apk_proximity_set = ili_apk_proximity_set;*/
		/*apk_op->apk_proximity_dis = ili_apk_proximity_dis;*/
		ts->apk_op->apk_noise_set = ili_apk_noise_set;
		ts->apk_op->apk_noise_get = ili_apk_noise_get;
		ts->apk_op->apk_gesture_debug = ili_apk_gesture_debug;
		ts->apk_op->apk_gesture_get = ili_apk_gesture_get;
		ts->apk_op->apk_gesture_info = ili_apk_gesture_info;
		ts->apk_op->apk_earphone_set = ili_apk_earphone_set;
		ts->apk_op->apk_earphone_get = ili_apk_earphone_get;
		ts->apk_op->apk_charger_set = ili_apk_charger_set;
		ts->apk_op->apk_charger_get = ili_apk_charger_get;
		ts->apk_op->apk_tp_info_get = ili_apk_tp_info_get;
		/*apk_op->apk_data_type_set = ili_apk_data_type_set;*/
		/*apk_op->apk_rawdata_get = ili_apk_rawdata_get;*/
		/*apk_op->apk_diffdata_get = ili_apk_diffdata_get;*/
		/*apk_op->apk_basedata_get = ili_apk_basedata_get;*/
		/*ts->apk_op->apk_backdata_get = ili_apk_backdata_get;*/
		/*apk_op->apk_debug_info = ili_apk_debug_info;*/

	} else {
		TPD_INFO("Can not kzalloc apk op.\n");
	}
}
#endif /* end of CONFIG_OPLUS_TP_APK*/

static int ilitek_alloc_global_data(void)
{
	idev = kzalloc(sizeof(*idev), GFP_KERNEL);

	if (ERR_ALLOC_MEM(idev)) {
		TPD_INFO("Failed to allocate chip_info memory, %ld\n", PTR_ERR(idev));
		idev = NULL;
		return -ENOMEM;
	}

	idev->fw_buf_dma = kzalloc(MAX_FW_BUF_SIZE, GFP_KERNEL | GFP_DMA);

	if (idev->fw_buf_dma == NULL) {
		TPD_INFO("fw kzalloc error\n");
		/*ret = -ENOMEM;*/
		return -ENOMEM;
	}

	idev->core_mp = (struct core_mp_test_data *)kzalloc(sizeof(
				struct core_mp_test_data), GFP_KERNEL);

	if (idev->core_mp == NULL) {
		TPD_INFO("core_mp kzalloc error\n");
		/*ret = -ENOMEM;*/
		return -ENOMEM;
	}

	idev->chip = kzalloc(sizeof(struct ilitek_ic_info), GFP_KERNEL);

	if (!idev->chip) {
		return  -ENOMEM;
	}

	idev->gestrue_fw_size = 10 * K;
	idev->gestrue_fw = kzalloc(idev->gestrue_fw_size, GFP_KERNEL);

	if (!idev->gestrue_fw) {
		return  -ENOMEM;
	}

	idev->g_user_buf_size = USER_STR_BUFF;
	idev->g_user_buf = kzalloc(idev->g_user_buf_size, GFP_KERNEL);

	if (!idev->g_user_buf) {
		return  -ENOMEM;
	}

	idev->tfd = kzalloc(sizeof(struct touch_fw_data), GFP_KERNEL);

	if (!idev->tfd) {
		return  -ENOMEM;
	}

	return 0;
}

static void ilitek_free_global_data(void)
{
	tp_kfree((void **)&idev->g_user_buf);
	tp_kfree((void **)&idev->gestrue_fw);
	tp_kfree((void **)&idev->chip);
	tp_kfree((void **)&idev->core_mp);
	tp_kfree((void **)&idev);
}

static void ilitek_mutex_atomic_init(void *chip_data)
{
	struct ilitek_tddi_dev *chip_info = (struct ilitek_tddi_dev *)chip_data;

	mutex_init(&chip_info->touch_mutex);
	mutex_init(&chip_info->debug_mutex);/*for ili apk debug*/
	mutex_init(&chip_info->debug_read_mutex);/*for ili apk debug*/
	init_waitqueue_head(&(chip_info->inq));/*for ili apk debug*/
	spin_lock_init(&chip_info->irq_spin);

	atomic_set(&chip_info->irq_stat, DISABLE);
	atomic_set(&chip_info->ice_stat, DISABLE);
	atomic_set(&chip_info->tp_reset, END);
	atomic_set(&chip_info->fw_stat, END);
	atomic_set(&chip_info->mp_stat, DISABLE);
	atomic_set(&chip_info->tp_sleep, END);
	atomic_set(&chip_info->mp_int_check, DISABLE);
	atomic_set(&chip_info->esd_stat, END);
	atomic_set(&chip_info->irq_stat, ENABLE);
}

int  ilitek_platform_probe(struct spi_device *spi)
{
	int ret = 0;
	struct touchpanel_data *ts = NULL;
	struct ilitek_tddi_dev *chip_info = NULL;
	u64 time_counter = 0;

	if (!spi) {
		TPD_INFO("spi device is NULL\n");
		return -ENODEV;
	}

	reset_healthinfo_time_counter(&time_counter);

	TPD_INFO("platform probe\n");

	/*step1:Alloc chip_info*/
	if (ilitek_alloc_global_data() < 0) {
		ret = -ENOMEM;
		goto err_out;
	}

	chip_info = idev;

	/*step2:Alloc common ts*/
	ts = common_touch_data_alloc();

	if (ts == NULL) {
		TPD_INFO("ts kzalloc error\n");
		ret = -ENOMEM;
		goto err_out;
	}

	memset(ts, 0, sizeof(*ts));



	chip_info->ts = ts;
	/*step3:spi function init*/
	chip_info->spi = spi;
	chip_info->dev = &spi->dev;
	chip_info->write = ilitek_spi_write;
	chip_info->read = ilitek_spi_read;

	chip_info->actual_tp_mode = P5_X_FW_DEMO_MODE;

	chip_info->gesture_mode = P5_X_FW_GESTURE_INFO_MODE;
	chip_info->report = ENABLE;
	chip_info->netlink = DISABLE;
	chip_info->debug_node_open = DISABLE;
	chip_info->already_upgrade = false;
	chip_info->monitor_data = &ts->monitor_data;

	core_spi_setup(chip_info, SPI_CLK);

	/*step4:ts function init*/
	ts->debug_info_ops = &ilitek_debug_info_proc_ops;
	ts->s_client = spi;
	ts->irq = spi->irq;

	spi_set_drvdata(spi, ts);

	ts->dev = &spi->dev;
	ts->chip_data = chip_info;

	chip_info->hw_res = &ts->hw_res;
	/*chip_info->tp_type = TP_AUO;*/

	/*---prepare for spi parameter---*/
	if (ts->s_client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		printk("Full duplex not supported by master\n");
		ret = -EIO;
		goto err_out;
	}

	ts->ts_ops = &ilitek_ops;
	ts->engineer_ops = &ilitek_9881_engineer_test_ops;
	ts->com_test_data.chip_test_ops = &ilitek_9881_test_ops;

	/*need before ftm mode*/
	ilitek_mutex_atomic_init(chip_info);

	/*set ic info flag*/
	chip_info->get_ic_info_flag = false;

	/*9881h ic init*/
	ilitek_tddi_ic_init(chip_info);

	/*get ftm firmware ini from touch.h*/
	chip_info->p_firmware_headfile = &ts->panel_data.firmware_headfile;

	/*register common touch device*/
	ret = register_common_touch_device(ts);

	if (ret < 0) {
		TPD_INFO("\n");
		goto abnormal_register_driver;
	}

	/*get ts tp_index to chip info*/
	chip_info->tp_index = ts->tp_index;
	/*idev[chip_info->tp_index] = chip_info;*/

	/* update fw in probe*/
#ifdef CONFIG_TOUCHPANEL_MTK_PLATFORM

	if (ts->boot_mode == RECOVERY_BOOT
			|| is_oem_unlocked() || ts->fw_update_in_probe_with_headfile) {
		ilitek_fw_update((void *)chip_info, NULL, false);
	}

#else

	if (ts->boot_mode == MSM_BOOT_MODE__RECOVERY
			|| is_oem_unlocked() || ts->fw_update_in_probe_with_headfile) {
		ilitek_fw_update((void *)chip_info, NULL, false);
	}

#endif


	ts->tp_suspend_order = TP_LCD_SUSPEND;
	ts->tp_resume_order = LCD_TP_RESUME;

	/*get default info from dts*/
	chip_info->irq_num = ts->irq;
	chip_info->fw_name = ts->panel_data.fw_name;
	chip_info->test_limit_name = ts->panel_data.test_limit_name;
	chip_info->resolution_x = ts->resolution_info.max_x;
	chip_info->resolution_y = ts->resolution_info.max_y;


	/*for ili apk debug node*/
	ilitek_tddi_node_init(chip_info);
#ifdef CONFIG_OPLUS_TP_APK
	chip_info->demo_debug_info[0] = ilitek_tddi_demo_debug_info_id0;
#endif /* end of CONFIG_OPLUS_TP_APK*/

#ifdef CONFIG_OPLUS_TP_APK
	ili_init_oplus_apk_op(ts);
#endif /* end of CONFIG_OPLUS_TP_APK*/

	if (ts->esd_handle_support) {
		ts->esd_info.esd_work_time = msecs_to_jiffies(WQ_ESD_DELAY);
		TPD_INFO("%s:change esd handle time to %d s\n",
			 __func__,
			 ts->esd_info.esd_work_time / HZ);
	}

	/*for gesture ws*/
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 190))
	chip_info->gesture_process_ws = wakeup_source_register("gesture_wake_lock");
#else
	chip_info->gesture_process_ws = wakeup_source_register(NULL,
					"gesture_wake_lock");
#endif

	if (!chip_info->gesture_process_ws) {
		TPD_INFO("gesture_process_ws request failed\n");
		ret = -EIO;
		goto err_out;
	}

	if (ts->health_monitor_support) {
		tp_healthinfo_report(&ts->monitor_data, HEALTH_PROBE, &time_counter);
	}

	ipio_debug_level = 0;
	return 0;

abnormal_register_driver:

	if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
			|| ts->boot_mode == MSM_BOOT_MODE__RF
			|| ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 190))
		chip_info->gesture_process_ws = wakeup_source_register("gesture_wake_lock");
#else
		chip_info->gesture_process_ws = wakeup_source_register(NULL,
						"gesture_wake_lock");
#endif
		TPD_INFO("ftm mode probe end ok\n");
		return 0;
	}

err_out:
	spi_set_drvdata(spi, NULL);
	TPD_INFO("err_spi_setup end\n");
	common_touch_data_free(ts);
	ilitek_free_global_data();
	return ret;
}

int  ilitek_platform_remove(struct spi_device *spi)
{
	struct touchpanel_data *ts = spi_get_drvdata(spi);

	TPD_INFO();

	wakeup_source_unregister(idev->gesture_process_ws);
	ilitek_tddi_node_exit(idev);
	ilitek_free_global_data();
	spi_set_drvdata(spi, NULL);

	if (ts) {
		kfree(ts);
		ts = NULL;
	}

	return 0;
}

static int ilitek_spi_resume(struct device *dev)
{
	struct touchpanel_data *ts = dev_get_drvdata(dev);

	TPD_INFO("%s is called\n", __func__);

	if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
			|| ts->boot_mode == MSM_BOOT_MODE__RF
			|| ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
		TPD_INFO("ilitek_spi_resume do nothing in ftm\n");
		return 0;
	}

	tp_pm_resume(ts);

	return 0;
}

static int ilitek_spi_suspend(struct device *dev)
{
	struct touchpanel_data *ts = dev_get_drvdata(dev);

	TPD_INFO("%s: is called\n", __func__);

	if ((ts->boot_mode == MSM_BOOT_MODE__FACTORY
			|| ts->boot_mode == MSM_BOOT_MODE__RF
			|| ts->boot_mode == MSM_BOOT_MODE__WLAN)) {
		TPD_INFO("ilitek_spi_suspend do nothing in ftm\n");
		return 0;
	}

	tp_pm_suspend(ts);

	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_spi_suspend,
	.resume = ilitek_spi_resume,
};

/*
 * The name in the table must match the definiation
 * in a dts file.
 *
 */
static struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

static struct spi_driver tp_spi_driver = {
	.driver = {
		.name    = DEVICE_ID,
		.owner = THIS_MODULE,
		.of_match_table = tp_match_table,
		.pm = &tp_pm_ops,
	},
	.probe = ilitek_platform_probe,
	.remove = ilitek_platform_remove,
};

int __init tp_driver_init_ili_9881h(void)
{
	int res = 0;

	TPD_INFO("%s is called\n", __func__);

	if (!tp_judge_ic_match(TPD_DEVICE)) {
		TPD_INFO("TP driver is already register\n");
		return -1;
	}

	res = spi_register_driver(&tp_spi_driver);

	if (res < 0) {
		TPD_INFO("Failed to add spi driver\n");
		return -ENODEV;
	}

	TPD_INFO("Succeed to add driver\n");
	return res;
}

void __exit tp_driver_exit_ili_9881h(void)
{
	spi_unregister_driver(&tp_spi_driver);
}

#ifdef CONFIG_TOUCHPANEL_LATE_INIT
late_initcall(tp_driver_init_ili_9881h);
#else
module_init(tp_driver_init_ili_9881h);
#endif
module_exit(tp_driver_exit_ili_9881h);

MODULE_DESCRIPTION("Touchscreen ilitek9881h Driver");
MODULE_LICENSE("GPL");
