// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/delay.h>
#include "ft3518_core.h"

/*******Part0:LOG TAG Declear********************/

#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "ft3518-test"
#else
#define TPD_DEVICE "ft3518-test"
#endif

#define FTS_TEST_FUNC_ENTER() do {\
	TPD_INFO("[FTS_TS][TEST]%s: Enter\n", __func__);\
} while (0)

#define FTS_TEST_FUNC_EXIT()  do {\
	TPD_INFO("[FTS_TS][TEST]%s: Exit(%d)\n", __func__, __LINE__);\
} while (0)

#define FTS_TEST_SAVE_INFO(fmt, args...) do {\
	if (g_fts_data->s) {\
        seq_printf(g_fts_data->s, fmt, ##args);\
	}\
} while (0)

#define FTS_TEST_SAVE_ERR(fmt, args...)  do {\
	if (g_fts_data->s) {\
        seq_printf(g_fts_data->s, fmt, ##args);\
	}\
	TPD_INFO(fmt, ##args);\
} while (0)

enum wp_type {
	WATER_PROOF_OFF = 0,
	WATER_PROOF_ON = 1,
	WATER_PROOF_ON_TX,
	WATER_PROOF_ON_RX,
	WATER_PROOF_OFF_TX,
	WATER_PROOF_OFF_RX,
};

enum byte_mode {
	DATA_ONE_BYTE,
	DATA_TWO_BYTE,
};

enum normalize_type {
	NORMALIZE_OVERALL,
	NORMALIZE_AUTO,
};


#define MAX_LENGTH_TEST_NAME            64


static void sys_delay(int ms)
{
	msleep(ms);
}

int focal_abs(int value)
{
	if (value < 0) {
		value = 0 - value;
	}

	return value;
}

static void print_buffer(int *buffer, int length, int line_num)
{
	int i = 0;
	int j = 0;
	int tmpline = 0;
	char *tmpbuf = NULL;
	int tmplen = 0;
	int cnt = 0;

	if ((NULL == buffer) || (length <= 0)) {
		TPD_INFO("buffer/length(%d) fail", length);
		return;
	}

	tmpline = line_num ? line_num : length;
	tmplen = tmpline * 6 + 128;
	tmpbuf = kzalloc(tmplen, GFP_KERNEL);

	if (!tmpbuf) {
		TPD_INFO("%s, alloc failed \n", __func__);
		return;
	}

	for (i = 0; i < length; i = i + tmpline) {
		cnt = 0;

		for (j = 0; j < tmpline; j++) {
			cnt += snprintf(tmpbuf + cnt, tmplen - cnt, "%5d ", buffer[i + j]);

			if ((cnt >= tmplen) || ((i + j + 1) >= length)) {
				break;
			}
		}

		TPD_DEBUG("%s", tmpbuf);
	}

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
}


#define NODE_MATCH      1
#define CHANNEL_MATCH   2
#define CHEN_MATCH      3
int ft3518_output_data(int *buffer, struct chip_data_ft3518 *ts_data,
		       struct auto_testdata *focal_testdata, int limit_type)
{
	uint8_t data_buf[64];
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int i = 0;
	int num_each_line = 0;
	int data_volumn = 0;

	if (limit_type == NODE_MATCH) {
		num_each_line = rx_num;
		data_volumn = rx_num * tx_num;

	} else if (limit_type == CHANNEL_MATCH) {
		num_each_line = rx_num + tx_num;
		data_volumn = (rx_num + tx_num) * 2;
	} else if (limit_type == CHEN_MATCH) {
		num_each_line = rx_num;
		data_volumn = rx_num + tx_num;
	}

	memset(data_buf, 0, sizeof(data_buf));

	for (i = 0; i < data_volumn; i += 1) {
		snprintf(data_buf, 64, "%d,", buffer[i]);
		tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
			      strlen(data_buf), focal_testdata->pos);

		if (!((i + 1) % num_each_line) || (i == data_volumn - 1)) {
			snprintf(data_buf, 64, "\n");
			tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
				      strlen(data_buf), focal_testdata->pos);
		}
	}

	return 0;
}

/********************************************************************
 * test read/write interface
 *******************************************************************/
static int fts_test_bus_read(u8 *cmd, int cmdlen, u8 *data, int datalen)
{
	int ret = 0;
	unsigned char *read_buf = NULL;
	unsigned char *write_buf = NULL;

	read_buf = (u8 *)kzalloc(datalen * sizeof(u8), GFP_KERNEL);

	if (NULL == read_buf) {
		FTS_TEST_SAVE_ERR("mass read_buf buffer malloc fail\n");
		return -ENOMEM;
	}

	write_buf = (u8 *)kzalloc(cmdlen * sizeof(u8), GFP_KERNEL);

	if (NULL == write_buf) {
		FTS_TEST_SAVE_ERR("mass write_buf buffer malloc fail\n");
		ret = -ENOMEM;
		goto malloc_fail;
	}

	memcpy(write_buf, cmd, cmdlen);

	ret = touch_i2c_read(g_fts_data->client, (char *)write_buf, cmdlen,
			     (char *)read_buf, datalen);
	memcpy(data, read_buf, datalen);

	kfree(write_buf);
malloc_fail:
	kfree(read_buf);

	if (ret < 0) {
		return ret;

	} else {
		return 0;
	}
}

static int fts_test_bus_write(u8 *writebuf, int writelen)
{
	int ret = 0;

	ret = touch_i2c_write_block(g_fts_data->client, writebuf[0], writelen - 1,
				    &writebuf[1]);

	if (ret < 0) {
		return ret;

	} else {
		return 0;
	}
}

static int fts_test_read_reg(u8 addr, u8 *val)
{
	int ret = 0;

	ret = touch_i2c_read_block(g_fts_data->client, addr, 1, val);

	if (ret < 0) {
		return ret;

	} else {
		return 0;
	}
}

static int fts_test_write_reg(u8 addr, u8 val)
{
	int ret;
	u8 cmd[2] = {0};

	cmd[0] = addr;
	cmd[1] = val;
	ret = fts_test_bus_write(cmd, 2);

	return ret;
}

static int fts_test_read(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;

	if (packet_remainder) {
		packet_num++;
	}

	if (byte_num < BYTES_PER_TIME) {
		packet_length = byte_num;

	} else {
		packet_length = BYTES_PER_TIME;
	}

	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	ret = fts_test_bus_read(&addr, 1, &readbuf[offset], packet_length);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read buffer fail\n");
		return ret;
	}

	for (i = 1; i < packet_num; i++) {
		offset += packet_length;

		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}


		ret = fts_test_bus_read(NULL, 0, &readbuf[offset],
					packet_length);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read buffer fail\n");
			return ret;
		}
	}

	return 0;
}

static int fts_test_write(u8 addr, u8 *writebuf, int writelen)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = writelen;

	data = kzalloc(BYTES_PER_TIME + 1, GFP_KERNEL);

	if (!data) {
		FTS_TEST_SAVE_ERR("malloc memory for bus write data fail\n");
		return -ENOMEM;
	}

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;

	if (packet_remainder) {
		packet_num++;
	}

	if (byte_num < BYTES_PER_TIME) {
		packet_length = byte_num;

	} else {
		packet_length = BYTES_PER_TIME;
	}

	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	data[0] = addr;

	for (i = 0; i < packet_num; i++) {
		if (i != 0) {
			data[0] = addr + 1;
		}

		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}

		memcpy(&data[1], &writebuf[offset], packet_length);

		ret = fts_test_bus_write(data, packet_length + 1);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write buffer fail\n");
			kfree(data);
			return ret;
		}

		offset += packet_length;
	}

	kfree(data);
	return 0;
}

/*
 * read_mass_data - read rawdata/short test data
 * addr - register addr which read data from
 * byte_num - read data length, unit:byte
 * buf - save data
 *
 * return 0 if read data succuss, otherwise return error code
 */
static int read_mass_data(u8 addr, int byte_num, int *buf)
{
	int ret = 0;
	int i = 0;
	u8 *data = NULL;

	data = (u8 *)kzalloc(byte_num * sizeof(u8), GFP_KERNEL);

	if (NULL == data) {
		FTS_TEST_SAVE_ERR("mass data buffer malloc fail\n");
		return -ENOMEM;
	}

	/* read rawdata buffer */
	TPD_INFO("mass data len:%d", byte_num);
	ret = fts_test_read(addr, data, byte_num);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read mass data fail\n");
		goto read_massdata_err;
	}

	for (i = 0; i < byte_num; i = i + 2) {
		buf[i >> 1] = (int)(short)((data[i] << 8) + data[i + 1]);
	}

	ret = 0;
read_massdata_err:
	kfree(data);
	return ret;
}


/********************************************************************
 * test global function enter work/factory mode
 *******************************************************************/
static int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	TPD_INFO("%s +\n", __func__);
	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);

	if ((ret >= 0) && (0x00 == mode)) {
		return 0;
	}

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, 0x00);

		if (ret >= 0) {
			sys_delay(FACTORY_TEST_DELAY);

			for (j = 0; j < 20; j++) {
				ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);

				if ((ret >= 0) && (0x00 == mode)) {
					TPD_INFO("enter work mode success");
					return 0;

				} else {
					sys_delay(FACTORY_TEST_DELAY);
				}
			}
		}

		sys_delay(50);
	}

	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_SAVE_ERR("Enter work mode fail\n");
		return -EIO;
	}

	TPD_INFO("%s -\n", __func__);
	return 0;
}


static int fts_special_operation_for_samsung(struct chip_data_ft3518 *ts_data)
{
	int ret = 0;

	if (true ==
			ts_data->use_panelfactory_limit) {                      /*only for firmware released to samsung factory*/
		ret = fts_test_write_reg(FTS_REG_SAMSUNG_SPECIFAL, 0x01);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write FTS_REG_SAMSUNG_SPECIFAL fail at %s,ret=%d\n", ret,
					  __func__);
			return -EIO;
		}
	}

	return ret;
}


#define FTS_FACTORY_MODE 0x40
static int enter_factory_mode(struct chip_data_ft3518 *ts_data)
{
	int ret = 0;
	u8 mode = 0;
	int i = 0;
	int j = 0;

	ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);

	if ((ret >= 0) && (FTS_FACTORY_MODE == mode)) {
		fts_special_operation_for_samsung(ts_data);
		return 0;
	}

	for (i = 0; i < ENTER_WORK_FACTORY_RETRIES; i++) {
		ret = fts_test_write_reg(DEVIDE_MODE_ADDR, 0x40);

		if (ret >= 0) {
			sys_delay(FACTORY_TEST_DELAY);

			for (j = 0; j < 20; j++) {
				ret = fts_test_read_reg(DEVIDE_MODE_ADDR, &mode);

				if ((ret >= 0) && (FTS_FACTORY_MODE == mode)) {
					TPD_INFO("enter factory mode success");
					sys_delay(200);
					fts_special_operation_for_samsung(ts_data);
					return 0;

				} else {
					sys_delay(FACTORY_TEST_DELAY);
				}
			}
		}

		sys_delay(50);
	}

	if (i >= ENTER_WORK_FACTORY_RETRIES) {
		FTS_TEST_SAVE_ERR("Enter factory mode fail\n");
		return -EIO;
	}

	fts_special_operation_for_samsung(ts_data);
	return 0;
}

