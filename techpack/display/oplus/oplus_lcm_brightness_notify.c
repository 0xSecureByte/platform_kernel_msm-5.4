/*******************************************************************
** Copyright (C), 2021, oplus Mobile Comm Corp., Ltd
** File: oplus_lcm_brightness_notify.c
** Description: oplus lcm brightness notify
** Version : 1.0
** Date : 2021/09/26
**
** ----------------------- Revision History: -----------------------
**  <author>        <data>        <version >        <desc>
**  Six.Xu          2021/09/26    1.0               Display notifier
*******************************************************************/
#include <linux/msm_drm_notify.h>

static BLOCKING_NOTIFIER_HEAD(lcdinfo_notifiers);
int register_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcdinfo_notifiers, nb);
}
EXPORT_SYMBOL(register_lcdinfo_notifier);

int unregister_lcdinfo_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcdinfo_notifiers, nb);
}
EXPORT_SYMBOL(unregister_lcdinfo_notifier);

void lcdinfo_notify(unsigned long val, void *v)
{
	blocking_notifier_call_chain(&lcdinfo_notifiers, val, v);
}
EXPORT_SYMBOL(lcdinfo_notify);
