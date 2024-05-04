/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef NOVA_H
#define NOVA_H

/*********PART1:Head files**********************/
#include <linux/firmware.h>
#include <linux/proc_fs.h>

#include "../touchpanel_common.h"
#include "../touch_comon_api/touch_comon_api.h"
#include "../touchpanel_autotest/touchpanel_autotest.h"
#include "../touchpanel_healthinfo/touchpanel_healthinfo.h"

/*********PART2:Define Area**********************/
struct nvt_testdata {
	int tx_num;
	int rx_num;
	int fd;
	int irq_gpio;
	int key_tx;
	int key_rx;
	uint64_t  tp_fw;
	const struct firmware *fw;
};

struct nvt_test_header {
	unsigned int magic1;
	unsigned int magic2;
	/*normal mode test*/
	unsigned int array_fw_rawdata_p_offset;
	unsigned int array_fw_rawdata_n_offset;
	unsigned int array_open_rawdata_p_offset;
	unsigned int array_open_rawdata_n_offset;
	signed int   config_lmt_short_rawdata_p;
	signed int   config_lmt_short_rawdata_n;
	unsigned int config_diff_test_frame;
	signed int   config_lmt_fw_diff_p;
	signed int   config_lmt_fw_diff_n;
	signed int   config_lmt_fw_cc_p;
	signed int   config_lmt_fw_cc_n;
	/*doze mode test*/
	unsigned int doze_x_channel;
	signed int   config_lmt_doze_rawdata_p;
	signed int   config_lmt_doze_rawdata_n;
	unsigned int config_doze_noise_test_frame;
	signed int   config_lmt_doze_diff_p;
	signed int   config_lmt_doze_diff_n;
	/*lpwg mode test*/
	signed int   config_lmt_lpwg_rawdata_p;
	signed int   config_lmt_lpwg_rawdata_n;
	signed int   config_lmt_lpwg_diff_p;
	signed int   config_lmt_lpwg_diff_n;
	/*fdm mode test*/
	unsigned int fdm_x_channel;
	signed int   config_lmt_fdm_rawdata_p;
	signed int   config_lmt_fdm_rawdata_n;
	unsigned int config_fdm_noise_test_frame;
	signed int   config_lmt_fdm_diff_p;
	signed int   config_lmt_fdm_diff_n;
	/*offset*/
	unsigned int   array_short_rawdata_p_offset;
	unsigned int   array_short_rawdata_n_offset;
	unsigned int   array_fw_cc_p_offset;
	unsigned int   array_fw_cc_n_offset;
	unsigned int   array_fw_diff_p_offset;
	unsigned int   array_fw_diff_n_offset;
	unsigned int   array_doze_diff_p_offset;
	unsigned int   array_doze_diff_n_offset;
	unsigned int   array_doze_rawdata_p_offset;
	unsigned int   array_doze_rawdata_n_offset;
	unsigned int   array_lpwg_rawdata_p_offset;
	unsigned int   array_lpwg_rawdata_n_offset;
	unsigned int   array_lpwg_diff_p_offset;
	unsigned int   array_lpwg_diff_n_offset;
	unsigned int   array_fdm_diff_p_offset;
	unsigned int   array_fdm_diff_n_offset;
	unsigned int   array_fdm_rawdata_p_offset;
	unsigned int   array_fdm_rawdata_n_offset;
	/*reserve space*/
	signed int   reserve[16];
};

/*********PART3:Struct Area**********************/
struct nvt_proc_operations {
	void (*auto_test)(struct seq_file *s, void *chip_data,
			  struct nvt_testdata *nvt_testdata);
};



/*********PART4:function declare*****************/
int nvt_create_proc(struct touchpanel_data *ts,
		    struct nvt_proc_operations *nvt_ops);
void nvt_flash_proc_init(struct touchpanel_data *ts, const char *name);
void nvt_limit_read(struct seq_file *s, struct touchpanel_data *ts);
void nvt_limit_read_std(struct seq_file *s, struct touchpanel_data *ts);
int nvt_auto_test(struct seq_file *s,  struct touchpanel_data *ts);
int nvt_black_screen_autotest(struct black_gesture_test *p,
			      struct touchpanel_data *ts);

/*********PART5:new auto test define*************************************/
#define Limit_MagicNum1     0x494D494C
#define Limit_MagicNum2     0x474D4954
#define Limit_MagicItem     0x4F50504F