static int get_channel_num(struct chip_data_ft3518 *ts_data)
{
	int ret = 0;
	u8 tx_num = 0;
	u8 rx_num = 0;

	ret = fts_test_read_reg(FACTORY_REG_CHX_NUM, &tx_num);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read tx_num register fail\n");
		return ret;
	}

	ret = fts_test_read_reg(FACTORY_REG_CHY_NUM, &rx_num);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rx_num register fail\n");
		return ret;
	}

	if ((tx_num != ts_data->hw_res->tx_num)
			|| (rx_num != ts_data->hw_res->rx_num)) {
		FTS_TEST_SAVE_ERR("channel num check fail, tx_num:%d-%d, rx_num:%d-%d\n",
				  tx_num, ts_data->hw_res->tx_num,
				  rx_num, ts_data->hw_res->rx_num);
		return -EIO;
	}

	return 0;
}

static int read_rawdata(u8 off_addr, u8 off_val, u8 rawdata_addr, int byte_num,
			int *data)
{
	int ret = 0;

	/* set line addr or rawdata start addr */
	ret = fts_test_write_reg(off_addr, off_val);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("wirte line/start addr fail\n");
		return ret;
	}

	ret = read_mass_data(rawdata_addr, byte_num, data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		return ret;
	}

	return 0;
}

static int start_scan(void)
{
	int ret = 0;
	u8 addr = 0;
	u8 val = 0;
	u8 finish_val = 0;
	int times = 0;

	addr = DEVIDE_MODE_ADDR;
	val = 0xC0;
	finish_val = 0x40;

	/* write register to start scan */
	ret = fts_test_write_reg(addr, val);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write start scan mode fail\n");
		return ret;
	}

	/* Wait for the scan to complete */
	while (times++ < FACTORY_TEST_RETRY) {
		sys_delay(FACTORY_TEST_DELAY);

		ret = fts_test_read_reg(addr, &val);

		if ((ret >= 0) && (val == finish_val)) {
			break;

		} else {
			TPD_INFO("reg%x=%x,retry:%d", addr, val, times);
		}
	}

	if (times >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("scan timeout\n");
		return -EIO;
	}

	return 0;
}

static bool get_fw_wp(u8 wp_ch_sel, enum wp_type water_proof_type)
{
	bool fw_wp_state = false;

	switch (water_proof_type) {
	case WATER_PROOF_ON:
		/* bit5: 0-check in wp on, 1-not check */
		fw_wp_state = !(wp_ch_sel & 0x20);
		break;

	case WATER_PROOF_ON_TX:
		/* Bit6:  0-check Rx+Tx in wp mode  1-check one channel
		   Bit2:  0-check Tx in wp mode;  1-check Rx in wp mode
		*/
		fw_wp_state = (!(wp_ch_sel & 0x40) || !(wp_ch_sel & 0x04));
		break;

	case WATER_PROOF_ON_RX:
		fw_wp_state = (!(wp_ch_sel & 0x40) || (wp_ch_sel & 0x04));
		break;

	case WATER_PROOF_OFF:
		/* bit7: 0-check in wp off, 1-not check */
		fw_wp_state = !(wp_ch_sel & 0x80);
		break;

	case WATER_PROOF_OFF_TX:
		/* Bit1-0:  00-check Tx in non-wp mode
		            01-check Rx in non-wp mode
		            10:check Rx+Tx in non-wp mode
		*/
		fw_wp_state = ((0x0 == (wp_ch_sel & 0x03)) || (0x02 == (wp_ch_sel & 0x03)));
		break;

	case WATER_PROOF_OFF_RX:
		fw_wp_state = ((0x01 == (wp_ch_sel & 0x03)) || (0x02 == (wp_ch_sel & 0x03)));
		break;

	default:
		break;
	}

	return fw_wp_state;
}

static int get_cb_sc(int byte_num, int *cb_buf, enum byte_mode mode)
{
	int ret = 0;
	int i = 0;
	int read_num = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	u8 cb_addr = 0;
	u8 off_addr = 0;
	u8 *cb = NULL;

	cb = (u8 *)kzalloc(byte_num * sizeof(u8), GFP_KERNEL);

	if (!cb) {
		FTS_TEST_SAVE_ERR("malloc memory for cb buffer fail\n");
		return -ENOMEM;
	}

	cb_addr = FACTORY_REG_MC_SC_CB_ADDR;
	off_addr = FACTORY_REG_MC_SC_CB_ADDR_OFF;

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;

	if (packet_remainder) {
		packet_num++;
	}

	read_num = BYTES_PER_TIME;
	offset = 0;

	TPD_INFO("cb packet:%d,remainder:%d", packet_num, packet_remainder);

	for (i = 0; i < packet_num; i++) {
		if ((i == (packet_num - 1)) && packet_remainder) {
			read_num = packet_remainder;
		}

		ret = fts_test_write_reg(off_addr, offset);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write cb addr offset fail\n");
			goto cb_err;
		}

		ret = fts_test_read(cb_addr, cb + offset, read_num);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read cb fail\n");
			goto cb_err;
		}

		offset += read_num;
	}

	if (DATA_ONE_BYTE == mode) {
		for (i = 0; i < byte_num; i++) {
			cb_buf[i] = cb[i];
		}

	} else if (DATA_TWO_BYTE == mode) {
		for (i = 0; i < byte_num; i = i + 2) {
			cb_buf[i >> 1] = (int)(((int)(cb[i]) << 8) + cb[i + 1]);
		}
	}

	ret = 0;
cb_err:
	kfree(cb);
	return ret;
}

static int get_cb_mc_sc(u8 wp, int byte_num, int *cb_buf, enum byte_mode mode)
{
	int ret = 0;

	/* 1:waterproof 0:non-waterproof */
	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE, wp);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get mc_sc mode fail\n");
		return ret;
	}

	/* read cb */
	ret = get_cb_sc(byte_num, cb_buf, mode);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get sc cb fail\n");
		return ret;
	}

	return 0;
}

static int get_rawdata_mc_sc(enum wp_type wp, int byte_num, int *data)
{
	int ret = 0;
	u8 val = 0;
	u8 addr = 0;
	u8 rawdata_addr = 0;

	addr = FACTORY_REG_LINE_ADDR;
	rawdata_addr = FACTORY_REG_RAWDATA_ADDR_MC_SC;

	if (WATER_PROOF_ON == wp) {
		val = 0xAC;

	} else {
		val = 0xAB;
	}

	ret = read_rawdata(addr, val, rawdata_addr, byte_num, data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read rawdata fail\n");
		return ret;
	}

	return 0;
}

static bool compare_mc_sc(struct chip_data_ft3518 *ts_data, bool tx_check,
			  bool rx_check, int *data, int *min, int *max)
{
	int i = 0;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int channel_num = tx_num + rx_num;
	bool result = true;

	if (rx_check) {
		for (i = 0; i < rx_num; i++) {
			if (0 == ts_data->mpt.thr.node_valid_sc[i]) {
				continue;
			}

			if ((data[i] < min[i]) || (data[i] > max[i])) {
				TPD_INFO("rx check ERR [%d]: [%d] > [%d] > [%d] \n", i, max[i], data[i],
					 min[i]);
				FTS_TEST_SAVE_ERR("test fail,rx%d=%5d,range=(%5d,%5d)\n",
						  i + 1, data[i], min[i], max[i]);
				result = false;
			}
		}
	}

	if (tx_check) {
		for (i = rx_num; i < channel_num; i++) {
			if (0 == ts_data->mpt.thr.node_valid_sc[i]) {
				continue;
			}

			if ((data[i] < min[i]) || (data[i] > max[i])) {
				TPD_INFO("tx check ERR [%d]: [%d] > [%d] > [%d] \n", i, max[i], data[i],
					 min[i]);
				FTS_TEST_SAVE_INFO("test fail,tx%d=%5d,range=(%5d,%5d)\n",
						   i - rx_num + 1, data[i], min[i], max[i]);
				result = false;
			}
		}
	}

	return result;
}

static int short_get_adc_data_mc(u8 retval, int byte_num, int *adc_buf, u8 mode)
{
	int ret = 0;
	int i = 0;
	u8 short_state = 0;

	FTS_TEST_FUNC_ENTER();
	/* select short test mode & start test */
	ret = fts_test_write_reg(FACTROY_REG_SHORT_TEST_EN, mode);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write short test mode fail\n");
		goto test_err;
	}

	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		sys_delay(FACTORY_TEST_RETRY_DELAY);

		ret = fts_test_read_reg(FACTROY_REG_SHORT_TEST_EN, &short_state);

		if ((ret >= 0) && (retval == short_state)) {
			break;
		} else
			TPD_DEBUG("reg%x=%x,retry:%d",
				  FACTROY_REG_SHORT_TEST_EN, short_state, i);
	}

	if (i >= FACTORY_TEST_RETRY) {
		FTS_TEST_SAVE_ERR("short test timeout, ADC data not OK\n");
		ret = -EIO;
		goto test_err;
	}

	ret = read_mass_data(FACTORY_REG_SHORT_ADDR_MC, byte_num, adc_buf);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get short(adc) data fail\n");
	}

	/*    TPD_DEBUG("adc data:\n");*/
	/*    print_buffer(adc_buf, byte_num / 2, 0);*/
test_err:
	FTS_TEST_FUNC_EXIT();
	return ret;
}

