// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/sched.h>
#include <asm/pgtable-types.h>
#include <asm/uaccess.h>
#include <linux/input/mt.h>
#include <linux/input.h>

#include "touchpanel_common.h"
#include "touch_comon_api/touch_comon_api.h"
#include "tp_ioctl.h"
#include "message_list.h"

struct tp_ioc_en {
	u8 __user *buf;
	u32 size;
};

struct tp_ioc_buf {
	u8 __user *buf;
	u8 __user *msec; // s64 ktime_to_ms
	u8 __user *size;
};

#define HIDL_PROPERTY_READ_BOOL 1
#define HIDL_PROPERTY_READ_U32 2
#define HIDL_PROPERTY_COUNT_U32_ELEMS 3
#define HIDL_PROPERTY_READ_U32_ARRAY 4
#define HIDL_PROPERTY_READ_STRING_INDEX 5
#define HIDL_PROPERTY_COUNT_U8_ELEMS 6
#define HIDL_PROPERTY_READ_U8_ARRAY 7

struct tp_ioc_dts_str {
	u32 type;
	char __user *propname; //
	void __user *out_values;
	int32_t __user *ret;
	u32 index;
	u32 name_size;
	u32 size;
};

#define TP_IOC_VERSION  0x01
#define TP_IOC_INIT	(_IOR(TP_IOC_VERSION, 0, struct tp_ioc_en))
#define TP_IOC_EN	(_IOW(TP_IOC_VERSION, 1, u8))
#define TP_IOC_INT	(_IOWR(TP_IOC_VERSION, 2, struct tp_ioc_buf))
#define TP_IOC_REF	(_IOWR(TP_IOC_VERSION, 3, struct tp_ioc_buf))
#define TP_IOC_DIFF	(_IOWR(TP_IOC_VERSION, 4, struct tp_ioc_buf))
#define TP_IOC_RAW	(_IOWR(TP_IOC_VERSION, 5, struct tp_ioc_buf))
#define TP_IOC_POINT	(_IOW(TP_IOC_VERSION, 6, struct tp_ioc_buf))
#define TP_IOC_DTS	(_IOR(TP_IOC_VERSION, 7, struct tp_ioc_dts_str))
#define TP_IOC_BUS	(_IOWR(TP_IOC_VERSION, 8, struct tp_ioc_buf))
#define TP_IOC_GPIO	(_IOWR(TP_IOC_VERSION, 9, struct tp_ioc_buf))


struct touch_point_report {
	uint8_t id;
	uint8_t report_bit;
	uint16_t x;
	uint16_t y;
	uint8_t press;
	uint8_t touch_major;
	uint8_t width_major;
	uint8_t reserved;
};

struct buf_head {
	uint8_t type;
	uint8_t extern_msg[7];
};

#define MSG_BUFF_SIZE	(256)
#define IO_BUF_SIZE_256	(8)


#define REPORT_X (0x01)
#define REPORT_Y (0x02)
#define REPORT_PRESS (0x04)
#define REPORT_TMAJOR (0x08)
#define REPORT_WMAJOR (0x10)
#define REPORT_UP (0x80)
#define REPORT_DOWN_ALL (0x7F)

void touch_misc_state_change(void *p_device, enum IOC_STATE_TYPE type, int state)
{
	struct touchpanel_data *ts = p_device;
	u8 buf[2];

	if (ts && ts->en_touch_event_helper) {
		buf[0] = type;
		buf[1] = state;
		post_message(ts->msg_list, 2, TYPE_STATE, (u8 *)&buf);
	}
}

static int touch_misc_open(struct inode *inode, struct file *filp)
{
	struct touchpanel_data *ts;

	ts = container_of(filp->private_data, struct touchpanel_data, misc_device);
	filp->private_data = ts;

	if (ts->misc_opened) {
		TPD_INFO("%s: misc is opened %d\n", __func__, ts->tp_index);
		return -EBUSY;
	}
	ts->misc_opened = true;
	TPD_INFO("%s: ts is %d\n", __func__, ts);
	return 0;
}

static int touch_misc_release(struct inode *inode, struct file *filp)
{
	struct touchpanel_data *ts;
	ts = (struct touchpanel_data *)filp->private_data;
	if (ts) {
		ts->en_touch_event_helper = false;
		ts->misc_opened = false;
		clear_message_list(&ts->msg_list);
	}
	TPD_INFO("%s: ts is %d\n", __func__, ts);
	filp->private_data = NULL;
	return 0;
}