struct nvt_autotest_para {
	signed int   config_lmt_short_rawdata_p;
	signed int   config_lmt_short_rawdata_n;
	unsigned int config_diff_test_frame;
	signed int   config_lmt_fw_diff_p;
	signed int   config_lmt_fw_diff_n;
	signed int   config_lmt_fw_cc_p;
	signed int   config_lmt_fw_cc_n;
	signed int   config_lmt_fw_digital_p;
	signed int   config_lmt_fw_digital_n;
	/*doze mode test*/
	unsigned int doze_x_channel;
	signed int   config_lmt_doze_rawdata_p;
	signed int   config_lmt_doze_rawdata_n;
	unsigned int config_doze_noise_test_frame;
	signed int   config_lmt_doze_diff_p;
	signed int   config_lmt_doze_diff_n;
	/*lpwg mode test*/
	signed int   config_lmt_lpwg_rawdata_p;
	signed int   config_lmt_lpwg_rawdata_n;
	signed int   config_lmt_lpwg_diff_p;
	signed int   config_lmt_lpwg_diff_n;
	/*fdm mode test*/
	unsigned int fdm_x_channel;
	signed int   config_lmt_fdm_rawdata_p;
	signed int   config_lmt_fdm_rawdata_n;
	unsigned int config_fdm_noise_test_frame;
	signed int   config_lmt_fdm_diff_p;
	signed int   config_lmt_fdm_diff_n;
};

struct nvt_autotest_offset {
	int32_t *fw_rawdata_p;
	int32_t *fw_rawdata_n;
	int32_t *open_rawdata_p;
	int32_t *open_rawdata_n;
	int32_t *short_rawdata_p;
	int32_t *short_rawdata_n;
	int32_t *diff_rawdata_p;
	int32_t *diff_rawdata_n;
	int32_t *digital_diff_p;
	int32_t *digital_diff_n;
	int32_t *cc_data_p;
	int32_t *cc_data_n;
	int32_t *doze_rawdata_p;
	int32_t *doze_rawdata_n;
	int32_t *doze_diff_rawdata_p;
	int32_t *doze_diff_rawdata_n;
	int32_t *lpwg_rawdata_p;
	int32_t *lpwg_rawdata_n;
	int32_t *lpwg_diff_rawdata_p;
	int32_t *lpwg_diff_rawdata_n;
	int32_t *fdm_rawdata_p;
	int32_t *fdm_rawdata_n;
	int32_t *fdm_diff_rawdata_p;
	int32_t *fdm_diff_rawdata_n;
};

struct nvt_auto_test_operations {
	int (*test1)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test2)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test3)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test4)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test5)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test6)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test7)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*test8)(struct seq_file *s, void *chip_data,
		     struct auto_testdata *nvt_testdata,
		     struct test_item_info *p_test_item_info);
	int (*black_screen_test1)(struct seq_file *s, void *chip_data,
				  struct auto_testdata *nvt_testdata,
				  struct test_item_info *p_test_item_info);
	int (*black_screen_test2)(struct seq_file *s, void *chip_data,
				  struct auto_testdata *nvt_testdata,
				  struct test_item_info *p_test_item_info);
	int (*black_screen_test3)(struct seq_file *s, void *chip_data,
				  struct auto_testdata *nvt_testdata,
				  struct test_item_info *p_test_item_info);
	int (*black_screen_test4)(struct seq_file *s, void *chip_data,
				  struct auto_testdata *nvt_testdata,
				  struct test_item_info *p_test_item_info);
	int (*auto_test_preoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *nvt_testdata,
				      struct test_item_info *p_test_item_info);
	int (*auto_test_endoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *nvt_testdata,
				      struct test_item_info *p_test_item_info);
	int (*black_screen_test_preoperation)(struct seq_file *s, void *chip_data,
					      struct auto_testdata *nvt_testdata,
					      struct test_item_info *p_test_item_info);
	int (*black_screen_test_endoperation)(struct seq_file *s, void *chip_data,
					      struct auto_testdata *nvt_testdata,
					      struct test_item_info *p_test_item_info);
};


/*test item*/
enum {
	TYPE_ERROR                              = 0x00,
	TYPE_FW_RAWDATA                         = 0x01,
	TYPE_OPEN_RAWDATA                       = 0x02,
	TYPE_SHORT_RAWDATA                      = 0x03,
	TYPE_CC_DATA                            = 0x04,
	TYPE_DIFF_RAWDATA                       = 0x05,
	TYPE_DOZE_DIFF_RAWDATA                  = 0x06,
	TYPE_DOZE_RAWDATA                       = 0x07,
	TYPE_LPWG_RAWDATA                       = 0x08,
	TYPE_LPWG_DIFF_RAWDATA                  = 0x09,
	TYPE_FDM_RAWDATA                        = 0x0A,
	TYPE_FDM_DIFF_RAWDATA                   = 0x0B,
	TYPE_DIGITAL_DIFF                       = 0x0C,
	TYPE_MAX                                = 0xFF,
};


#endif /*NOVA_H*/