static bool short_test_get_channel_num(struct chip_data_ft3518 *ts_data,
				       int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_mut)
{
	int ret = 0;
	int i;
	int count = 0;
	int total_num;
	int max_tx;
	int all_adc_data_num;
	int res_stalls = 111;
	int code_1 = 1437;
	int code_0 = 1437;
	bool tmp_result = true;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;
	int min_cc = thr->short_cc;

	total_num = tx_num + rx_num;
	max_tx = tx_num;
	all_adc_data_num = 1 + tx_num + rx_num;

	for (i = 0; i < 1; i++) {
		ret = short_get_adc_data_mc(TEST_RETVAL_00, all_adc_data_num * 2, adc_data,
					    FACTROY_REG_SHORT_CA);
		sys_delay(50);

		if (ret) {
			tmp_result = false;
			FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
			goto TEST_END;
		}
	}

	/* print all Adc value */
	FTS_TEST_SAVE_INFO("Offset:%4d, Code0:%4d, Code1:%4d, \n", offset, code_0,
			   code_1);
	/*    show_data_mc_sc(&adc_data[1]);*/

	count = 0;

	for (i = 0; i < total_num; i++) {
		if (code_1 - adc_data[i] <= 0) {
			fm_short_resistance[i] = min_cc;
			continue;
		}

		fm_short_resistance[i] = (adc_data[i] - offset + 395) * res_stalls /
					 (code_1 - adc_data[i]) - 3;

		if (fm_short_resistance[i] < 0) {
			fm_short_resistance[i] = abs(fm_short_resistance[i]);
		}

		if (min_cc > fm_short_resistance[i]) {
			count++;
		}
	}

	if (count > 0) {
		*is_weak_short_mut = true;
	}

TEST_END:
	return tmp_result;
}

static bool short_test_channel_to_gnd(struct chip_data_ft3518 *ts_data,
				      int *fm_short_resistance, int *adc_data, int offset, bool *is_weak_short_gnd)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int min_cg = 0;
	int min_cc = 0;
	int i;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *fg_short_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL;
	bool is_used = false;
	bool tmp_result = true;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	total_num = tx_num + rx_num;
	all_adc_data_num = 1 + tx_num + rx_num;
	min_cc = thr->short_cc;
	min_cg = thr->short_cg;

	error_ch = kzalloc(total_num * sizeof(u8), GFP_KERNEL);

	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
		goto TEST_END;
	}

	min_70k_ch = kzalloc(total_num * sizeof(u8), GFP_KERNEL);

	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
		goto TEST_END;
	}

	w_buf = kzalloc((total_num + 3) * sizeof(u8), GFP_KERNEL);

	if (NULL == w_buf) {
		FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
		goto TEST_END;
	}

	tmp_adc_data = kzalloc(total_num * total_num * sizeof(int), GFP_KERNEL);

	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
		goto TEST_END;
	}

	fg_short_resistance =  kzalloc(total_num * sizeof(int), GFP_KERNEL);

	if (NULL == fg_short_resistance) {
		FTS_TEST_SAVE_ERR("fg_short_resistance buffer malloc fail\n");
		goto TEST_END;
	}

	error_num = 0;
	min_70k_num = 0;

	for (i = 0; i < total_num; i++) {
		if (fm_short_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}

	if (error_num > 0) {
		w_buf[0] = (u8)error_num;

		for (i = 0; i < error_num; i++) {
			w_buf[1 + i] = error_ch[i];
		}

		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);

		for (i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, error_num * 2, tmp_adc_data,
						    FACTROY_REG_SHORT_CG);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		for (i = 0; i < error_num; i++) {
			if (code_1 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}

			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls /
				 (code_1 - tmp_adc_data[i]) - 3;

			if (fvalue < 0) {
				fvalue = abs(fg_short_resistance[i]);
			}

			if (min_cg > fvalue) {
				fg_short_resistance[error_ch[i] - 1] = fvalue;
				adc_data[error_ch[i] - 1] = tmp_adc_data[i];
				*is_weak_short_gnd = true;

				if (fvalue > 70) {
					if (error_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d with GND ", error_ch[i]);

					} else {
						FTS_TEST_SAVE_INFO("Rx%d with GND ", (error_ch[i] - tx_num));
					}

					FTS_TEST_SAVE_INFO(" Resistance: %2d, ADC: %d\n", fvalue, tmp_adc_data[i]);
					tmp_result = false;
				}

				if (fvalue < 70) {
					is_used = false;

					for (num = 0; num < min_70k_num; num++) {
						if (error_ch[i] == min_70k_ch[num]) {
							is_used = true;
							break;
						}
					}

					if (!is_used) {
						min_70k_ch[min_70k_num] = error_ch[i];
						min_70k_num++;
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x00);

		if (ret) {
			goto TEST_END;
		}

		memset(tmp_adc_data, 0, (all_adc_data_num + 1));
		w_buf[0] = (u8)min_70k_num;

		for (i = 0; i < min_70k_num; i++) {
			w_buf[1 + i] = min_70k_ch[i];
		}

		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);

		for (i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, min_70k_num * 2, tmp_adc_data,
						    FACTROY_REG_SHORT_CG);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		for (i = 0; i < min_70k_num; i++) {
			if (code_0 - tmp_adc_data[i] <= 0) {
				fg_short_resistance[i] = min_cg;
				continue;
			}

			fvalue = (tmp_adc_data[i] - offset + 395) * res_stalls0 /
				 (code_0 - tmp_adc_data[i]) - 3;

			if (fvalue < 0) {
				fvalue = abs(fg_short_resistance[i]);
			}

			if (min_cg > fvalue) {
				fg_short_resistance[min_70k_ch[i] - 1] = fvalue;
				adc_data[min_70k_ch[i] - 1] = tmp_adc_data[i];

				if (min_70k_ch[i] <= tx_num) {
					FTS_TEST_SAVE_INFO("Tx%d with GND", min_70k_ch[i]);

				} else {
					FTS_TEST_SAVE_INFO("Rx%d with GND", (min_70k_ch[i] - tx_num));
				}

				FTS_TEST_SAVE_INFO(" Resistance: %d, ADC: %d\n", fvalue, tmp_adc_data[i]);
				tmp_result = false;
			}
		}
	}

TEST_END:

	if (error_ch) {
		kfree(error_ch);
		error_ch = NULL;
	}

	if (min_70k_ch) {
		kfree(min_70k_ch);
		min_70k_ch = NULL;
	}

	if (tmp_adc_data) {
		kfree(tmp_adc_data);
		tmp_adc_data = NULL;
	}

	if (fg_short_resistance) {
		kfree(fg_short_resistance);
		fg_short_resistance = NULL;
	}

	if (w_buf) {
		kfree(w_buf);
		w_buf = NULL;
	}

	return tmp_result;
}

static bool short_test_channel_to_channel(struct chip_data_ft3518 *ts_data,
		int *fm_short_resistance, int *adc_data, int offset)
{
	int ret = 0;
	int error_num = 0;
	int min_70k_num = 0;
	int total_num;
	int min_cc = 0;
	int i;
	int j;
	int num;
	int code_1 = 1437;
	int code_0 = 1437;
	int res_stalls = 111;
	int fvalue = 0;
	int adc_count = 0;
	int all_adc_data_num = 0;
	int res_stalls0 = 4;
	int *f_origin_resistance = NULL;
	int *tmp_adc_data = NULL;
	u8 *w_buf = NULL;
	u8 *error_ch = NULL;
	u8 *min_70k_ch = NULL;
	bool is_used = false;
	bool tmp_result = true;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	total_num = tx_num + rx_num;
	all_adc_data_num = 1 + tx_num + rx_num;
	min_cc = thr->short_cc;

	f_origin_resistance =  kzalloc(total_num * sizeof(int), GFP_KERNEL);

	if (NULL == f_origin_resistance) {
		FTS_TEST_SAVE_ERR("f_origin_resistance buffer malloc fail \n");
		goto TEST_END;
	}

	tmp_adc_data = kzalloc(total_num * total_num * sizeof(int), GFP_KERNEL);

	if (NULL == tmp_adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail \n");
		goto TEST_END;
	}

	for (i = 0; i < total_num; i++) {
		f_origin_resistance[i] = fm_short_resistance[i];
	}

	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x01);

	if (ret) {
		goto TEST_END;
	}

	w_buf = kzalloc((total_num + 3) * sizeof(u8), GFP_KERNEL);

	if (NULL == w_buf) {
		FTS_TEST_SAVE_ERR("w_buf buffer malloc fail\n");
		return -ENOMEM;
	}

	error_ch = kzalloc(total_num * sizeof(u8), GFP_KERNEL);

	if (NULL == error_ch) {
		FTS_TEST_SAVE_ERR("error_ch buffer malloc fail\n");
		goto TEST_END;
	}

	min_70k_ch =  kzalloc(total_num * sizeof(u8), GFP_KERNEL);

	if (NULL == min_70k_ch) {
		FTS_TEST_SAVE_ERR("min_70k_ch buffer malloc fail\n");
		goto TEST_END;
	}

	error_num = 0;
	min_70k_num = 0;

	for (i = 0; i < total_num; i++) {
		if (f_origin_resistance[i] < min_cc) {
			error_ch[error_num] = (u8)(i + 1);
			error_num++;
		}
	}

	if (error_num > 1) {
		w_buf[0] = (u8)error_num;

		for (i = 0; i < error_num; i++) {
			w_buf[1 + i] = error_ch[i];
		}

		memset(tmp_adc_data, 0, (all_adc_data_num + 1));

		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, error_num + 1);

		for (j = 0; j < 1; j++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00, error_num * (error_num - 1) * 2 / 2,
						    tmp_adc_data, FACTROY_REG_SHORT_CC);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		adc_count = 0;

		for (i = 0; i < error_num; i++) {
			for (j = i + 1; j < error_num; j++) {
				if (code_1 - tmp_adc_data[adc_count] <= 0) {
					fvalue = min_cc;
					continue;
				}

				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls /
					 (code_1 - tmp_adc_data[adc_count]) - 3;
				adc_count++;

				if (fvalue < 0) {
					fvalue = abs(fvalue);
				}

				if (min_cc > fvalue) {
					fm_short_resistance[error_ch[i] - 1] = fvalue;
					fm_short_resistance[error_ch[j] - 1] = fvalue;
					adc_data[error_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[error_ch[j] - 1] = tmp_adc_data[adc_count];

					if (fvalue > 70) {
						if (error_ch[i] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d ", (error_ch[i]));

						} else {
							FTS_TEST_SAVE_INFO("Rx%d ", (error_ch[i] - tx_num));
						}

						if (error_ch[j] <= tx_num) {
							FTS_TEST_SAVE_INFO("Tx%d ", (error_ch[j]));

						} else {
							FTS_TEST_SAVE_INFO("Rx%d ", (error_ch[j] - tx_num));
						}

						FTS_TEST_SAVE_INFO(": Resistance: %d , ADC: %d\n", fvalue,
								   tmp_adc_data[adc_count]);
						tmp_result = false;

					} else {
						is_used = false;

						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[i] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}

						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[i];
							min_70k_num++;
						}

						is_used = false;

						for (num = 0; num < min_70k_num; num++) {
							if (error_ch[j] == min_70k_ch[num]) {
								is_used = true;
								break;
							}
						}

						if (!is_used) {
							min_70k_ch[min_70k_num] = error_ch[j];
							min_70k_num++;
						}
					}
				}
			}
		}
	}

	if (min_70k_num > 0) {
		ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x00);

		if (ret) {
			goto TEST_END;
		}

		w_buf[0] = (u8)min_70k_num + 1;

		for (i = 0; i < min_70k_num; i++) {
			w_buf[1 + i] = min_70k_ch[i];
		}

		ret = fts_test_write(FACTROY_REG_SHORT_AB_CH, w_buf, min_70k_num + 1);
		memset(tmp_adc_data, 0, (all_adc_data_num + 1));

		for (i = 0; i < 1; i++) {
			ret = short_get_adc_data_mc(TEST_RETVAL_00,
						    min_70k_num * (min_70k_num - 1) * 2 / 2, tmp_adc_data, FACTROY_REG_SHORT_CC);
			sys_delay(50);

			if (ret) {
				FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d\n", ret);
				tmp_result = false;
				goto TEST_END;
			}
		}

		adc_count = 0;

		for (i = 0; i < min_70k_num; i++) {
			for (j = i + 1; j < min_70k_num; j++) {
				if (0 >= code_0 - tmp_adc_data[adc_count]) {
					fvalue = min_cc;
					continue;
				}

				fvalue = (tmp_adc_data[adc_count] - offset + 395) * res_stalls0 /
					 (code_0 - tmp_adc_data[adc_count]) - 3;
				adc_count++;

				if (fvalue < 0) {
					fvalue = abs(fvalue);
				}

				if (min_cc > fvalue) {
					fm_short_resistance[min_70k_ch[i] - 1] = fvalue;
					fm_short_resistance[min_70k_ch[j] - 1] = fvalue;
					adc_data[min_70k_ch[i] - 1] = tmp_adc_data[adc_count];
					adc_data[min_70k_ch[j] - 1] = tmp_adc_data[adc_count];

					if (min_70k_ch[i] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d ", (min_70k_ch[i]));

					} else {
						FTS_TEST_SAVE_INFO("Rx%d ", (min_70k_ch[i] - tx_num));
					}

					if (min_70k_ch[j] <= tx_num) {
						FTS_TEST_SAVE_INFO("Tx%d ", (min_70k_ch[j]));

					} else {
						FTS_TEST_SAVE_INFO("Rx%d ", (min_70k_ch[j] - tx_num));
					}

					FTS_TEST_SAVE_INFO(":Resistance: %d, ADC: %d\n", fvalue,
							   tmp_adc_data[adc_count]);

					tmp_result = false;
				}
			}
		}
	}

