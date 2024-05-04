/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef __FOCAL_COMMON_H__
#define __FOCAL_COMMON_H__

/*********PART1:Head files**********************/
#include <linux/firmware.h>
#include <linux/proc_fs.h>

#include "../touchpanel_common.h"
#include "../touch_comon_api/touch_comon_api.h"
#include "../touchpanel_autotest/touchpanel_autotest.h"

/*********PART2:Define Area**********************/
/*create apk debug channel*/
#define PROC_UPGRADE                            0
#define PROC_READ_REGISTER                      1
#define PROC_WRITE_REGISTER                     2
#define PROC_AUTOCLB                            4
#define PROC_UPGRADE_INFO                       5
#define PROC_WRITE_DATA                         6
#define PROC_READ_DATA                          7
#define PROC_SET_TEST_FLAG                      8
#define PROC_SET_SLAVE_ADDR                     10
#define PROC_HW_RESET                           11

#define WRITE_BUF_SIZE                          512
#define READ_BUF_SIZE                           512
#define FILE_NAME_LENGTH                        128

struct focal_testdata {
	int tx_num;
	int rx_num;
	int fd;
	int irq_gpio;
	int key_tx;
	int key_rx;
	uint64_t  tp_fw;
	const struct firmware *fw;
	bool fd_support;
	bool fingerprint_underscreen_support;
	uint64_t test_item;
};

/*test item*/
enum {
	TYPE_ERROR                                  = 0x00,
	TYPE_NOISE_DATA                             = 0x01,
	TYPE_RAW_DATA                               = 0x02,
	TYPE_UNIFORMITY_DATA                        = 0x03,
	TYPE_SCAP_CB_DATA                           = 0x04,
	TYPE_SCAP_RAW_DATA                          = 0x05,
	TYPE_SCAP_CB_WATERPROOF_DATA                = 0x06,
	TYPE_PANEL_DIFFER_DATA                      = 0x07,
	TYPE_SCAP_RAW_WATERPROOF_DATA               = 0x08,

	TYPE_FACTORY_NOISE_DATA                     = 0x15,            /*limit from panel factory*/
	TYPE_FACTORY_RAW_DATA                       = 0x16,
	TYPE_FACTORY_UNIFORMITY_DATA                = 0x17,
	TYPE_FACTORY_SCAP_CB_DATA                   = 0x18,
	TYPE_FACTORY_SCAP_RAW_DATA                  = 0x19,
	TYPE_FACTORY_SCAP_CB_WATERPROOF_DATA        = 0x1A,
	TYPE_FACTORY_PANEL_DIFFER_DATA              = 0x1B,
	TYPE_FACTORY_SCAP_RAW_WATERPROOF_DATA       = 0x1C,

	TYPE_MAX                                    = 0xFF,
};

/*test item, used for (get_test_item_info) authenticate responding test item*/
enum {
	TYPE_TEST_ERROR              = 0x00,
	TYPE_TEST1                   = 0x01,
	TYPE_TEST2                   = 0x02,
	TYPE_TEST3                   = 0x03,
	TYPE_TEST4                   = 0x04,
	TYPE_TEST5                   = 0x05,
	TYPE_TEST6                   = 0x06,
	TYPE_TEST_MAX                = 0xFF,
};

struct fts_proc_operations {
	void (*auto_test)(struct seq_file *s, void *chip_data,
			  struct focal_testdata *focal_testdata);
	void (*set_touchfilter_state)(void *chip_data, uint8_t range_size);
	uint8_t (*get_touchfilter_state)(void *chip_data);
};

struct focal_auto_test_operations {
	int (*test1)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test2)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test3)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test4)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test5)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test6)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *focal_testdata,
		     struct test_item_info *p_test_item_info);
	int (*auto_test_preoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *focal_testdata,
				      struct test_item_info *p_test_item_info);
	int (*auto_test_endoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *focal_testdata,
				      struct test_item_info *p_test_item_info);
};

/*int fts_create_proc(struct touchpanel_data *ts,
		    struct fts_proc_operations *syna_ops);
*/
/*********PART3:Struct Area**********************/
struct focal_debug_func {
	void (*esd_check_enable)(void *chip_data, bool enable);
	bool (*get_esd_check_flag)(void *chip_data);
	void (*reset)(void *chip_data, int msecond);
	int (*get_fw_version)(void *chip_data);
	int (*dump_reg_sate)(void *chip_data, char *buf);
	void (*set_grip_handle)(void *chip_data, int para_num, char *buf);
};

/*********PART4:function declare*****************/
int focal_create_sysfs(struct i2c_client *client);
int focal_create_apk_debug_channel(struct touchpanel_data *ts);
void ft_limit_read_std(struct seq_file *s, struct touchpanel_data *ts);
int focal_auto_test(struct seq_file *s,  struct touchpanel_data *ts);

#endif /*__FOCAL_COMMON_H__*/