static int get_point_info_from_point_report(struct point_info *points_info, struct touch_point_report *points_report, int num)
{
	int obj_attention = 0, i = 0;
	for (i = 0; i < num; i++) {
		if ((points_report[i].report_bit)&REPORT_UP) {
			points_info[points_report[i].id].status = 0;
		} else {
			points_info[points_report[i].id].status = 1;
			obj_attention |= (1 << points_report[i].id);
		}
		if ((points_report[i].report_bit)&REPORT_X) {
			points_info[points_report[i].id].x = points_report[i].x;
		}
		if ((points_report[i].report_bit)&REPORT_Y) {
			points_info[points_report[i].id].y = points_report[i].y;
		}
		if ((points_report[i].report_bit)&REPORT_PRESS) {
			points_info[points_report[i].id].z = points_report[i].press;
		}
		if ((points_report[i].report_bit)&REPORT_TMAJOR) {
			points_info[points_report[i].id].touch_major = points_report[i].touch_major;
		}
		if ((points_report[i].report_bit)&REPORT_WMAJOR) {
			points_info[points_report[i].id].width_major = points_report[i].width_major;
		}
	}
	return obj_attention;
}

static void report_point(struct touchpanel_data *ts, struct touch_point_report *points, int num)
{
	int i = 0;
	int down_num=0;
	int obj_attention = 0;
	struct point_info points_info[MAX_FINGER_NUM];
	if (num == 0) {
		return;
	}
	mutex_lock(&ts->report_mutex);
	for (i = 0; i < num; i++) {
		if ((points[i].report_bit)&REPORT_UP) {
			input_mt_slot(ts->input_dev, points[i].id);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

			TPD_DETAIL("id %d up\n", points[i].id);
			continue;
		}
		down_num++;
		input_mt_slot(ts->input_dev, points[i].id);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
		input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

		if ((points[i].report_bit)&REPORT_X) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, points[i].x);
		}
		if ((points[i].report_bit)&REPORT_Y) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, points[i].y);
		}
		if ((points[i].report_bit)&REPORT_PRESS) {
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, points[i].press);
		}
		if ((points[i].report_bit)&REPORT_TMAJOR) {
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, points[i].touch_major);
		}
		if ((points[i].report_bit)&REPORT_WMAJOR) {
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, points[i].width_major);
		}
		TPD_DETAIL("id %d, x%d,y%d,press%d\n", points[i].id, points[i].x, points[i].y, points[i].press);
	}

	if (0 == down_num) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
	}
	input_sync(ts->input_dev);
	obj_attention = get_point_info_from_point_report(points_info, points, num);
	mutex_unlock(&ts->report_mutex);

}

static void report_point_ext(struct touchpanel_data *ts, struct touch_point_report *points, int num)
{
	int i = 0;
	int down_num = 0;
	int obj_attention = 0;
	struct point_info points_info[MAX_FINGER_NUM];
	mutex_lock(&ts->report_mutex);
	for (i = 0; i < num; i++) {
		if ((points[i].report_bit)&REPORT_UP) {
			input_mt_slot(ts->input_dev, points[i].id);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

			TPD_DETAIL("id %d up\n", points[i].id);
			continue;
		}
		down_num++;
		input_mt_slot(ts->input_dev, points[i].id);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
		input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

		if ((points[i].report_bit)&REPORT_X) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, points[i].x);
		}
		if ((points[i].report_bit)&REPORT_Y) {
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, points[i].y);
		}
		if ((points[i].report_bit)&REPORT_PRESS) {
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE, points[i].press);
		}
		if ((points[i].report_bit)&REPORT_TMAJOR) {
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, points[i].touch_major);
		}
		if ((points[i].report_bit)&REPORT_WMAJOR) {
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, points[i].width_major);
		}
		TPD_DETAIL("id %hhu, x%hu,y%hu,press%hhu\n", points[i].id, points[i].x, points[i].y, points[i].press);
	}

	if (0 == num) {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
	}
	input_sync(ts->input_dev);
	obj_attention = get_point_info_from_point_report(points_info, points, num);
	mutex_unlock(&ts->report_mutex);
}