TEST_END:

	if (error_ch) {
		kfree(error_ch);
		error_ch = NULL;
	}

	if (min_70k_ch) {
		kfree(min_70k_ch);
		min_70k_ch = NULL;
	}

	if (tmp_adc_data) {
		kfree(tmp_adc_data);
		tmp_adc_data = NULL;
	}

	if (f_origin_resistance) {
		kfree(f_origin_resistance);
		f_origin_resistance = NULL;
	}

	if (w_buf) {
		kfree(w_buf);
		w_buf = NULL;
	}

	return tmp_result;
}


static void ft3518_autotest_populate_result_head(
		struct chip_data_ft3518 *ts_data, struct auto_testdata *p_testdata)
{
	uint8_t  data_buf[256];
	uint32_t buflen = 0;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;

	FTS_TEST_FUNC_ENTER();

	/*header*/
	buflen = snprintf(data_buf, 256, "ECC, 85, 170, IC Name, %s, IC Code, %x\n",
			  "FT3518U", 0x5509);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "TestItem Num, %d, ", 8);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ", "Noise Test", 14,
			  tx_num, rx_num, 11, 1);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ", "Rawdata Test", 7,
			  tx_num, rx_num, 11 + tx_num, 2);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ",
			  "Rawdata Uniformity Test", 16, tx_num, rx_num, 11 + tx_num * 2, 1);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ",
			  "Rawdata Uniformity Test", 16, tx_num, rx_num, 11 + tx_num * 3, 2);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", 9,
			  2, rx_num, 11 + tx_num * 4, 1);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ", "SCAP CB Test", 9,
			  2, rx_num, 11 + tx_num * 4 + 2, 2);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ",
			  "SCAP Rawdata Test", 10, 2, rx_num, 11 + tx_num * 4 + 4, 1);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "%s, %d, %d, %d, %d, %d, ",
			  "SCAP Rawdata Test", 10, 2, rx_num, 11 + tx_num * 4 + 6, 2);
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	buflen = snprintf(data_buf, 256, "\n\n\n\n\n\n\n\n\n");
	tp_test_write(p_testdata->fp, p_testdata->length, data_buf, buflen,
		      p_testdata->pos);

	FTS_TEST_FUNC_EXIT();
	return;
}


#define NUM_MODE 2
#define TEST_RESULT_NORMAL     0
#define TEST_RESULT_ABNORMAL  -1
int ft3518_auto_preoperation(struct seq_file *s, void *chip_data,
			     struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;

	int node_num = ts_data->hw_res->tx_num * ts_data->hw_res->rx_num;
	int channel_num = ts_data->hw_res->tx_num + ts_data->hw_res->rx_num;

	ts_data->noise_rawdata = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!ts_data->noise_rawdata) {
		FTS_TEST_SAVE_ERR("kzalloc for noise_rawdata fail\n");
		goto alloc_err;
	}

	ts_data->rawdata = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!ts_data->rawdata) {
		FTS_TEST_SAVE_ERR("kzalloc for rawdata fail\n");
		goto alloc_err;
	}


	ts_data->scap_cb = (int *)kzalloc(channel_num * NUM_MODE * sizeof(int),
					  GFP_KERNEL);

	if (!ts_data->scap_cb) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_cb fail\n");
		goto alloc_err;
	}

	ts_data->scap_rawdata = (int *)kzalloc(channel_num * NUM_MODE * sizeof(int),
					       GFP_KERNEL);

	if (!ts_data->scap_rawdata) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_rawdata fail\n");
		goto alloc_err;
	}

	ts_data->panel_differ_raw = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!ts_data->panel_differ_raw) {
		FTS_TEST_SAVE_ERR("kzalloc for panel_differ_raw fail\n");
		goto alloc_err;
	}

	ts_data->rawdata_linearity = kzalloc(node_num * 2 * sizeof(int), GFP_KERNEL);

	if (!ts_data->rawdata_linearity) {
		FTS_TEST_SAVE_ERR("ts_data->rawdata_linearity buffer malloc fail\n");
		goto alloc_err;
	}

	ft3518_autotest_populate_result_head(ts_data, focal_testdata);
	fts_test_entry(ts_data, focal_testdata);

	return TEST_RESULT_NORMAL;

alloc_err:

	if (ts_data->rawdata_linearity) {
		kfree(ts_data->rawdata_linearity);
		ts_data->rawdata_linearity = NULL;
	}

	if (ts_data->panel_differ_raw) {
		kfree(ts_data->panel_differ_raw);
		ts_data->panel_differ_raw = NULL;
	}

	if (ts_data->scap_rawdata) {
		kfree(ts_data->scap_rawdata);
		ts_data->scap_rawdata = NULL;
	}

	if (ts_data->scap_cb) {
		kfree(ts_data->scap_cb);
		ts_data->scap_cb = NULL;
	}

	if (ts_data->rawdata) {
		kfree(ts_data->rawdata);
		ts_data->rawdata = NULL;
	}

	if (ts_data->noise_rawdata) {
		kfree(ts_data->noise_rawdata);
		ts_data->noise_rawdata = NULL;
	}

	return TEST_RESULT_ABNORMAL;
}



int ft3518_noise_autotest(struct seq_file *s, void *chip_data,
			  struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	int i = 0;
	u8 fir = 0;
	u8 reg06_val = 0;
	u8 reg0d_val = 0;
	u8 rawdata_addr = 0;
	//uint8_t data_buf[64];
	bool result = false;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int byte_num = 0;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	TPD_INFO("\n============ Test Item: Noise Test\n");
	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Noise Test\n");


	if (!ts_data->fts_autotest_offset->fts_noise_data_P
			|| !ts_data->fts_autotest_offset->fts_noise_data_N) {
		TPD_INFO("fts_noise_data_P || fts_noise_data_N is NULL");
		return 0;

	} else {
		TPD_INFO("fts_noise_data_P || fts_noise_data_N is effective \n");
	}

	if (!thr || !thr->node_valid || !ts_data->noise_rawdata) {
		FTS_TEST_SAVE_ERR("thr/node_valid/rawdata is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_TOUCH_THR, &reg0d_val);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg0d fail,ret=%d\n", ret);
		goto test_err;
	}

	TPD_INFO("reg0d_val = [%d]\n", reg0d_val);

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_DATA_SELECT, &reg06_val);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read reg06 fail,ret=%d\n", ret);
		goto test_err;
	}

	TPD_INFO("reg06_val = [%d]\n", reg06_val);

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read fir error,ret=%d\n", ret);
		goto test_err;
	}

	TPD_INFO("fir = [%d]\n", fir);

	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, 0x01);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set reg06 fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, 0x01);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_FRAME_NUM, 20);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set frame fail,ret=%d\n", ret);
		goto restore_reg;
	}

	ret = fts_test_write_reg(FACTORY_REG_MAX_DIFF, 0x01);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("write 0x1B fail,ret=%d\n", ret);
		goto restore_reg;
	}

	for (i = 0; i < 3; i++) {
		/* lost 3 frames, in order to obtain stable data */
		/* start scanning */
		ret = start_scan();

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("scan fail\n");
			continue;
		}

		/* read rawdata */
		rawdata_addr = FACTORY_REG_RAWDATA_ADDR_MC_SC;
		byte_num = node_num * 2;
		ret = read_rawdata(FACTORY_REG_LINE_ADDR, 0xAA, rawdata_addr, byte_num,
				   ts_data->noise_rawdata);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read rawdata fail\n");
		}
	}

	if (ret < 0) {
		result = false;
		goto restore_reg;
	}

	/*snprintf(data_buf, 64, "%s\n", "[FOCAL NOISE DATA]");
	tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
		      strlen(data_buf), focal_testdata->pos);*/
	ft3518_output_data(ts_data->noise_rawdata, ts_data, focal_testdata, NODE_MATCH);

	/* compare */
	/*max = reg0d_val * 4 * thr->noise_coefficient / 100;*/
	/*TPD_INFO("reg0d:%d, max:%d", (int)reg0d_val, max);*/
	result = true;

	if (ts_data->fts_autotest_offset->fts_noise_data_P
			&& ts_data->fts_autotest_offset->fts_noise_data_N) {
		for (i = 0; i < node_num; i++) {
			if (ts_data->noise_rawdata[i] >
					ts_data->fts_autotest_offset->fts_noise_data_P[i]) {
				TPD_INFO("noise data ERR [%d]: [%d] > [%d] > [%d] \n", i,
					 ts_data->fts_autotest_offset->fts_noise_data_P[i], ts_data->noise_rawdata[i],
					 ts_data->fts_autotest_offset->fts_noise_data_N[i]);
				FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
						  i / rx_num + 1, i % rx_num + 1, ts_data->noise_rawdata[i],
						  ts_data->fts_autotest_offset->fts_noise_data_N[i],
						  ts_data->fts_autotest_offset->fts_noise_data_P[i]);
				result = false;
			}
		}

	} else {
		TPD_INFO("fts_raw_data_P || fts_raw_data_N is null \n");
		result = false;
	}

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_DATA_SELECT, reg06_val);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}

