// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include "../touchpanel_common.h"
#include "novatek_common.h"
#include <linux/module.h>

/*******LOG TAG Declear*****************************/
#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "nvt_common"
#else
#define TPD_DEVICE "nvt_common"
#endif

/*********** nvt tool operate content***********************/
static ssize_t nvt_flash_read(struct file *filp, char __user *buff,
			      size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;
	unsigned short addr_tmp = 0;

	struct touchpanel_data *ts = PDE_DATA(file_inode(filp));

	if (count > sizeof(str)) {
		TPD_INFO("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		TPD_INFO("copy from user error\n");
		return -EFAULT;
	}

	if (ts->esd_handle_support) {
		esd_handle_switch(&ts->esd_info, false);
	}

	i2c_wr = str[0] >> 7;

	if (i2c_wr == 0 && count > 2) {    /*I2C write*/
		while (retries < 20) {
			addr_tmp = ts->client->addr;
			ts->client->addr = str[0] & 0x7F;
			ret = touch_i2c_write(ts->client, &str[2], str[1]);
			ts->client->addr = addr_tmp;

			if (ret == 1) {
				break;

			} else {
				TPD_INFO("error, retries=%d, ret=%d\n", retries, ret);
			}

			retries++;
		}

		if (unlikely(retries == 20)) {
			TPD_INFO("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;

	} else if (i2c_wr == 1 && count > 3) {    /*I2C read*/
		while (retries < 20) {
			addr_tmp = ts->client->addr;
			ts->client->addr = str[0] & 0x7F;
			ret = touch_i2c_read(ts->client, &str[2], 1, &str[3], str[1] - 1);
			ts->client->addr = addr_tmp;

			if (ret == 2) {
				break;

			} else {
				TPD_INFO("error, retries=%d, ret=%d\n", retries, ret);
			}

			retries++;
		}

		/* copy buff to user if i2c transfer*/
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				return -EFAULT;
			}
		}

		if (unlikely(retries == 20)) {
			TPD_INFO("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;

	} else {
		TPD_INFO("Call error, str[0]=%d\n", str[0]);
		return -EFAULT;
	}
}

#if LINUX_VERSION_CODE>= KERNEL_VERSION(5, 10, 0)
static const struct proc_ops nvt_flash_fops = {
	.proc_open = simple_open,
    .proc_read = nvt_flash_read,
};
#else
static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
    .open = simple_open,
    .read = nvt_flash_read,
};
#endif

static ssize_t nvt_noflash_read(struct file *filp, char __user *buff,
				size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = -1;
	int32_t retries = 0;
	u8 spi_wr = 0;
	uint8_t *buf = NULL;
	int rw_len;

	struct touchpanel_data *ts = PDE_DATA(file_inode(filp));

	if (count > SPI_TANSFER_LEN + 2 || count < 2) {
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);

	if (str == NULL) {
		TPD_INFO("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto out;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);

	if (buf == NULL) {
		TPD_INFO("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(str, (u8 *)buff, count)) {
		TPD_INFO("copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

	if (ts->esd_handle_support) {
		esd_handle_switch(&ts->esd_info, false);
	}

	spi_wr = str[0] >> 7;
	rw_len = (str[0] & 0x7F) * 256 + str[1];

	if (rw_len + 2 > count) {
		rw_len = count - 2;
	}

	memcpy(buf, &str[2], rw_len);

	if (spi_wr == 0) {    /*SPI write*/
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->s_client, buf, rw_len);

			if (!ret) {
				break;

			} else {
				TPD_INFO("error, retries=%d, ret=%d\n", retries, ret);
			}

			retries++;
		}

		if (unlikely(retries == 20)) {
			TPD_INFO("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}

	} else if (spi_wr == 1) {    /*SPI read*/
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->s_client, buf, rw_len);

			if (!ret) {
				break;

			} else {
				TPD_INFO("error, retries=%d, ret=%d\n", retries, ret);
			}

			retries++;
		}

		/* copy buff to user if spi transfer*/
		memcpy(&str[2], buf, rw_len);

		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			TPD_INFO("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}

	} else {
		TPD_INFO("Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:

	if (str != NULL) {
		kfree(str);
	}

	if (buf != NULL) {
		kfree(buf);
	}

	return ret;
}

#if LINUX_VERSION_CODE>= KERNEL_VERSION(5, 10, 0)
static const struct proc_ops nvt_noflash_fops = {
	.proc_open = simple_open,
    .proc_read = nvt_noflash_read,
};
#else
static const struct file_operations nvt_noflash_fops = {
	.owner = THIS_MODULE,
    .open = simple_open,
    .read = nvt_noflash_read,
};
#endif

void nvt_flash_proc_init(struct touchpanel_data *ts, const char *name)
{
	struct proc_dir_entry *nvt_proc_entry;

	if (strstr(name, "SPI")) {
		TPD_INFO("create /proc/NVTSPI!\n");
		nvt_proc_entry = proc_create_data(name, 0444, NULL, &nvt_noflash_fops, ts);

		if (nvt_proc_entry == NULL) {
			TPD_INFO("%s Failed!\n", __func__);
			return;

		} else {
			TPD_INFO("%s Succeeded!\n", __func__);
		}

	} else {
		TPD_INFO("create /proc/NVTflash!\n");
		nvt_proc_entry = proc_create_data(name, 0444, NULL, &nvt_flash_fops, ts);

		if (nvt_proc_entry == NULL) {
			TPD_INFO("%s Failed!\n", __func__);
			return;

		} else {
			TPD_INFO("%s Succeeded!\n", __func__);
		}
	}

	return;
}
EXPORT_SYMBOL(nvt_flash_proc_init);

#define LEN_DOZE_FDM_ROW_DATA 2
#define LEN_TEST_ITEM_FIELD 16
void nvt_limit_read_std(struct seq_file *s, struct touchpanel_data *ts)
{
	int ret = 0, m = 0, i = 0, j = 0, item_cnt = 0;
	const struct firmware *fw = NULL;
	struct auto_test_header *ph = NULL;
	struct auto_test_item_header *item_head = NULL;
	uint32_t *p_item_offset = NULL;
	int32_t *p_data32 = NULL;

	ret = request_firmware(&fw, ts->panel_data.test_limit_name, ts->dev);

	if (ret < 0) {
		TPD_INFO("Request firmware failed - %s (%d)\n", ts->panel_data.test_limit_name,
			 ret);
		seq_printf(s, "Request failed, Check the path\n");
		return;
	}

	ph = (struct auto_test_header *)(fw->data);
	p_item_offset = (uint32_t *)(fw->data + LEN_TEST_ITEM_FIELD);

	if ((ph->magic1 != 0x494D494C) || (ph->magic2 != 0x474D4954)) {
		TPD_INFO("limit image is not generated by oplus\n");
		seq_printf(s, "limit image is not generated by oplus\n");
		release_firmware(fw);
		return;
	}

	for (i = 0; i < 8 * sizeof(ph->test_item); i++) {
		if ((ph->test_item >> i) & 0x01) {
			item_cnt++;
		}
	}

	TPD_INFO("%s: total test item = %d \n", __func__, item_cnt);

	if (!item_cnt) {
		TPD_INFO("limit image has no test item\n");
		seq_printf(s, "limit image has no test item\n");
	}

	for (m = 0; m < item_cnt; m++) {
		TPD_INFO("common debug d: p_item_offset[%d] = 0x%x \n", m, p_item_offset[m]);
		item_head = (struct auto_test_item_header *)(fw->data + p_item_offset[m]);
		TPD_INFO("common debug a\n");

		if (item_head->item_magic != 0x4F50504F) {
			TPD_INFO("item: %d limit data has some problem\n", item_head->item_bit);
			seq_printf(s, "item: %d limit data has some problem\n", item_head->item_bit);
			continue;
		}

		TPD_INFO("common debug b\n");
		TPD_INFO("item %d[size %d, limit type %d, para num %d] :\n",
			 item_head->item_bit, item_head->item_size, item_head->item_limit_type,
			 item_head->para_num);
		seq_printf(s, "item %d[size %d, limit type %d, para num %d] :\n",
			   item_head->item_bit, item_head->item_size, item_head->item_limit_type,
			   item_head->para_num);

		if (item_head->item_limit_type == LIMIT_TYPE_NO_DATA) {
			TPD_INFO("common debug e LIMIT_TYPE_NO_DATA\n");
			seq_printf(s, "no limit data\n");

		} else if (item_head->item_limit_type == LIMIT_TYPE_TOP_FLOOR_DATA) {
			TPD_INFO("common debug e LIMIT_TYPE_TOP_FLOOR_DATA\n");

			if (item_head->item_bit == TYPE_FW_RAWDATA) {
				seq_printf(s, "TYPE_FW_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_OPEN_RAWDATA) {
				seq_printf(s, "TYPE_OPEN_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_SHORT_RAWDATA) {
				seq_printf(s, "TYPE_SHORT_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_CC_DATA) {
				seq_printf(s, "TYPE_CC_DATA: \n");

			} else if (item_head->item_bit == TYPE_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_DOZE_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_DOZE_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_DOZE_RAWDATA) {
				seq_printf(s, "TYPE_DOZE_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_LPWG_RAWDATA) {
				seq_printf(s, "TYPE_LPWG_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_LPWG_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_LPWG_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_FDM_RAWDATA) {
				seq_printf(s, "TYPE_FDM_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_FDM_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_FDM_DIFF_RAWDATA: \n");
			}

			TPD_INFO("top data [%d]: \n", m);
			seq_printf(s, "top data: \n");
			p_data32 = (int32_t *)(fw->data + item_head->top_limit_offset);

			for (i = 0; i < (ts->hw_res.tx_num * ts->hw_res.rx_num); i++) {
				if (i % ts->hw_res.tx_num == 0) {
					seq_printf(s, "\n[%2d] ", (i / ts->hw_res.tx_num));
				}

				seq_printf(s, "%4d, ", p_data32[i]);
				TPD_DEBUG("%d, ", p_data32[i]);
			}

			seq_printf(s, "\nfloor data: \n");
			p_data32 = (int32_t *)(fw->data + item_head->floor_limit_offset);

			for (i = 0; i < (ts->hw_res.tx_num * ts->hw_res.rx_num); i++) {
				if (i % ts->hw_res.tx_num == 0) {
					seq_printf(s, "\n[%2d] ", (i / ts->hw_res.tx_num));
				}

				seq_printf(s, "%4d, ", p_data32[i]);
				TPD_DEBUG("%d, ", p_data32[i]);
			}
		}

		if (item_head->item_limit_type == LIMIT_TYPE_DOZE_FDM_DATA) {
			TPD_INFO("common debug e LIMIT_TYPE_TOP_FLOOR_DATA\n");

			if (item_head->item_bit == TYPE_FW_RAWDATA) {
				seq_printf(s, "TYPE_FW_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_OPEN_RAWDATA) {
				seq_printf(s, "TYPE_OPEN_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_SHORT_RAWDATA) {
				seq_printf(s, "TYPE_SHORT_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_CC_DATA) {
				seq_printf(s, "TYPE_CC_DATA: \n");

			} else if (item_head->item_bit == TYPE_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_DOZE_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_DOZE_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_DOZE_RAWDATA) {
				seq_printf(s, "TYPE_DOZE_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_LPWG_RAWDATA) {
				seq_printf(s, "TYPE_LPWG_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_LPWG_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_LPWG_DIFF_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_FDM_RAWDATA) {
				seq_printf(s, "TYPE_FDM_RAWDATA: \n");

			} else if (item_head->item_bit == TYPE_FDM_DIFF_RAWDATA) {
				seq_printf(s, "TYPE_FDM_DIFF_RAWDATA: \n");
			}

			TPD_INFO("top data [%d]: \n", m);
			seq_printf(s, "top data: \n");
			p_data32 = (int32_t *)(fw->data + item_head->top_limit_offset);
			TPD_INFO("size 1: %d * %d = %d \n", LEN_DOZE_FDM_ROW_DATA, ts->hw_res.rx_num,
				 (LEN_DOZE_FDM_ROW_DATA * ts->hw_res.rx_num));

			for (i = 0; i < (LEN_DOZE_FDM_ROW_DATA * ts->hw_res.rx_num); i++) {
				if (i % LEN_DOZE_FDM_ROW_DATA == 0) {
					seq_printf(s, "\n[%2d] ", (i / LEN_DOZE_FDM_ROW_DATA));
				}

				seq_printf(s, "%4d, ", p_data32[i]);
				TPD_DEBUG("%d, ", p_data32[i]);
			}

			seq_printf(s, "\nfloor data: \n");
			p_data32 = (int32_t *)(fw->data + item_head->floor_limit_offset);
			TPD_INFO("size 2: %d * %d = %d \n", LEN_DOZE_FDM_ROW_DATA, ts->hw_res.rx_num,
				 (LEN_DOZE_FDM_ROW_DATA * ts->hw_res.rx_num));

			for (i = 0; i < (LEN_DOZE_FDM_ROW_DATA * ts->hw_res.rx_num); i++) {
				if (i % LEN_DOZE_FDM_ROW_DATA == 0) {
					seq_printf(s, "\n[%2d] ", (i / LEN_DOZE_FDM_ROW_DATA));
				}

				seq_printf(s, "%4d, ", p_data32[i]);
				TPD_DEBUG("%d, ", p_data32[i]);
			}
		}

		TPD_INFO("common debug c\n");
		p_data32 = (int32_t *)(fw->data + p_item_offset[m] + sizeof(
					       struct auto_test_item_header));
		TPD_INFO("common debug f\n");

		if (item_head->para_num) {
			seq_printf(s, "parameter:");

			for (j = 0; j < item_head->para_num; j++) {
				seq_printf(s, "%d, ", p_data32[j]);
			}

			seq_printf(s, "\n");
		}

		seq_printf(s, "\n");
	}

	release_firmware(fw);
}
/************ nvt auto test content*************************/


static int nvt_before_autotest(struct seq_file *s, struct touchpanel_data *ts,
			       struct auto_testdata *p_nvt_testdata)
{
	const struct firmware *fw = NULL;
	struct auto_test_header *test_head = NULL;
	uint32_t *p_data32 = NULL;

	TPD_INFO("%s: enter\n", __func__);
	fw = ts->com_test_data.limit_fw;
	/*step4: decode the limit image*/
	test_head = (struct auto_test_header *)fw->data;
	p_data32 = (uint32_t *)(fw->data + 16);

	if ((test_head->magic1 != Limit_MagicNum1)
			|| (test_head->magic2 != Limit_MagicNum2)) {
		TPD_INFO("limit image is not generated by oplus\n");
		seq_printf(s, "limit image is not generated by oplus\n");
		return  -1;
	}

	TPD_INFO("current test item: %llx\n", test_head->test_item);
	p_nvt_testdata->tx_num = ts->hw_res.tx_num;
	p_nvt_testdata->rx_num = ts->hw_res.rx_num;
	p_nvt_testdata->irq_gpio = ts->hw_res.irq_gpio;
	p_nvt_testdata->tp_fw = ts->panel_data.tp_fw;
	p_nvt_testdata->fp = ts->com_test_data.result_data;
	p_nvt_testdata->length = ts->com_test_data.result_max_len;
	p_nvt_testdata->pos = &ts->com_test_data.result_cur_len;
	p_nvt_testdata->fw = fw;
	p_nvt_testdata->test_item = test_head->test_item;
	return 0;
}



static int nvt_doing_autotest(struct seq_file *s, struct touchpanel_data *ts,
			      struct auto_testdata *p_nvt_testdata)
{
	int error_count = 0;
	int ret = 0;
	struct test_item_info *p_test_item_info = NULL;
	struct nvt_auto_test_operations *nvt_test_ops = NULL;
	struct com_test_data *com_test_data_p = NULL;

	com_test_data_p = &ts->com_test_data;

	if (!com_test_data_p || !com_test_data_p->chip_test_ops) {
		TPD_INFO("%s: com_test_data is null\n", __func__);
		error_count++;
		goto END;
	}

	nvt_test_ops = (struct nvt_auto_test_operations *)
		       com_test_data_p->chip_test_ops;

	if (!nvt_test_ops->auto_test_preoperation) {
		TPD_INFO("not support gd_test_ops->auto_test_preoperation callback\n");

	} else {
		ret = nvt_test_ops->auto_test_preoperation(s, ts->chip_data, p_nvt_testdata,
				p_test_item_info);

		if (ret < 0) {
			TPD_INFO("auto_test_preoperation failed\n");
			error_count++;
			goto END;
		}
	}

	if (!nvt_test_ops->test1) {
		TPD_INFO("not support gd_test_ops->test1 callback\n");

	} else {
		ret = nvt_test_ops->test1(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test1] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->test2) {
		TPD_INFO("not support gd_test_ops->test2 callback\n");

	} else {
		ret = nvt_test_ops->test2(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test2] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);


	if (!nvt_test_ops->test3) {
		TPD_INFO("not support gd_test_ops->test3 callback\n");

	} else {
		ret = nvt_test_ops->test3(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test3] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->test4) {
		TPD_INFO("not support gd_test_ops->test4 callback\n");

	} else {
		ret = nvt_test_ops->test4(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test4] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);


	if (!nvt_test_ops->test5) {
		TPD_INFO("not support gd_test_ops->test5 callback\n");

	} else {
		ret = nvt_test_ops->test5(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test5] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->test6) {
		TPD_INFO("not support gd_test_ops->test6 callback\n");

	} else {
		ret = nvt_test_ops->test6(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test6] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->test7) {
		TPD_INFO("not support gd_test_ops->test7 callback\n");

	} else {
		ret = nvt_test_ops->test7(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test7] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->test8) {
		TPD_INFO("not support gd_test_ops->test8 callback\n");

	} else {
		ret = nvt_test_ops->test8(s, ts->chip_data, p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [test8] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!nvt_test_ops->auto_test_endoperation) {
		TPD_INFO("not support gd_test_ops->auto_test_preoperation callback\n");

	} else {
		ret = nvt_test_ops->auto_test_endoperation(s, ts->chip_data, p_nvt_testdata,
				p_test_item_info);

		if (ret < 0) {
			TPD_INFO("auto_test_endoperation failed\n");
			error_count++;
		}
	}

END:
	return error_count;
}


int nvt_auto_test(struct seq_file *s, struct touchpanel_data *ts)
{
	struct auto_testdata nvt_testdata = {
		.tx_num = 0,
		.rx_num = 0,
		.fp = NULL,
		.irq_gpio = -1,
		.tp_fw = 0,
		.fw = NULL,
		.test_item = 0,
	};
	int error_count = 0;
	int ret = 0;

	TPD_INFO("%s + \n", __func__);
	ret = nvt_before_autotest(s, ts, &nvt_testdata);

	if (ret) {
		error_count++;
		goto END;
	}

	error_count += nvt_doing_autotest(s, ts, &nvt_testdata);

END:
	seq_printf(s, "imageid = 0x%llx, deviceid = 0x%llx\n", nvt_testdata.tp_fw,
		   nvt_testdata.dev_tp_fw);
	seq_printf(s, "%d error(s). %s\n", error_count,
		   error_count ? "" : "All test passed.");
	TPD_INFO(" TP auto test %d error(s). %s\n", error_count,
		 error_count ? "" : "All test passed.");
	TPD_INFO("%s - \n", __func__);
	return error_count;
}
EXPORT_SYMBOL(nvt_auto_test);

static int nvt_before_black_screen_autotest(struct seq_file *s,
		struct touchpanel_data *ts, struct auto_testdata *p_nvt_testdata)
{
	const struct firmware *fw = NULL;
	struct auto_test_header *test_head = NULL;
	uint32_t *p_data32 = NULL;

	TPD_INFO("%s + \n", __func__);
	fw = ts->com_test_data.limit_fw;
	/*step4: decode the limit image*/
	test_head = (struct auto_test_header *)fw->data;
	p_data32 = (uint32_t *)(fw->data + 16);

	if ((test_head->magic1 != Limit_MagicNum1)
			|| (test_head->magic2 != Limit_MagicNum2)) {
		TPD_INFO("limit image is not generated by oplus\n");
		/*seq_printf(s, "limit image is not generated by oplus\n");*/
		return  -1;
	}

	TPD_INFO("current test item: %llx\n", test_head->test_item);
	p_nvt_testdata->tx_num = ts->hw_res.tx_num;
	p_nvt_testdata->rx_num = ts->hw_res.rx_num;
	p_nvt_testdata->irq_gpio = ts->hw_res.irq_gpio;
	p_nvt_testdata->tp_fw = ts->panel_data.tp_fw;
	p_nvt_testdata->fp = ts->com_test_data.bs_result_data;
	p_nvt_testdata->length = ts->com_test_data.bs_result_max_len;
	p_nvt_testdata->pos = &ts->com_test_data.bs_result_cur_len;
	p_nvt_testdata->fw = fw;
	p_nvt_testdata->test_item = test_head->test_item;
	TPD_INFO("%s - \n", __func__);
	return 0;
}


static int nvt_doing_black_screen_autotest(struct seq_file *s,
		struct touchpanel_data *ts, struct auto_testdata *p_nvt_testdata)
{
	int error_count = 0;
	int ret = 0;
	struct test_item_info *p_test_item_info = NULL;
	struct nvt_auto_test_operations *nvt_test_ops = NULL;
	struct com_test_data *com_test_data_p = NULL;

	TPD_INFO("%s + \n", __func__);
	com_test_data_p = &ts->com_test_data;

	if (!com_test_data_p || !com_test_data_p->chip_test_ops) {
		TPD_INFO("%s: com_test_data is null\n", __func__);
		error_count++;
		goto END;
	}

	nvt_test_ops = (struct nvt_auto_test_operations *)
		       com_test_data_p->chip_test_ops;

	if (!nvt_test_ops->black_screen_test_preoperation) {
		TPD_INFO("not support nvt_test_ops->black_screen_test_preoperation callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test_preoperation(s, ts->chip_data,
				p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("black_screen_test_preoperation failed\n");
			error_count++;
			goto END;
		}
	}

	if (!nvt_test_ops->black_screen_test1) {
		TPD_INFO("not support nvt_test_ops->black_screen_test1 callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test1(s, ts->chip_data, p_nvt_testdata,
						       p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [black_screen_test1] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);


	if (!nvt_test_ops->black_screen_test2) {
		TPD_INFO("not support nvt_test_ops->black_screen_test2 callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test2(s, ts->chip_data, p_nvt_testdata,
						       p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [black_screen_test2] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);



	if (!nvt_test_ops->black_screen_test3) {
		TPD_INFO("not support nvt_test_ops->black_screen_test3 callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test3(s, ts->chip_data, p_nvt_testdata,
						       p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [black_screen_test3] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);


	if (!nvt_test_ops->black_screen_test4) {
		TPD_INFO("not support nvt_test_ops->black_screen_test4 callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test4(s, ts->chip_data, p_nvt_testdata,
						       p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test [black_screen_test4] failed! ret is %d\n", ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);


	if (!nvt_test_ops->black_screen_test_endoperation) {
		TPD_INFO("not support nvt_test_ops->black_screen_test_endoperation callback\n");

	} else {
		ret = nvt_test_ops->black_screen_test_endoperation(s, ts->chip_data,
				p_nvt_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("auto_test_endoperation failed\n");
			error_count++;
		}
	}

	TPD_INFO("%s - \n", __func__);
END:
	return error_count;
}



#define LEN_BLACK_SCREEN_TEST_ALLOC 128
int nvt_black_screen_autotest(struct black_gesture_test *p,
			      struct touchpanel_data *ts)
{
	char buf[LEN_BLACK_SCREEN_TEST_ALLOC] = {0};
	struct auto_testdata nvt_testdata = {
		.tx_num = 0,
		.rx_num = 0,
		.fp = NULL,
		.irq_gpio = -1,
		.tp_fw = 0,
		.fw = NULL,
		.test_item = 0,
	};
	int error_count = 0;
	int ret = 0;

	TPD_INFO("%s + \n", __func__);
	ret = nvt_before_black_screen_autotest(NULL, ts, &nvt_testdata);

	if (ret) {
		error_count++;
		goto END;
	}

	error_count += nvt_doing_black_screen_autotest(NULL, ts, &nvt_testdata);

END:
	/*seq_printf(s, "imageid = 0x%llx, deviceid = 0x%llx\n", nvt_testdata.tp_fw, nvt_testdata.dev_tp_fw);*/
	/*seq_printf(s, "%d error(s). %s\n", error_count, error_count ? "" : "All test passed.");*/
	snprintf(p->message, MESSAGE_SIZE, "%d error(s). %s%s\n", error_count,
		 error_count ? "" : "All test passed.", buf);
	TPD_INFO("%d errors. %s", error_count, buf);
	TPD_INFO(" TP auto test %d error(s). %s\n", error_count,
		 error_count ? "" : "All test passed.");
	TPD_INFO("%s - \n", __func__);
	return error_count;
}
EXPORT_SYMBOL(nvt_black_screen_autotest);

MODULE_DESCRIPTION("Touchscreen Novatek common Driver");
MODULE_LICENSE("GPL");
