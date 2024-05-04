/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#ifndef _TOUCHPANEL_COMMON_ILITEK_H_
#define _TOUCHPANEL_COMMON_ILITEK_H_

#include <linux/uaccess.h>
#include "../touchpanel_common.h"
#include "../touch_comon_api/touch_comon_api.h"
#include "../touchpanel_autotest/touchpanel_autotest.h"
#include "../touchpanel_healthinfo/touchpanel_healthinfo.h"

#include <linux/proc_fs.h>

/****************************PART1:auto test define*************************************/

struct ilitek_test_operations {
	int (*auto_test_preoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *testdata);
	int (*auto_test_endoperation)(struct seq_file *s, void *chip_data,
				      struct auto_testdata *testdata);
	int (*black_screen_preoperation)(char *msg, int msg_size, void *chip_data,
					 struct auto_testdata *testdata);
};


/****************************PART3:FUNCTION*************************************/

int ilitek_auto_test(struct seq_file *s,  struct touchpanel_data *ts);
int ilitek_black_screen_test(struct black_gesture_test *p,
			     struct touchpanel_data *ts);
#endif /*_TOUCHPANEL_COMMON_ILITEK_H_*/