test_err:
	FTS_TEST_FUNC_EXIT();

	if (result) {
		return TEST_RESULT_NORMAL;

	} else {
		return TEST_RESULT_ABNORMAL;
	}
}


int ft3518_rawdata_autotest(struct seq_file *s, void *chip_data,
			    struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	int i = 0;
	u8 fre = 0;
	u8 fir = 0;
	u8 normalize = 0;
	u8 rawdata_addr = 0;
	//uint8_t data_buf[64];
	bool result = false;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int byte_num = 0;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Rawdata Test\n");

	if (!ts_data->fts_autotest_offset->fts_raw_data_P
			|| !ts_data->fts_autotest_offset->fts_raw_data_N) {
		TPD_INFO("fts_raw_data_P || fts_raw_data_N is NULL");
		return 0;
	}

	if (!thr || !thr->rawdata_h_min || !thr->rawdata_h_max || !thr->node_valid
			|| !ts_data->rawdata) {
		FTS_TEST_SAVE_ERR("thr/rawdata_h_min/max/node/rawdata is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	/* save origin value */
	ret = fts_test_read_reg(FACTORY_REG_NORMALIZE, &normalize);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read normalize fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FRE_LIST, &fre);

	if (ret) {
		FTS_TEST_SAVE_ERR("read 0x0A fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_FIR, &fir);

	if (ret) {
		FTS_TEST_SAVE_ERR("read 0xFB error,ret=%d\n", ret);
		goto test_err;
	}

	/* set to auto normalize */
	if (normalize != 0x01) {
		ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, 0x01);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("write normalize fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	/* set frequecy high */

	if (!ts_data->use_panelfactory_limit) {
		ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x81);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
			goto restore_reg;
		}

	} else if (ts_data->use_panelfactory_limit) {
		ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, 0x0);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("set frequecy fail,ret=%d\n", ret);
			goto restore_reg;
		}
	}

	/* fir enable */
	ret = fts_test_write_reg(FACTORY_REG_FIR, 1);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("set fir fail,ret=%d\n", ret);
		goto restore_reg;
	}

	/*********************GET RAWDATA*********************/
	for (i = 0; i < 3; i++) {
		/* lost 3 frames, in order to obtain stable data */
		/* start scanning */
		ret = start_scan();

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("scan fail\n");
			continue;
		}

		/* read rawdata */
		rawdata_addr = FACTORY_REG_RAWDATA_ADDR_MC_SC;
		byte_num = node_num * 2;
		ret = read_rawdata(FACTORY_REG_LINE_ADDR, 0xAA, rawdata_addr, byte_num,
				   ts_data->rawdata);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read rawdata fail\n");
		}
	}

	if (ret < 0) {
		result = false;
		goto restore_reg;
	}

	/*snprintf(data_buf, 64, "%s\n", "[FOCAL RAW DATA]");
	tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
		      strlen(data_buf), focal_testdata->pos);*/
	ft3518_output_data(ts_data->rawdata, ts_data, focal_testdata, NODE_MATCH);


	/* compare */
	result = true;

	for (i = 0; i < node_num; i++) {
		if (0 == thr->node_valid[i]) {
			continue;
		}

		if ((ts_data->rawdata[i] < ts_data->fts_autotest_offset->fts_raw_data_N[i])
				|| (ts_data->rawdata[i] > ts_data->fts_autotest_offset->fts_raw_data_P[i])) {
			TPD_INFO("raw data ERR [%d]: [%d] > [%d] > [%d] \n", i,
				 ts_data->fts_autotest_offset->fts_raw_data_P[i], ts_data->rawdata[i],
				 ts_data->fts_autotest_offset->fts_raw_data_N[i]);
			FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
					  i / rx_num + 1, i % rx_num + 1, ts_data->rawdata[i],
					  ts_data->fts_autotest_offset->fts_raw_data_N[i],
					  ts_data->fts_autotest_offset->fts_raw_data_P[i]);
			result = false;
		}
	}

restore_reg:
	/* set the origin value */
	ret = fts_test_write_reg(FACTORY_REG_NORMALIZE, normalize);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore normalize fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FRE_LIST, fre);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0x0A fail,ret=%d\n", ret);
	}

	ret = fts_test_write_reg(FACTORY_REG_FIR, fir);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("restore 0xFB fail,ret=%d\n", ret);
	}

test_err:
	FTS_TEST_FUNC_EXIT();

	if (result) {
		return TEST_RESULT_NORMAL;

	} else {
		return TEST_RESULT_ABNORMAL;
	}
}


int ft3518_uniformity_autotest(struct seq_file *s, void *chip_data,
			       struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	int row = 0;
	int col = 1;
	int i = 0;
	int deviation = 0;
	int max = 0;
	int *rl_tmp = NULL;
	int offset = 0;
	int offset2 = 0;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;
	bool result = false;
	bool result2 = false;
	//uint8_t data_buf[64];

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Rawdata Unfiormity Test\n");

	if (!ts_data->fts_autotest_offset->fts_uniformity_data_P
			|| !ts_data->fts_autotest_offset->fts_uniformity_data_N) {
		TPD_INFO("fts_uniformity_data_P || fts_uniformity_data_N is NULL");
		return 0;
	}

	if (!thr || !thr->tx_linearity_max || !thr->rx_linearity_max
			|| !thr->node_valid || !ts_data->rawdata) {
		FTS_TEST_SAVE_ERR("thr/tx/rx_lmax/node_valid/rawdata is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		goto test_err;
	}

	result = true;
	/*    FTS_TEST_SAVE_INFO("Check Tx Linearity\n");*/
	ts_data->rl_cnt = 0;
	rl_tmp = ts_data->rawdata_linearity + ts_data->rl_cnt;

	for (row = 0; row < tx_num; row++) {
		for (col = 0; col < rx_num - 1; col++) {
			offset = row * rx_num + col;
			offset2 = row * rx_num + col + 1;
			deviation = abs(ts_data->rawdata[offset] - ts_data->rawdata[offset2]);
			max = max(ts_data->rawdata[offset], ts_data->rawdata[offset2]);
			max = max ? max : 1;
			rl_tmp[offset] = 100 * deviation / max;
		}
	}

	/*snprintf(data_buf, 64, "%s\n", "[FOCAL TX UNIFORMITY DATA]");
	tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
		      strlen(data_buf), focal_testdata->pos);*/
	ft3518_output_data(rl_tmp, ts_data, focal_testdata, NODE_MATCH);

	/* compare */
	for (i = 0; i < node_num; i++) {
		if (0 == thr->node_valid[i]) {
			continue;
		}

		if ((rl_tmp[i] < ts_data->fts_autotest_offset->fts_uniformity_data_N[i])
				|| (rl_tmp[i] > ts_data->fts_autotest_offset->fts_uniformity_data_P[i])) {
			TPD_INFO("uniformity data ERR [%d]: [%d] > [%d] > [%d] \n", i,
				 ts_data->fts_autotest_offset->fts_uniformity_data_P[i], rl_tmp[i],
				 ts_data->fts_autotest_offset->fts_uniformity_data_N[i]);
			FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
					  i / rx_num + 1, i % rx_num + 1, rl_tmp[i],
					  ts_data->fts_autotest_offset->fts_uniformity_data_N[i],
					  ts_data->fts_autotest_offset->fts_uniformity_data_P[i]);
			result = false;
		}
	}

	ts_data->rl_cnt += node_num;

	result2 = true;
	/*    FTS_TEST_SAVE_INFO("Check Rx Linearity\n");*/
	rl_tmp = ts_data->rawdata_linearity + ts_data->rl_cnt;

	for (row = 0; row < tx_num - 1; row++) {
		for (col = 0; col < rx_num; col++) {
			offset = row * rx_num + col;
			offset2 = (row + 1) * rx_num + col;
			deviation = abs(ts_data->rawdata[offset] - ts_data->rawdata[offset2]);
			max = max(ts_data->rawdata[offset], ts_data->rawdata[offset2]);
			max = max ? max : 1;
			rl_tmp[offset] = 100 * deviation / max;
		}
	}

	/*snprintf(data_buf, 64, "%s\n", "[FOCAL RX UNIFORMITY DATA]");
	tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
		      strlen(data_buf), focal_testdata->pos);*/
	ft3518_output_data(rl_tmp, ts_data, focal_testdata, NODE_MATCH);


	/* compare */
	for (i = 0; i < node_num; i++) {
		if (0 == thr->node_valid[i]) {
			continue;
		}

		if ((rl_tmp[i] < ts_data->fts_autotest_offset->fts_uniformity_data_N[i])
				|| (rl_tmp[i] > ts_data->fts_autotest_offset->fts_uniformity_data_P[i])) {
			TPD_INFO("uniformity data ERR [%d]: [%d] > [%d] > [%d] \n", i,
				 ts_data->fts_autotest_offset->fts_uniformity_data_P[i], rl_tmp[i],
				 ts_data->fts_autotest_offset->fts_uniformity_data_N[i]);
			FTS_TEST_SAVE_ERR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
					  i / rx_num + 1, i % rx_num + 1, rl_tmp[i],
					  ts_data->fts_autotest_offset->fts_uniformity_data_N[i],
					  ts_data->fts_autotest_offset->fts_uniformity_data_P[i]);
			result2 = false;
		}
	}

	ts_data->rl_cnt += node_num;

test_err:
	FTS_TEST_FUNC_EXIT();

	if (result && result2) {
		FTS_TEST_SAVE_INFO("------Rawdata Uniformity Test PASS\n");
		return TEST_RESULT_NORMAL;

	} else {
		FTS_TEST_SAVE_ERR("------Rawdata Uniformity Test NG\n");
		return TEST_RESULT_ABNORMAL;
	}
}



