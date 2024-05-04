// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */


#ifndef _MESSAGE_LIST_H_
#define _MESSAGE_LIST_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/slab.h>



#define MESSAGE_LIST_DEBUG

#ifndef MESSAGE_LIST_DEBUG
#define MESSAGE_DBG(fmt, arg...) do {} while (0)
#define MESSAGE_LOG(fmt, arg...) do {} while (0)
#else
#define MESSAGE_DBG(fmt, arg...) pr_info("[MSG_LIST]"fmt, ##arg)
#define MESSAGE_LOG(fmt, arg...) pr_err("[MSG_LIST]"fmt, ##arg)
#endif


#define MESSAGE_ERR(fmt, arg...) pr_err("[MSG_LIST_ERR]"fmt, ##arg)


struct message_node {
	s64			msec;
	int			len;
	int			type;
	u8			*msg;
	u32			used_bit;
	struct list_head	node;
};

#define MAX_NAME_SIZE		(32)
#define MAX_STATIC_SIZE		(32)
struct message_list {
	char			*name;
	int			size;
	int			max_size;
	u32			memory_bit;
	u8			*memory_buf;
	u32			block_size;
	wait_queue_head_t	waiter;
	spinlock_t		lock;
	struct list_head	head;
};

bool post_message(struct message_list *list, int len, int type, u8 *msg);

struct message_node *get_message(struct message_list *list, int timeout);
void delete_message_node(struct message_list *list, struct message_node **msg);

struct message_list * init_message_list(int max_size,
					u32 block_size,
					const char *name);

bool clear_message_list(struct message_list **list);

bool delete_message_list(struct message_list **list);
#endif