static long ioc_init(struct touchpanel_data *ts, unsigned long arg)
{
	struct tp_ioc_en ioc_en;

	if (copy_from_user(&ioc_en,
			   (struct tp_ioc_en *)arg,
			   sizeof(struct tp_ioc_en))) {
		return -EFAULT;
	}

	TPD_DETAIL("ioc_init size is %d.\n", ioc_en.size);

	if (ioc_en.size != ts->ioc_init_size) {
		return -EFAULT;
	}

	if (copy_to_user((void __user *)ioc_en.buf,
			 ts->ioc_init_buf, ts->ioc_init_size)) {
		return -EFAULT;
	}
	return 0;

}

void points_to_user(u8 *buf,struct point_info *points)
{
	buf[0]= TYPE_POINT;
	memcpy(&buf[sizeof(struct buf_head)], points, MAX_FINGER_NUM*sizeof(struct point_info));

}

static long ioc_int(struct touchpanel_data *ts, unsigned long arg)
{
	struct tp_ioc_buf ioc_buf;
	int32_t size;
	u8 *buf = NULL;
	struct message_node *msg;
	s64 msec;
	int ret = 0;

	if (copy_from_user(&ioc_buf,
			   (struct tp_ioc_buf *)arg,
			   sizeof(struct tp_ioc_buf))) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}

	if (copy_from_user(&size,
			   ioc_buf.size,
			   sizeof(int32_t))) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}

	if (size <= 0) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}

	buf = kzalloc(MSG_BUFF_SIZE, GFP_KERNEL);

	if (buf == NULL) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}

	if (copy_from_user(buf,
			   ioc_buf.buf,
			   size)) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}
	TPD_DETAIL("ioc_int size is %d.\n", size);

	if (buf[0]==TYPE_REPORT) {
		size = size - sizeof(struct buf_head);
		size = size/sizeof(struct touch_point_report);
		report_point(ts, (struct touch_point_report *)&buf[sizeof(struct buf_head)], size);
	}

	msg = get_message(ts->msg_list, 0);
	if (msg == NULL) {
		ret = 0;
		goto IOC_INT_RETURN;
	}
	memset(buf, 0, MSG_BUFF_SIZE);
	size = sizeof(struct buf_head);
	switch (msg->type) {
	case TYPE_POINT:
		points_to_user(buf, (struct point_info *)msg->msg);
		size += sizeof(struct point_info) * MAX_FINGER_NUM;
		break;
	case TYPE_SUSPEND:
		buf[0]= TYPE_SUSPEND;
		break;
	case TYPE_RESUME:
		buf[0]= TYPE_RESUME;
		break;
	case TYPE_RESET:
		buf[0]= TYPE_RESET;
		break;
	case TYPE_STATE:
		buf[0]= TYPE_STATE;
		if (msg->len < sizeof(struct buf_head)) {
			memcpy(&buf[1], msg->msg, msg->len);
		} else {
			memcpy(&buf[1], msg->msg, sizeof(struct buf_head)-1);
		}
		break;
	default:
		break;
	}
	msec = msg->msec;
	delete_message_node(ts->msg_list, &msg);

	if (copy_to_user(ioc_buf.buf,buf,size)) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}
	if (copy_to_user(ioc_buf.size, &size, sizeof(int32_t))) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}
	if (copy_to_user(ioc_buf.msec, &msec, sizeof(s64))) {
		ret = -EFAULT;
		goto IOC_INT_RETURN;
	}
IOC_INT_RETURN:
	if (buf) {
		kfree(buf);
	}

	return ret;
}

static long ioc_point_report(struct touchpanel_data *ts, unsigned long arg)
{
	struct tp_ioc_buf ioc_buf;
	int32_t size;
	u8 *buf = NULL;
	int ret = 0;

	if (copy_from_user(&ioc_buf,
			   (struct tp_ioc_buf *)arg,
			   sizeof(struct tp_ioc_buf))) {
		ret = -EFAULT;
		goto IOC_POINT_REPORT_RETURN;
	}

	if (copy_from_user(&size,
			   ioc_buf.size,
			   sizeof(int32_t))) {
		ret = -EFAULT;
		goto IOC_POINT_REPORT_RETURN;
	}

	if (size <= 0) {
		ret = -EFAULT;
		goto IOC_POINT_REPORT_RETURN;
	}
	buf = kzalloc(size, GFP_KERNEL);

	if (buf == NULL) {
		ret = -EFAULT;
		goto IOC_POINT_REPORT_RETURN;
	}

	if (copy_from_user(buf,
			   ioc_buf.buf,
			   size)) {
		ret = -EFAULT;
		goto IOC_POINT_REPORT_RETURN;
	}
	TPD_DETAIL("ioc_int size is %d.\n", size);

	if (buf[0]==TYPE_REPORT) {
		size = size - sizeof(struct buf_head);
		size = size/sizeof(struct touch_point_report);
		report_point_ext(ts, (struct touch_point_report *)&buf[sizeof(struct buf_head)], size);
	}

IOC_POINT_REPORT_RETURN:
	if (buf) {
		kfree(buf);
	}

	return 0;
}