int ft3518_scap_cb_autotest(struct seq_file *s, void *chip_data,
			    struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	u8 sc_mode = 0;
	//uint8_t data_buf[64];
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int *scb_tmp = NULL;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int channel_num = tx_num + rx_num;
	int byte_num = channel_num * 2;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap CB Test\n");

	if (!ts_data->fts_autotest_offset->fts_scap_cb_data_P
			|| !ts_data->fts_autotest_offset->fts_scap_cb_data_N
			|| !ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N
			|| !ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P) {
		TPD_INFO("fts_scap_cb_data_P || fts_scap_cb_data_N || fts_scap_cb_data_waterproof_N || fts_scap_cb_data_waterproof_P is NULL");
		return 0;
	}

	if (!thr || !thr->node_valid_sc || !thr->scap_cb_on_min || !thr->scap_cb_on_max
			|| !thr->scap_cb_off_min || !thr->scap_cb_off_max) {
		FTS_TEST_SAVE_ERR("thr/node_valid_sc/scap_cb_on/off_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	ret = fts_test_read_reg(FACTORY_REG_MC_SC_MODE, &sc_mode);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read sc_mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* water proof on check */
	ts_data->scb_cnt = 0;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);

	if (fw_wp_check) {
		scb_tmp = ts_data->scap_cb + ts_data->scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_ON, byte_num, scb_tmp, DATA_TWO_BYTE);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		/*        FTS_TEST_SAVE_INFO("scap_cb in waterproof on mode:\n");*/

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);

		/*snprintf(data_buf, 64, "%s\n", "[FOCAL SCAP CB WP DATA]");
		tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
			      strlen(data_buf), focal_testdata->pos);*/
		ft3518_output_data(scb_tmp, ts_data, focal_testdata, CHEN_MATCH);

		tmp_result = compare_mc_sc(ts_data, tx_check, rx_check, scb_tmp,
					   ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N,
					   ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P);

		ts_data->scb_cnt += channel_num;

	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);

	if (fw_wp_check) {
		scb_tmp = ts_data->scap_cb + ts_data->scb_cnt;
		/* 1:waterproof 0:non-waterproof */
		ret = get_cb_mc_sc(WATER_PROOF_OFF, byte_num, scb_tmp, DATA_TWO_BYTE);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("read sc_cb fail,ret=%d\n", ret);
			goto restore_reg;
		}

		/* show Scap CB */
		/*        FTS_TEST_SAVE_INFO("scap_cb in waterproof off mode:\n");*/

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);

		/*snprintf(data_buf, 64, "%s\n", "[FOCAL SCAP CB DATA]");
		tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
			      strlen(data_buf), focal_testdata->pos);*/
		ft3518_output_data(scb_tmp, ts_data, focal_testdata, CHEN_MATCH);

		tmp2_result = compare_mc_sc(ts_data, tx_check, rx_check, scb_tmp,
					    ts_data->fts_autotest_offset->fts_scap_cb_data_N,
					    ts_data->fts_autotest_offset->fts_scap_cb_data_P);

		ts_data->scb_cnt += channel_num;

	} else {
		tmp2_result = true;
	}


restore_reg:
	ret = fts_test_write_reg(FACTORY_REG_MC_SC_MODE,
				 sc_mode);/* set the origin value */

	if (ret) {
		FTS_TEST_SAVE_ERR("write sc mode fail,ret=%d\n", ret);
	}

test_err:
	FTS_TEST_FUNC_EXIT();

	if (tmp_result && tmp2_result) {
		FTS_TEST_SAVE_INFO("------Scap CB (normal && waterproof) Test PASS\n");
		return TEST_RESULT_NORMAL;

	} else {
		if (tmp_result) {
			FTS_TEST_SAVE_ERR("------Scap CB Test (waterproof) NG\n");
		}

		return TEST_RESULT_ABNORMAL;

		if (tmp2_result) {
			FTS_TEST_SAVE_ERR("------Scap CB Test (normal) NG\n");
		}

		return TEST_RESULT_ABNORMAL;
	}
}


int ft3518_scap_rawdata_autotest(struct seq_file *s, void *chip_data,
				 struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	bool tmp_result = false;
	bool tmp2_result = false;
	u8 wc_sel = 0;
	//uint8_t data_buf[64];
	bool fw_wp_check = false;
	bool tx_check = false;
	bool rx_check = false;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int *srawdata_tmp = NULL;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int channel_num = tx_num + rx_num;
	int byte_num = channel_num * 2;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_INFO("\n============ Test Item: Scap Rawdata Test\n");

	if (!ts_data->fts_autotest_offset->fts_scap_raw_data_P
			|| !ts_data->fts_autotest_offset->fts_scap_raw_data_N ||
			!ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N
			|| !ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P) {
		TPD_INFO("fts_scap_raw_data_P || fts_scap_raw_data_N || fts_scap_raw_waterproof_data_N || fts_scap_raw_waterproof_data_P is NULL");
		return 0;
	}

	if (!thr || !thr->node_valid_sc || !thr->scap_rawdata_on_min
			|| !thr->scap_rawdata_on_max
			|| !thr->scap_rawdata_off_min || !thr->scap_rawdata_off_max) {
		FTS_TEST_SAVE_ERR("thr/node_valid_sc/scap_rawdata_on/off_min/max is null\n");
		ret = -EINVAL;
		goto test_err;
	}

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	/* get waterproof channel select */
	ret = fts_test_read_reg(FACTORY_REG_WC_SEL, &wc_sel);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("read water_channel_sel fail,ret=%d\n", ret);
		goto test_err;
	}

	/* scan rawdata */
	ret = start_scan();

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("scan scap rawdata fail\n");
		goto test_err;
	}

	/* water proof on check */
	ts_data->srawdata_cnt = 0;
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_ON);

	if (fw_wp_check) {
		srawdata_tmp = ts_data->scap_rawdata + ts_data->srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_ON, byte_num, srawdata_tmp);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_ON) rawdata fail\n");
			goto test_err;
		}

		/*        FTS_TEST_SAVE_INFO("scap_rawdata in waterproof on mode:\n");*/

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_ON_RX);

		/*snprintf(data_buf, 64, "%s\n", "[FOCAL SCAP RAWDATA WP DATA]");
		tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
			      strlen(data_buf), focal_testdata->pos);*/
		ft3518_output_data(srawdata_tmp, ts_data, focal_testdata, CHEN_MATCH);

		tmp_result = compare_mc_sc(ts_data, tx_check, rx_check, srawdata_tmp,
					   ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N,
					   ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P);
		ts_data->srawdata_cnt += channel_num;

	} else {
		tmp_result = true;
	}

	/* water proof off check */
	fw_wp_check = get_fw_wp(wc_sel, WATER_PROOF_OFF);

	if (fw_wp_check) {
		srawdata_tmp = ts_data->scap_rawdata + ts_data->srawdata_cnt;
		ret = get_rawdata_mc_sc(WATER_PROOF_OFF, byte_num, srawdata_tmp);

		if (ret < 0) {
			FTS_TEST_SAVE_ERR("get scap(WP_OFF) rawdata fail\n");
			goto test_err;
		}

		/*        FTS_TEST_SAVE_INFO("scap_rawdata in waterproof off mode:\n");*/

		/* compare */
		tx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_TX);
		rx_check = get_fw_wp(wc_sel, WATER_PROOF_OFF_RX);

		/*snprintf(data_buf, 64, "%s\n", "[FOCAL SCAP RAWDATA DATA]");
		tp_test_write(focal_testdata->fp, focal_testdata->length, data_buf,
			      strlen(data_buf), focal_testdata->pos);*/
		ft3518_output_data(srawdata_tmp, ts_data, focal_testdata, CHEN_MATCH);

		tmp2_result = compare_mc_sc(ts_data, tx_check, rx_check, srawdata_tmp,
					    ts_data->fts_autotest_offset->fts_scap_raw_data_N,
					    ts_data->fts_autotest_offset->fts_scap_raw_data_P);
		ts_data->srawdata_cnt += channel_num;

	} else {
		tmp2_result = true;
	}


test_err:
	FTS_TEST_FUNC_EXIT();

	if (tmp_result && tmp2_result) {
		FTS_TEST_SAVE_INFO("------SCAP Rawdata Test PASS\n");
		return TEST_RESULT_NORMAL;

	} else {
		FTS_TEST_SAVE_INFO("------SCAP Rawdata Test NG\n");
		return TEST_RESULT_ABNORMAL;
	}

	return ret;
}


int ft3518_short_test(struct seq_file *s, void *chip_data,
		      struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	int ret = 0;
	int offset = 0;
	int total_num;
	int all_adc_data_num = 63;
	int *adc_data  = NULL;
	int *fm_short_resistance = NULL;
	int offset_value[4] = {0};
	u8 stall_value = 1;
	bool tmp_result = true;
	bool is_weak_short_gnd = false;
	bool is_weak_short_mut = false;
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int channel_num = tx_num + rx_num;

	total_num = channel_num;
	all_adc_data_num = 1 + total_num;

	FTS_TEST_SAVE_INFO("\n============ Test Item: Short Test\n");
	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("enter factory mode fail,ret=%d\n", ret);
		goto test_err;
	}

	adc_data = kzalloc((all_adc_data_num + 1) * sizeof(int), GFP_KERNEL);

	if (NULL == adc_data) {
		FTS_TEST_SAVE_ERR("adc_data buffer malloc fail\n");
		goto test_err;
	}

	fm_short_resistance = kzalloc(total_num * sizeof(int), GFP_KERNEL);

	if (NULL == fm_short_resistance) {
		FTS_TEST_SAVE_ERR("fm_short_resistance buffer malloc fail\n");
		goto test_err;
	}

	ret = fts_test_read_reg(FACTROY_REG_SHORT_DELAY, &stall_value);
	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, 0x01);

	if (ret) {
		tmp_result = false;
		goto test_err;
	}

	ret = short_get_adc_data_mc(TEST_RETVAL_00, 2, offset_value,
				    FACTROY_REG_SHORT_OFFSET);

	if (ret) {
		tmp_result = false;
		FTS_TEST_SAVE_ERR("failed to get weak short data,ret= %d \n", ret);
		goto test_err;
	}

	offset = offset_value[0] - 1024;

	/* get short resistance and exceptional channel */
	tmp_result = short_test_get_channel_num(ts_data, fm_short_resistance, adc_data,
						offset, &is_weak_short_mut);

	/* use the exceptional channel to conduct channel to ground short circuit test. */
	if (is_weak_short_mut) {
		tmp_result = short_test_channel_to_gnd(ts_data, fm_short_resistance, adc_data,
						       offset, &is_weak_short_gnd);
	}

	/* use the exceptional channel to conduct channel to channel short circuit test. */
	if (is_weak_short_mut) {
		tmp_result &= short_test_channel_to_channel(ts_data, fm_short_resistance,
				adc_data, offset);
	}

