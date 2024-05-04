/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef HIMAX_COMMON_H
#define HIMAX_COMMON_H

/*********PART1:Head files**********************/
#include <linux/firmware.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <linux/time.h>

#include "../touchpanel_common.h"
#include "../touch_comon_api/touch_comon_api.h"
#include "../touchpanel_autotest/touchpanel_autotest.h"
#include "../touchpanel_healthinfo/touchpanel_healthinfo.h"

/*********PART2:Define Area**********************/
#define NO_ERR                0
#define READY_TO_SERVE        1
#define WORK_OUT              2
#define I2C_FAIL              -1
#define MEM_ALLOC_FAIL        -2
#define CHECKSUM_FAIL         -3
#define GESTURE_DETECT_FAIL   -4
#define INPUT_REGISTER_FAIL   -5
#define FW_NOT_READY          -6
#define LENGTH_FAIL           -7
#define OPEN_FILE_FAIL        -8
#define ERR_WORK_OUT          -10
/*********PART3:Struct Area**********************/

struct test_header {
	unsigned int magic1;
	unsigned int magic2;
	unsigned int with_cbc;
	unsigned int array_limit_offset;
	unsigned int array_limit_size;
	unsigned int array_limitcbc_offset;
	unsigned int array_limitcbc_size;
};

struct himax_proc_operations {
	int (*test_prepare)(void *chip_data, struct auto_testdata *hx_testdata);
	void (*test_finish)(void *chip_data);
	int (*int_pin_test)(struct seq_file *s, void *chip_data,
			    struct auto_testdata *hx_testdata);
	int (*self_test)(struct seq_file *s, void *chip_data,
			 struct auto_testdata *hx_testdata);
	int (*blackscreen_test)(void *chip_data, char *message, int msg_size,
				struct auto_testdata *hx_testdata);
	size_t (*himax_proc_register_write)(struct file *file, const char *buff,
					    size_t len, loff_t *pos);
	size_t (*himax_proc_register_read)(struct file *file, char *buff, size_t len,
					   loff_t *pos);
	size_t (*himax_proc_diag_write)(struct file *file, const char *buff,
					size_t len, loff_t *pos);
	size_t (*himax_proc_diag_read)(struct file *file, char *buff, size_t len,
				       loff_t *pos);
	size_t (*himax_proc_DD_debug_read)(struct file *file, char *buf, size_t len,
					   loff_t *pos);
	size_t (*himax_proc_DD_debug_write)(struct file *file, const char *buff,
					    size_t len, loff_t *pos);
	size_t (*himax_proc_FW_debug_read)(struct file *file, char *buff, size_t len,
					   loff_t *pos);
	size_t (*himax_proc_reset_write)(struct file *file, const char *buff,
					 size_t len, loff_t *pos);
	size_t (*himax_proc_sense_on_off_write)(struct file *file, const char *buff,
						size_t len, loff_t *pos);
};

int  himax_create_proc(struct touchpanel_data *ts,
		       struct himax_proc_operations *hx_ops);

int hx_auto_test(struct seq_file *s,  struct touchpanel_data *ts);
int hx_black_screen_test(struct black_gesture_test *p_black_gesture_test,
			 struct touchpanel_data *ts);
#endif  /*HIMAX_COMMON_H*/