static long ioc_dts_read(struct touchpanel_data *ts, unsigned long arg)
{
	struct tp_ioc_dts_str dts_str;
	char *name = NULL;
	void *out_value = NULL;
	int ret =0;
	int ret_to_user = 0;
	struct device_node *np;
	int r_size;
	char *str;

	np = ts->dev->of_node;

	if (copy_from_user(&dts_str,
			   (struct tp_ioc_dts_str *)arg,
			   sizeof(struct tp_ioc_dts_str))) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}

	if (dts_str.name_size <= 0) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}

	name = kzalloc(dts_str.name_size, GFP_KERNEL);
	if (name == NULL) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}
	if (copy_from_user(name,
			   dts_str.propname,
			   dts_str.name_size)) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}

	if (dts_str.size <= 0) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}

	if (dts_str.size) {
		out_value = kzalloc(dts_str.size, GFP_KERNEL);
		if (out_value == NULL) {
			ret = -EFAULT;
			goto IOC_DTS_READ_RETURN;
		}
		memset(out_value, 0, dts_str.size);
	}

	switch (dts_str.type) {
	case HIDL_PROPERTY_READ_BOOL:
		ret_to_user = of_property_read_bool(np, name);
		TPD_DETAIL("HIDL_PROPERTY_READ_BOOL ret %d", ret_to_user);
		break;
	case HIDL_PROPERTY_READ_U32:
		if (out_value == NULL) {
			ret = -EFAULT;
			goto IOC_DTS_READ_RETURN;
		}
		ret_to_user = of_property_read_u32(np, name, (u32 *)out_value);
		TPD_DETAIL("HIDL_PROPERTY_READ_U32 ret %d, name: %s, value %d", ret_to_user, name, *(u32 *)out_value);
		break;
	case HIDL_PROPERTY_COUNT_U32_ELEMS:
		ret_to_user = of_property_count_u32_elems(np, name);
		TPD_DETAIL("HIDL_PROPERTY_COUNT_U32_ELEMS ret %d", ret_to_user);
		break;
	case HIDL_PROPERTY_READ_U32_ARRAY:
		r_size = dts_str.size/sizeof(u32);
		ret_to_user = of_property_read_u32_array(np, name,
				(u32 *)out_value,
				r_size);
		TPD_DETAIL("HIDL_PROPERTY_READ_U32_ARRAY ret %d, name: %s", ret_to_user, name);
		break;
	case HIDL_PROPERTY_READ_STRING_INDEX:
		ret_to_user = of_property_read_string_index(np, name,
				dts_str.index,
				(const char **)&str);
		r_size = strlen(str);
		if (dts_str.size < 2) {
			ret = 0;
			goto IOC_DTS_READ_RETURN;
		}
		if (r_size < dts_str.size) {
			dts_str.size = r_size + 1;
		} else {
			r_size = dts_str.size - 1;
		}
		if (out_value) {
			kfree(out_value);
		}
		out_value = kzalloc(dts_str.size, GFP_KERNEL);
		if (out_value == NULL) {
			ret = -EFAULT;
			goto IOC_DTS_READ_RETURN;
		}
		memset(out_value, 0, dts_str.size);
		memcpy(out_value, str, r_size);
		TPD_DETAIL("HIDL_PROPERTY_READ_STRING_INDEX ret %d, str %s", ret_to_user, (char *)out_value);
		break;
	case HIDL_PROPERTY_COUNT_U8_ELEMS:
		ret_to_user = of_property_count_u8_elems(np, name);
		TPD_DETAIL("HIDL_PROPERTY_COUNT_U8_ELEMS ret %d", ret_to_user);
		break;
	case HIDL_PROPERTY_READ_U8_ARRAY:
		r_size = dts_str.size/sizeof(u8);
		ret_to_user = of_property_read_u8_array(np, name,
							(u8 *)out_value,
							r_size);
		TPD_DETAIL("HIDL_PROPERTY_READ_U8_ARRAY ret %d", ret_to_user);
		break;
	default:
		break;
	}

	if (copy_to_user(dts_str.ret, &ret_to_user, sizeof(int32_t))) {
		ret = -EFAULT;
		goto IOC_DTS_READ_RETURN;
	}
	if (dts_str.size) {
		if (copy_to_user(dts_str.out_values, out_value, dts_str.size)) {
			ret = -EFAULT;
		}
	}


