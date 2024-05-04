// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */


#ifndef _TP_IOCTL_H_
#define _TP_IOCTL_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/slab.h>

enum BUF_TYPE
{
    BUF_TYPE_NONE,
    TYPE_POINT,
    TYPE_RAW,
    TYPE_DIFF,
    TYPE_SUSPEND,
    TYPE_RESUME,
    TYPE_RESET,
    TYPE_REPORT,
    TYPE_STATE,
};

enum IOC_STATE_TYPE
{
	IOC_STATE_NONE,
	IOC_STATE_DIR,
	IOC_STATE_CHARGER,
	IOC_STATE_WIRELESS_CHARGER,
	IOC_STATE_GAME,
	IOC_STATE_DEBUG_LEVEL,
	IOC_STATE_PREVENTION_PARA_CHANGE,
};

void touch_misc_state_change(void *p_device, enum IOC_STATE_TYPE type, int state);

void init_touch_misc_device(void *p_device);

#endif