test_err:

	if (adc_data) {
		kfree(adc_data);
		adc_data = NULL;
	}

	if (fm_short_resistance) {
		kfree(fm_short_resistance);
		fm_short_resistance = NULL;
	}

	ret = fts_test_write_reg(FACTROY_REG_SHORT_DELAY, stall_value);

	if (is_weak_short_gnd && is_weak_short_mut) {
		TPD_INFO("gnd and mutual weak short! \n");

	} else if (is_weak_short_gnd) {
		TPD_INFO("gnd weak short! \n");

	} else if (is_weak_short_mut) {
		TPD_INFO("mutual weak short! \n");

	} else {
		TPD_INFO("no short! \n");
	}

	if (tmp_result) {
		FTS_TEST_SAVE_INFO("------Short test PASS\n");
		return TEST_RESULT_NORMAL;

	} else {
		FTS_TEST_SAVE_ERR("------Short Test NG\n");
		return TEST_RESULT_ABNORMAL;
	}
}


static int fts_rawdata_free(struct chip_data_ft3518 *ts_data)
{
	TPD_INFO("%s +\n", __func__);

	if (ts_data->rawdata_linearity) {
		kfree(ts_data->rawdata_linearity);
		ts_data->rawdata_linearity = NULL;
	}

	if (ts_data->panel_differ_raw) {
		kfree(ts_data->panel_differ_raw);
		ts_data->panel_differ_raw = NULL;
	}

	if (ts_data->scap_rawdata) {
		kfree(ts_data->scap_rawdata);
		ts_data->scap_rawdata = NULL;
	}

	if (ts_data->scap_cb) {
		kfree(ts_data->scap_cb);
		ts_data->scap_cb = NULL;
	}

	if (ts_data->rawdata) {
		kfree(ts_data->rawdata);
		ts_data->rawdata = NULL;
	}

	if (ts_data->noise_rawdata) {
		kfree(ts_data->noise_rawdata);
		ts_data->noise_rawdata = NULL;
	}

	TPD_INFO("%s -\n", __func__);

	return 0;
}

static void fts_threshold_free(struct chip_data_ft3518 *ts_data)
{
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	TPD_INFO("%s +\n", __func__);
	kfree(thr->node_valid);
	kfree(thr->node_valid_sc);
	kfree(thr->rawdata_h_max);
	kfree(thr->rawdata_h_min);
	kfree(thr->tx_linearity_max);
	kfree(thr->tx_linearity_min);
	kfree(thr->rx_linearity_max);
	kfree(thr->rx_linearity_min);
	kfree(thr->scap_cb_off_max);
	kfree(thr->scap_cb_off_min);
	kfree(thr->scap_cb_on_max);
	kfree(thr->scap_cb_on_min);
	kfree(thr->scap_rawdata_off_max);
	kfree(thr->scap_rawdata_off_min);
	kfree(thr->scap_rawdata_on_max);
	kfree(thr->scap_rawdata_on_min);
	kfree(thr->panel_differ_max);
	kfree(thr->panel_differ_min);
	TPD_INFO("%s -\n", __func__);
	return;
}


int ft3518_auto_endoperation(struct seq_file *s, void *chip_data,
			     struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info)
{
	struct chip_data_ft3518 *ts_data = (struct chip_data_ft3518 *)chip_data;

	TPD_INFO("%s +\n", __func__);

	if (ts_data->fts_autotest_offset) {
		kfree(ts_data->fts_autotest_offset);
		ts_data->fts_autotest_offset = NULL;
	}

	enter_work_mode();
	fts_threshold_free(ts_data);
	fts_rawdata_free(ts_data);

	TPD_INFO("%s -\n", __func__);
	return 0;
}



static int fts_threshold_malloc(struct chip_data_ft3518 *ts_data)
{
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	int channel_num = tx_num + rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	thr->node_valid = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->node_valid) {
		FTS_TEST_SAVE_ERR("kzalloc for node_valid fail\n");
		goto thr_free;
	}

	thr->node_valid_sc = (int *)kzalloc(channel_num * sizeof(int), GFP_KERNEL);

	if (!thr->node_valid_sc) {
		FTS_TEST_SAVE_ERR("kzalloc for node_valid_sc fail\n");
		goto thr_free;
	}

	thr->rawdata_h_max = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->rawdata_h_max) {
		FTS_TEST_SAVE_ERR("kzalloc for rawdata_h_max fail\n");
		goto thr_free;
	}

	thr->rawdata_h_min = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->rawdata_h_min) {
		FTS_TEST_SAVE_ERR("kzalloc for rawdata_h_min fail\n");
		goto thr_free;
	}

	thr->tx_linearity_max = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->tx_linearity_max) {
		FTS_TEST_SAVE_ERR("kzalloc for tx_linearity_max fail\n");
		goto thr_free;
	}

	thr->tx_linearity_min = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->tx_linearity_min) {
		FTS_TEST_SAVE_ERR("kzalloc for tx_linearity_min fail\n");
		goto thr_free;
	}

	thr->rx_linearity_max = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->rx_linearity_max) {
		FTS_TEST_SAVE_ERR("kzalloc for rx_linearity_max fail\n");
		goto thr_free;
	}

	thr->rx_linearity_min = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->rx_linearity_min) {
		FTS_TEST_SAVE_ERR("kzalloc for rx_linearity_min fail\n");
		goto thr_free;
	}

	thr->scap_cb_off_max = (int *)kzalloc(channel_num * sizeof(int), GFP_KERNEL);

	if (!thr->scap_cb_off_max) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_cb_off_max fail\n");
		goto thr_free;
	}

	thr->scap_cb_off_min = (int *)kzalloc(channel_num * sizeof(int), GFP_KERNEL);

	if (!thr->scap_cb_off_min) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_cb_off_min fail\n");
		goto thr_free;
	}

	thr->scap_cb_on_max = (int *)kzalloc(channel_num * sizeof(int), GFP_KERNEL);

	if (!thr->scap_cb_on_max) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_cb_on_max fail\n");
		goto thr_free;
	}

	thr->scap_cb_on_min = (int *)kzalloc(channel_num * sizeof(int), GFP_KERNEL);

	if (!thr->scap_cb_on_min) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_cb_on_min fail\n");
		goto thr_free;
	}

	thr->scap_rawdata_off_max = (int *)kzalloc(channel_num * sizeof(int),
				    GFP_KERNEL);

	if (!thr->scap_rawdata_off_max) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_rawdata_off_max fail\n");
		goto thr_free;
	}

	thr->scap_rawdata_off_min = (int *)kzalloc(channel_num * sizeof(int),
				    GFP_KERNEL);

	if (!thr->scap_rawdata_off_min) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_rawdata_off_min fail\n");
		goto thr_free;
	}

	thr->scap_rawdata_on_max = (int *)kzalloc(channel_num * sizeof(int),
				   GFP_KERNEL);

	if (!thr->scap_rawdata_on_max) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_rawdata_on_max fail\n");
		goto thr_free;
	}


	thr->scap_rawdata_on_min = (int *)kzalloc(channel_num * sizeof(int),
				   GFP_KERNEL);

	if (!thr->scap_rawdata_on_min) {
		FTS_TEST_SAVE_ERR("kzalloc for scap_rawdata_on_min fail\n");
		goto thr_free;
	}

	thr->panel_differ_max = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->panel_differ_max) {
		FTS_TEST_SAVE_ERR("kzalloc for panel_differ_max fail\n");
		goto thr_free;
	}

	thr->panel_differ_min = (int *)kzalloc(node_num * sizeof(int), GFP_KERNEL);

	if (!thr->panel_differ_min) {
		FTS_TEST_SAVE_ERR("kzalloc for panel_differ_min fail\n");
		goto thr_free;
	}

	return 0;

thr_free:
	fts_threshold_free(ts_data);
	return -ENOMEM;
}

static int fts_get_threshold(struct chip_data_ft3518 *ts_data, char *data)
{
	int i = 0;
	int ret = 0;
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	int channel_num = tx_num + rx_num;
	struct mc_sc_threshold *thr = &ts_data->mpt.thr;

	ret = fts_threshold_malloc(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("malloc for threshold fail\n");
		return ret;
	}

	if (!data) {
		thr->noise_coefficient = 200;
		thr->short_cc = 500;
		thr->short_cg = 500;

		for (i = 0; i < node_num; i++) {
			thr->node_valid[i] = 1;
			thr->rawdata_h_max[i] = 12000;
			thr->rawdata_h_min[i] = 2690;
			thr->tx_linearity_max[i] = 40;
			thr->tx_linearity_min[i] = 0;
			thr->rx_linearity_max[i] = 40;
			thr->rx_linearity_min[i] = 0;
			thr->panel_differ_max[i] = 1400;
			thr->panel_differ_min[i] = 200;
		}

		TPD_INFO("raw_max = [%d] raw_min = [%d] \n", 12000, 2690);
		TPD_INFO("tx_linearity_max = [%d] tx_linearity_min = [%d] \n", 40, 0);
		TPD_INFO("rx_linearity_max = [%d] rx_linearity_min = [%d] \n", 40, 0);
		TPD_INFO("panel_differ_max = [%d] panel_differ_min = [%d] \n", 1400, 200);

		for (i = 0; i < channel_num; i++) {
			thr->node_valid_sc[i] = 1;
			thr->scap_cb_off_max[i] = 490;
			thr->scap_cb_off_min[i] = 0;
			thr->scap_cb_on_max[i] = 490;
			thr->scap_cb_on_min[i] = 0;
			thr->scap_rawdata_off_max[i] = 15000;
			thr->scap_rawdata_off_min[i] = 3000;
			thr->scap_rawdata_on_max[i] = 15000;
			thr->scap_rawdata_on_min[i] = 3000;
		}

		TPD_INFO("node_valid_sc = [%d] \n", 1);
		TPD_INFO("scap_cb_off_max = [%d] scap_cb_off_min = [%d] \n", 490, 0);
		TPD_INFO("scap_cb_on_max = [%d] scap_cb_on_min = [%d] \n", 490, 0);
		TPD_INFO("scap_rawdata_off_max = [%d] scap_rawdata_off_min = [%d] \n", 15000,
			 3000);
		TPD_INFO("scap_rawdata_on_max = [%d] scap_rawdata_on_min = [%d] \n", 15000,
			 3000);
	}

	return 0;
}

