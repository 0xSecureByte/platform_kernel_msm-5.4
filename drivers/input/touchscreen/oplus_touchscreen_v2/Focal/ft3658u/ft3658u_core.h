/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef __FT3658U_CORE_H__
#define __FT3658U_CORE_H__

/*********PART1:Head files**********************/
#include <linux/i2c.h>
#include <linux/vmalloc.h>
#include <linux/input/mt.h>
#include <linux/input.h>
#include "../focal_common.h"

/*********PART2:Define Area**********************/

#define RESET_TO_NORMAL_TIME                    200        /*Sleep time after reset*/
#define POWEWRUP_TO_RESET_TIME                  10

#define INTERVAL_READ_REG                       200  /* unit:ms */
#define TIMEOUT_READ_REG                        1000 /* unit:ms */

#define FTS_VAL_CHIP_ID                         0x56
#define FTS_VAL_CHIP_ID2                        0x52
#define FTS_VAL_BL_ID                           0x56
#define FTS_VAL_BL_ID2                          0xB2



#define FTS_REG_SMOOTH_LEVEL                    0x85
#define FTS_REG_GAME_MODE_EN                    0xC3
#define FTS_REG_REPORT_RATE                     0x88/*0x12:180hz, 0x0C:120hz*/
#define FTS_REG_HIGH_FRAME_EN                   0x8E
#define FTS_REG_HIGH_FRAME_TIME                 0x8A
#define FTS_REG_CHARGER_MODE_EN                 0x8B
#define FTS_REG_EDGE_LIMIT                      0x8C
#define FTS_REG_SENSITIVE_LEVEL                 0xBA
#define FTS_REG_HEADSET_MODE_EN                 0xC4
#define FTS_REG_FOD_EN                          0xCF
#define FTS_REG_FOD_INFO                        0xE1
#define FTS_REG_FOD_INFO_LEN                    9

#define FTS_REG_INT_CNT                         0x8F
#define FTS_REG_FLOW_WORK_CNT                   0x91
#define FTS_REG_CHIP_ID                         0xA3
#define FTS_REG_CHIP_ID                         0xA3
#define FTS_REG_CHIP_ID2                        0x9F
#define FTS_REG_POWER_MODE                      0xA5
#define FTS_REG_FW_VER                          0xA6
#define FTS_REG_VENDOR_ID                       0xA8
#define FTS_REG_GESTURE_EN                      0xD0
#define FTS_REG_GESTURE_CONFIG1                 0xD1
#define FTS_REG_GESTURE_OUTPUT_ADDRESS          0xD3
#define FTS_REG_MODULE_ID                       0xE3
#define FTS_REG_LIC_VER                         0xE4
#define FTS_REG_AUTOCLB_ADDR                    0xEE
#define FTS_REG_SAMSUNG_SPECIFAL                0xFA
#define FTS_REG_HEALTH_1                        0xFD
#define FTS_REG_HEALTH_2                        0xFE


#define FTS_MAX_POINTS_SUPPORT                  10
#define FTS_MAX_ID                              0x0A
#define FTS_POINTS_ONE                          21  /*2 + 6*3 + 1*/
#define FTS_POINTS_TWO                          41  /*8*10 - 1*/
#define FTS_MAX_POINTS_LENGTH          ((FTS_POINTS_ONE) + (FTS_POINTS_TWO))
#define FTS_REG_POINTS                          0x01
#define FTS_REG_POINTS_N                        (FTS_POINTS_ONE + 1)
#define FTS_REG_POINTS_LB                       0x3E


#define FTS_GESTURE_DATA_LEN                    28


#define BYTES_PER_TIME                          (128)  /* max:128 */

/*
 * factory test registers
 */
#define ENTER_WORK_FACTORY_RETRIES              5
#define DEVIDE_MODE_ADDR                        0x00
#define FTS_FACTORY_MODE_VALUE                  0x40
#define FTS_WORK_MODE_VALUE                     0x00
#define FACTORY_TEST_RETRY                      50
#define FACTORY_TEST_DELAY                      18
#define FACTORY_TEST_RETRY_DELAY                100

/* mc_sc */
#define FACTORY_REG_LINE_ADDR                   0x01
#define FACTORY_REG_CHX_NUM                     0x02
#define FACTORY_REG_CHY_NUM                     0x03
#define FACTORY_REG_CLB                         0x04
#define FACTORY_REG_DATA_SELECT                 0x06
#define FACTORY_REG_FRE_LIST                    0x0A
#define FACTORY_REG_TOUCH_THR                   0x0D
#define FACTORY_REG_NORMALIZE                   0x16
#define FACTORY_REG_MAX_DIFF                    0x1B
#define FACTORY_REG_FRAME_NUM                   0x1C

#define FACTORY_REG_RAWDATA_ADDR_MC_SC          0x36
#define FACTORY_REG_FIR                         0xFB
#define FACTORY_REG_WC_SEL                      0x09
#define FACTORY_REG_MC_SC_MODE                  0x44
#define FACTORY_REG_HC_SEL                      0x0F
#define FACTORY_REG_MC_SC_CB_ADDR_OFF           0x45
#define FACTORY_REG_MC_SC_CB_ADDR               0x4E
#define FACTROY_REG_SHORT_TEST_EN               0x07
#define FACTROY_REG_SHORT_CA                    0x01
#define FACTROY_REG_SHORT_CC                    0x02
#define FACTROY_REG_SHORT_CG                    0x03
#define FACTROY_REG_SHORT_OFFSET                0x04
#define FACTROY_REG_SHORT_AB_CH                 0x58
#define FACTROY_REG_SHORT_DELAY                 0x5A
#define FACTORY_REG_SHORT_ADDR_MC               0xF4