IOC_DTS_READ_RETURN:
	if (name) {
		kfree(name);
	}

	if (out_value) {
		kfree(out_value);
	}
	return 0;
}


static long touch_misc_ioctl(struct file *filp,
			     unsigned int cmd,
			     unsigned long arg)
{
	struct touchpanel_data *ts;
	int ret = 0;

	ts = (struct touchpanel_data *)filp->private_data;
	if (!ts) {
		return -ENODEV;
	}

	switch (cmd) {
	case TP_IOC_INIT:
		TPD_DETAIL("TP_IOC_INIT  start!!!!");
		ret = ioc_init(ts, arg);
		break;
	case TP_IOC_EN:
		TPD_DETAIL("TP_IOC_EN  start!!!!");
		if (copy_from_user(&ts->en_touch_event_helper,
				   (struct tp_ioc_en *)arg,
				   sizeof(u8))) {
			ret = -EFAULT;
		}
		break;
	case TP_IOC_INT:
		TPD_DETAIL("TP_IOC_INT  start!!!!");
		ret = ioc_int(ts, arg);
		break;
	case TP_IOC_REF:
		break;
	case TP_IOC_DIFF:
		break;
	case TP_IOC_RAW:
		break;
	case TP_IOC_POINT:
		ret = ioc_point_report(ts, arg);
		break;
	case TP_IOC_DTS:
		TPD_DETAIL("TP_IOC_DTS  start!!!!");
		ret = ioc_dts_read(ts, arg);
		break;
	default:
		ret = -EOPNOTSUPP;
		TPD_INFO("cmd not support:0x%08X\n", cmd);
		break;
	}

	return ret;
}



static struct file_operations touch_misc_fops = {
	.owner = THIS_MODULE,
	.open = touch_misc_open,
	.unlocked_ioctl = touch_misc_ioctl,
	.release = touch_misc_release,
};

void init_touch_misc_device(void *p_device)
{
	struct touchpanel_data *ts = (struct touchpanel_data *)p_device;
	char *namep;
	struct device_node *np;
	/*int rc = 0;*/
	np = ts->dev->of_node;

	// read dts set
	/*ts->ioc_init_size = of_property_count_u8_elems(np, "ioc_init_buf");

	if (ts->ioc_init_size <= 0) {
		TPD_INFO("No ioc buf in dts !\n");
		return;

	} else {
		TPD_INFO("The ioc buf len %d!\n", ts->ioc_init_size);
		ts->ioc_init_buf = kzalloc(ts->ioc_init_size, GFP_KERNEL);

		if (ts->ioc_init_buf == NULL) {
			return;
		}
		rc = of_property_read_u8_array(np, "ioc_init_buf",
					       (u8 *)ts->ioc_init_buf,
					       ts->ioc_init_size);

		if (rc) {
			TPD_INFO("Can not get the ioc init config in dts!\n");
			kfree(ts->ioc_init_buf);
			ts->ioc_init_buf = NULL;
			ts->ioc_init_size = 0;
			return;
		}
	}
	TPD_INFO("ioc_init_size is %d.\n", ts->ioc_init_size);*/
	if (!of_property_read_bool(np, "enable_touch_helper")) {
		return;
	}

	namep = kzalloc(TP_NAME_SIZE_MAX, GFP_KERNEL);
	if (!namep) {
		return;
	}

	if (ts->tp_index == 0) {
		snprintf(namep, TP_NAME_SIZE_MAX, "%s", "tp_misc");

	} else {
		snprintf(namep, TP_NAME_SIZE_MAX, "%s%d", "tp_misc", ts->tp_index);
	}

	ts->msg_list = init_message_list(TP_MSG_SIZE_MAX, IO_BUF_SIZE_256, namep);

	ts->misc_device.minor = MISC_DYNAMIC_MINOR;
	ts->misc_device.fops = &touch_misc_fops;

	ts->misc_device.name = namep;
	TPD_INFO("%s: ts misc register ok", __func__);
	misc_register(&ts->misc_device);

}