static int fts_get_threshold_from_img(struct chip_data_ft3518 *ts_data,
				      char *data, const struct firmware *limit_fw,
				      struct auto_testdata *p_focal_testdata)
{
	int ret = 0;
	int i = 0;
	int item_cnt = 0;
	/*uint8_t * p_print = NULL;*/
	uint32_t *p_item_offset = NULL;
	struct auto_test_header *ph = NULL;
	struct auto_test_item_header *item_head = NULL;

	ret = touch_i2c_read_byte(ts_data->client, FTS_REG_FW_VER);

	if (ret > 0x10) {
		ts_data->use_panelfactory_limit = false;

	} else {
		ts_data->use_panelfactory_limit = true;
	}

	TPD_INFO("%s, use_panelfactory_limit = %d \n", __func__,
		 ts_data->use_panelfactory_limit);

	ts_data->fts_autotest_offset = kzalloc(sizeof(struct fts_autotest_offset),
					       GFP_KERNEL);

	limit_fw = (const struct firmware *) p_focal_testdata->fw;
	ph = (struct auto_test_header *)(limit_fw->data);

	p_item_offset = (uint32_t *)(limit_fw->data + 16);

	for (i = 0; i < 8 * sizeof(ph->test_item); i++) {
		if ((ph->test_item >> i) & 0x01) {
			item_cnt++;
		}
	}

	TPD_INFO("%s: total test item = %d \n", __func__, item_cnt);

	TPD_INFO("%s: populating fts_test_offset \n", __func__);

	for (i = 0; i < item_cnt; i++) {
		TPD_INFO("%s: i[%d] \n", __func__, i);
		item_head = (struct auto_test_item_header *)(limit_fw->data + p_item_offset[i]);

		if (item_head->item_limit_type == LIMIT_TYPE_NO_DATA) {
			TPD_INFO("[%d] incorrect item type: LIMIT_TYPE_NO_DATA\n", item_head->item_bit);

		} else if (item_head->item_limit_type == LIMIT_TYPE_TOP_FLOOR_DATA) {
			if (false == ts_data->use_panelfactory_limit) {
				TPD_INFO("test item bit [%d] \n", item_head->item_bit);

				if (item_head->item_bit == TYPE_NOISE_DATA) {
					ts_data->fts_autotest_offset->fts_noise_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_noise_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_noise_data_P = %p, fts_noise_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_noise_data_P,
						 ts_data->fts_autotest_offset->fts_noise_data_N);

				} else if (item_head->item_bit == TYPE_RAW_DATA) {
					ts_data->fts_autotest_offset->fts_raw_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_raw_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_raw_data_P = %p, fts_raw_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_raw_data_P,
						 ts_data->fts_autotest_offset->fts_raw_data_N);

				} else if (item_head->item_bit == TYPE_UNIFORMITY_DATA) {
					ts_data->fts_autotest_offset->fts_uniformity_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_uniformity_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_uniformity_data_P = %p, fts_uniformity_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_uniformity_data_P,
						 ts_data->fts_autotest_offset->fts_uniformity_data_N);

				} else if (item_head->item_bit == TYPE_PANEL_DIFFER_DATA) {
					ts_data->fts_autotest_offset->fts_panel_differ_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_panel_differ_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_panel_differ_data_P = %p, fts_panel_differ_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_panel_differ_data_P,
						 ts_data->fts_autotest_offset->fts_panel_differ_data_N);
				}

			} else if (true == ts_data->use_panelfactory_limit) {
				TPD_INFO("test item bit [%d] \n", item_head->item_bit);

				if (item_head->item_bit == TYPE_FACTORY_NOISE_DATA) {
					ts_data->fts_autotest_offset->fts_noise_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_noise_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_noise_data_P = %p, fts_noise_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_noise_data_P,
						 ts_data->fts_autotest_offset->fts_noise_data_P);

				} else if (item_head->item_bit == TYPE_FACTORY_RAW_DATA) {
					ts_data->fts_autotest_offset->fts_raw_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_raw_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_raw_data_P = %p, fts_raw_data_P = %p \n",
						 ts_data->fts_autotest_offset->fts_raw_data_P,
						 ts_data->fts_autotest_offset->fts_raw_data_N);

				} else if (item_head->item_bit == TYPE_FACTORY_UNIFORMITY_DATA) {
					ts_data->fts_autotest_offset->fts_uniformity_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_uniformity_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_uniformity_data_P = %p, fts_uniformity_data_P = %p \n",
						 ts_data->fts_autotest_offset->fts_uniformity_data_P,
						 ts_data->fts_autotest_offset->fts_uniformity_data_N);

				} else if (item_head->item_bit == TYPE_FACTORY_PANEL_DIFFER_DATA) {
					ts_data->fts_autotest_offset->fts_panel_differ_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_panel_differ_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_panel_differ_data_P = %p, fts_panel_differ_data_P = %p \n",
						 ts_data->fts_autotest_offset->fts_panel_differ_data_P,
						 ts_data->fts_autotest_offset->fts_panel_differ_data_N);
				}
			}

		} else if (item_head->item_limit_type == LIMIT_TYPE_TOP_FLOOR_RX_TX_DATA) {
			if (false == ts_data->use_panelfactory_limit) {
				TPD_INFO("test item bit [%d] \n", item_head->item_bit);

				if (item_head->item_bit == TYPE_SCAP_CB_DATA) {
					ts_data->fts_autotest_offset->fts_scap_cb_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_cb_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_scap_cb_data_P = %p, fts_scap_cb_data_P = %p \n",
						 ts_data->fts_autotest_offset->fts_scap_cb_data_P,
						 ts_data->fts_autotest_offset->fts_scap_cb_data_N);

				} else if (item_head->item_bit == TYPE_SCAP_RAW_DATA) {
					ts_data->fts_autotest_offset->fts_scap_raw_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_raw_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_scap_raw_data_P = %p, fts_scap_raw_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_scap_raw_data_P,
						 ts_data->fts_autotest_offset->fts_scap_raw_data_N);

				} else if (item_head->item_bit == TYPE_SCAP_CB_WATERPROOF_DATA) {
					ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_scap_cb_data_waterproof_P = %p, fts_scap_cb_data_waterproof_N = %p \n",
						 ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P,
						 ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N);

				} else if (item_head->item_bit == TYPE_SCAP_RAW_WATERPROOF_DATA) {
					ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
					TPD_INFO("fts_scap_raw_waterproof_data_P = %p, fts_scap_raw_waterproof_data_N = %p \n",
						 ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P,
						 ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N);
				}

			} else if (true == ts_data->use_panelfactory_limit) {
				TPD_INFO("test item bit [%d] \n", item_head->item_bit);

				if (item_head->item_bit == TYPE_FACTORY_SCAP_CB_DATA) {
					ts_data->fts_autotest_offset->fts_scap_cb_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_cb_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);

				} else if (item_head->item_bit == TYPE_FACTORY_SCAP_RAW_DATA) {
					ts_data->fts_autotest_offset->fts_scap_raw_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_raw_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);

				} else if (item_head->item_bit == TYPE_FACTORY_SCAP_CB_WATERPROOF_DATA) {
					ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);

				} else if (item_head->item_bit == TYPE_FACTORY_SCAP_RAW_WATERPROOF_DATA) {
					ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P = (int32_t *)(
								limit_fw->data + item_head->top_limit_offset);
					ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N = (int32_t *)(
								limit_fw->data + item_head->floor_limit_offset);
				}
			}

		} else {
			TPD_INFO("[%d] unknown item type \n", item_head->item_bit);
		}
	}

	ret = 0;
	return ret;
}

static void fts_print_threshold(struct chip_data_ft3518 *ts_data)
{
	int tx_num = ts_data->hw_res->tx_num;
	int rx_num = ts_data->hw_res->rx_num;
	int node_num = tx_num * rx_num;
	int channel_num = tx_num + rx_num;

	TPD_INFO("noise threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_noise_data_P, node_num, rx_num);
	print_buffer(ts_data->fts_autotest_offset->fts_noise_data_N, node_num, rx_num);

	TPD_INFO("rawdata threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_raw_data_P, node_num, rx_num);
	print_buffer(ts_data->fts_autotest_offset->fts_raw_data_N, node_num, rx_num);

	TPD_INFO("uniformity threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_uniformity_data_P, node_num,
		     rx_num);
	print_buffer(ts_data->fts_autotest_offset->fts_uniformity_data_N, node_num,
		     rx_num);

	TPD_INFO("scap cb normal threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_scap_cb_data_P, channel_num,
		     channel_num);
	print_buffer(ts_data->fts_autotest_offset->fts_scap_cb_data_N, channel_num,
		     channel_num);

	TPD_INFO("scap cb waterproof threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_P,
		     channel_num, channel_num);
	print_buffer(ts_data->fts_autotest_offset->fts_scap_cb_data_waterproof_N,
		     channel_num, channel_num);

	TPD_INFO("scap rawdata threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_scap_raw_data_P, channel_num,
		     channel_num);
	print_buffer(ts_data->fts_autotest_offset->fts_scap_raw_data_N, channel_num,
		     channel_num);

	TPD_INFO("scap rawdata waterproof threshold max/min:");
	print_buffer(ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_P,
		     channel_num, channel_num);
	print_buffer(ts_data->fts_autotest_offset->fts_scap_raw_waterproof_data_N,
		     channel_num, channel_num);
}

int fts_test_entry(struct chip_data_ft3518 *ts_data,
		   struct auto_testdata *focal_testdata)
{
	int ret = 0;
	const struct firmware *limit_fw = NULL;

	FTS_TEST_FUNC_ENTER();
	FTS_TEST_SAVE_ERR("FW_VER:0x%02x, tx_num:%d, rx_num:%d\n", ts_data->fwver,
			  ts_data->hw_res->tx_num, ts_data->hw_res->rx_num);
	ret = fts_get_threshold(ts_data, NULL);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get threshold fail,ret=%d\n", ret);
		return 0xFF;
	}

	ret = fts_get_threshold_from_img(ts_data, NULL, limit_fw, focal_testdata);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("get threshold from img fail,ret=%d\n", ret);
		return 0xFF;
	}

	fts_print_threshold(ts_data);

	ret = enter_factory_mode(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("failed to enter factory mode,ret=%d\n", ret);
		ret = 0xFF;
		goto test_err;
	}

	ret = get_channel_num(ts_data);

	if (ret < 0) {
		FTS_TEST_SAVE_ERR("check channel num fail,ret=%d\n", ret);
		ret = 0xFF;
		goto test_err;
	}

	FTS_TEST_FUNC_EXIT();
	return ret;

test_err:
	enter_work_mode();
	FTS_TEST_FUNC_EXIT();
	return ret;
}