#define FACTROY_REG_SHORT2_TEST_EN              0xC0
#define FACTROY_REG_SHORT2_CA                   0x01
#define FACTROY_REG_SHORT2_CC                   0x02
#define FACTROY_REG_SHORT2_CG                   0x03
#define FACTROY_REG_SHORT2_OFFSET               0x04
#define FACTROY_REG_SHORT2_RES_LEVEL            0xC1
#define FACTROY_REG_SHORT2_DEALY                0xC2
#define FACTROY_REG_SHORT2_TEST_STATE           0xC3
#define FACTORY_REG_SHORT2_ADDR_MC              0xC4
#define FACTROY_REG_SHORT2_AB_CH                0xC6

#define FTX_MAX_COMMMAND_LENGTH                 16

#define TEST_RETVAL_00                          0x00
#define TEST_RETVAL_AA                          0xAA

#define FTS_EVENT_FOD                           0x26

#define MAX_PACKET_SIZE                         128
#define FTS_120HZ_REPORT_RATE                   0x00
#define FTS_180HZ_REPORT_RATE                   0x01
#define FTS_360HZ_REPORT_RATE                   0x02
#define FTS_720HZ_REPORT_RATE                   0x03

#define FTS_GET_RATE_0                       0
#define FTS_GET_RATE_180                     180
#define FTS_GET_RATE_300                     300
#define FTS_GET_RATE_600                     600


struct fts_autotest_offset {
	int32_t *fts_raw_data_P;
	int32_t *fts_raw_data_N;
	int32_t *fts_noise_data_P;
	int32_t *fts_noise_data_N;
	int32_t *fts_uniformity_data_P;
	int32_t *fts_uniformity_data_N;
	int32_t *fts_scap_cb_data_P;
	int32_t *fts_scap_cb_data_N;
	int32_t *fts_scap_cb_data_waterproof_P;
	int32_t *fts_scap_cb_data_waterproof_N;
	int32_t *fts_scap_raw_data_P;
	int32_t *fts_scap_raw_data_N;
	int32_t *fts_scap_raw_waterproof_data_P;
	int32_t *fts_scap_raw_waterproof_data_N;
};


struct fts_fod_info {
	u8 fp_id;
	u8 event_type;
	u8 fp_area_rate;
	u8 tp_area;
	u16 fp_x;
	u16 fp_y;
	u8 fp_down;
	u8 fp_down_report;
};

struct ftxxxx_proc {
	struct proc_dir_entry *proc_entry;
	u8 opmode;
	u8 cmd_len;
	u8 cmd[FTX_MAX_COMMMAND_LENGTH];
};

struct chip_data_ft3658u {
	bool esd_check_need_stop;   /*true:esd check do nothing*/
	bool esd_check_enabled;
	bool use_panelfactory_limit;
	bool prc_mode;
	u8 rbuf[FTS_MAX_POINTS_LENGTH];
	u8 irq_type;
	u8 fwver;
	u8 touch_direction;
	u8 fp_en;
	u8 fp_down;

	int rl_cnt;
	int scb_cnt;
	int srawdata_cnt;
	int last_mode;
	int csv_fd;
	int irq_num;
	int probe_done;
	int *noise_rawdata;
	int *rawdata;
	int *scap_cb;
	int *scap_rawdata;
	int *rawdata_linearity;
	int tp_index;
	int *node_valid;
	int *node_valid_sc;

	char *test_limit_name;
	char *fw_name;
#ifdef FTS_KIT
	struct focal_auto_test_operations *auto_test_ops;
#endif
	tp_dev tp_type;             /*tp_devices.h*/

	struct i2c_client *client;
	struct device *dev;
	struct hw_resource *hw_res;
	struct ftxxxx_proc proc;
	struct fts_fod_info fod_info;
	struct seq_file *s;
	struct fts_autotest_offset *fts_autotest_offset;
	struct touchpanel_data *ts;
	struct delayed_work prc_work;
	struct workqueue_struct *ts_workqueue;
	unsigned long intr_jiffies;
	bool high_resolution_support;
	bool high_resolution_support_x8;
	bool switch_game_rate_support;
	uint16_t game_rate;
};


extern struct chip_data_ft3658u *g_fts3658u_data;

int fts3658u_test_entry(struct chip_data_ft3658u *ts_data,
                   struct auto_testdata *focal_testdata);
int ft3658u_auto_preoperation(struct seq_file *s, void *chip_data,
                              struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_noise_autotest(struct seq_file *s, void *chip_data,
                           struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_rawdata_autotest(struct seq_file *s, void *chip_data,
                             struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_uniformity_autotest(struct seq_file *s, void *chip_data,
                                struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_scap_cb_autotest(struct seq_file *s, void *chip_data,
                             struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_scap_rawdata_autotest(struct seq_file *s, void *chip_data,
                                  struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_short_test(struct seq_file *s, void *chip_data,
                       struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);
int ft3658u_auto_endoperation(struct seq_file *s, void *chip_data,
                              struct auto_testdata *focal_testdata, struct test_item_info *p_test_item_info);


#endif /*__FT3658U_CORE_H__*/
