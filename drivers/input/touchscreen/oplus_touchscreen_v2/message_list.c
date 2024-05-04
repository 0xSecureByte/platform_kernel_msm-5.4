// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */


#include <linux/sched.h>
#include "message_list.h"

u32 find_bit(u32 value)
{
	int i;
	for (i = 0; i < MAX_STATIC_SIZE; i++) {
		if ((value&0x01) == 1) {
			break;
		}
		value = value>>1;
	}
	return i;
}

bool post_message(struct message_list *list, int len, int type, u8 *msg)
{
	struct message_node *node;
	ktime_t kt;
	unsigned long flags;

	if (NULL == list) {
		return false;
	}

	node = kzalloc(sizeof(struct message_node), GFP_KERNEL);
	if (NULL == node || IS_ERR(node)) {
		MESSAGE_LOG("%s post fail.", list->name);
		return false;
	}
	MESSAGE_DBG("post_message:lock list 0x%x,node addr 0x%x node 0x%x", list, (u64)&node, (u64)node);
	spin_lock_irqsave(&list->lock, flags);
	if (list->size >= list->max_size) {
		kfree(node);
		MESSAGE_ERR("%s list is full.", list->name);
		spin_unlock_irqrestore(&list->lock, flags);
		MESSAGE_DBG("post_message: unlock list 0x%x,node 0x%x", list, (u64)node);
		return false;
	}

	if (len > 0) {
		node->used_bit = find_bit(list->memory_bit);
		if (node->used_bit >= list->max_size) {
			kfree(node);
			MESSAGE_ERR("%s post fail useb bit %d.", list->name, node->used_bit);
			spin_unlock_irqrestore(&list->lock, flags);
			MESSAGE_DBG("post_message: unlock list 0x%x,node 0x%x", list, (u64)node);
			return false;
		}
		node->msg = &list->memory_buf[node->used_bit*list->block_size];
		memset(node->msg, 0, list->block_size);
		node->used_bit = 1 << node->used_bit;
		list->memory_bit &= (~node->used_bit);

		memcpy(node->msg, msg, len);
	}

	node->len = len;
	node->type = type;
	kt = ktime_get();
	node->msec = (unsigned long long)ktime_to_ms(kt);

	list_add_tail(&node->node, &list->head);

	list->size= list->size + 1;
	MESSAGE_LOG("%s post message, list size is %d.", list->name, list->size);
	spin_unlock_irqrestore(&list->lock, flags);
	MESSAGE_DBG("post_message: unlock list 0x%x,0x%x", list, (u64)node);
	wake_up_interruptible(&list->waiter);

	return true;
}

struct message_node *get_message(struct message_list *list, int timeout)
{
	struct message_node *msg;
	unsigned long flags;

	if (NULL == list) {
		return NULL;
	}
	if (list->size == 0) {
		if (0 == timeout) {
			wait_event_interruptible(list->waiter, list->size);
		} else {
			wait_event_interruptible_timeout(list->waiter,
							 list->size,
							 timeout);
		}
	}
	if (list->size <= 0) {
		MESSAGE_ERR("%s list size is error %d.", list->name, list->size);
		return NULL;
	}
	MESSAGE_LOG("%s get message, list size is %d.", list->name, list->size);
	MESSAGE_DBG("get_message:lock list 0x%x,node 0x%x", list, (u64)&msg);
	spin_lock_irqsave(&list->lock, flags);
	msg = list_first_entry(&list->head, struct message_node, node);
	list_del_init(&msg->node);
	list->size--;
	spin_unlock_irqrestore(&list->lock, flags);
	MESSAGE_DBG("get_message:unlock list 0x%x,node 0x%x", list, msg);

	return msg;
}

void delete_message_node(struct message_list *list, struct message_node **msg)
{
	unsigned long flags;
	if (NULL == list) {
		return;
	}

	if ((*msg) != NULL) {
		MESSAGE_DBG("delete_message_node:lock list 0x%x,node 0x%x", list, (u64)*msg);
		spin_lock_irqsave(&list->lock, flags);
		list->memory_bit |= (*msg)->used_bit;
		spin_unlock_irqrestore(&list->lock, flags);
		MESSAGE_DBG("delete_message_node:unlock list 0x%x,node 0x%x", list, (u64)*msg);
		kfree(*msg);
		*msg = NULL;
	}
}


struct message_list *init_message_list(int max_size,
				       u32 block_size,
				       const char *name)

{
	struct message_list *list = NULL;

	if (0 >= max_size) {
		MESSAGE_ERR("%s, message list init failed!", name);
		goto ERROR_EXIT;
	}
	if ((max_size > MAX_STATIC_SIZE || block_size > 12)) {
		MESSAGE_ERR("%s, static message list init failed!", name);
		goto ERROR_EXIT;
	}

	list = kzalloc(sizeof(struct message_list), GFP_KERNEL);
	if (NULL == list || IS_ERR(list)) {
		MESSAGE_ERR("%s, message list init failed!", name);
		goto ERROR_EXIT;
	}

	list->name = kzalloc(MAX_NAME_SIZE, GFP_KERNEL);
	if (NULL == list->name || IS_ERR(list->name)) {
		MESSAGE_ERR("%s, message list init failed!", name);
		goto ERROR_EXIT;
	}
	snprintf(list->name, MAX_NAME_SIZE, "%s", name);

	list->max_size = max_size;

	list->block_size = 1 << block_size;
	list->memory_buf = kzalloc(max_size * list->block_size,
				   GFP_KERNEL);
	list->memory_bit = (1L << max_size) - 1;
	MESSAGE_LOG("%s, memory_bit 0x%0X", name, list->memory_bit);

	spin_lock_init(&list->lock);
	init_waitqueue_head(&list->waiter);
	INIT_LIST_HEAD(&list->head);

	MESSAGE_LOG("%s, message list init ok!", list->name);

	return list;
ERROR_EXIT:
	if (list) {
		if (list->name) {
			kfree(list->name);
		}
		if (list->memory_buf) {
			kfree(list->memory_buf);
		}
		kfree(list);
	}
	return NULL;
}

bool clear_message_list(struct message_list **list)
{

	struct message_node *pnode;
	struct message_node *pn_node;
	unsigned long flags;

	if (NULL == (*list)) {
		return true;
	}

	(*list)->size = -1;
	wake_up_interruptible(&(*list)->waiter);

	MESSAGE_DBG("clear_message_list:lock list 0x%x", (u64)*list);
	spin_lock_irqsave(&(*list)->lock, flags);

	list_for_each_entry_safe(pnode, pn_node, &(*list)->head, node) {
		list_del_init(&pnode->node);
		kfree(pnode);
	}

	(*list)->size = 0;
	(*list)->memory_bit = (1L << (*list)->max_size) - 1;

	if (!list_empty(&(*list)->head)) {
		spin_unlock_irqrestore(&(*list)->lock, flags);
		MESSAGE_DBG("clear_message_list:unlock list 0x%x", (u64)*list);
		MESSAGE_ERR("%s,message list not empty after clear!", (*list)->name);
		return false;
	}
	MESSAGE_LOG("%s,message list clear!", (*list)->name);
	spin_unlock_irqrestore(&(*list)->lock, flags);
	MESSAGE_DBG("clear_message_list:unlock list 0x%x", (u64)*list);
	return true;
}

bool delete_message_list(struct message_list **list)
{
	struct message_node *pnode;
	struct message_node *pn_node;
	unsigned long flags;

	if (NULL == (*list)) {
		return true;
	}

	(*list)->size = -1;
	wake_up_interruptible(&(*list)->waiter);
	MESSAGE_DBG("clear_message_list:lock list 0x%x", (u64)*list);
	spin_lock_irqsave(&(*list)->lock, flags);

	list_for_each_entry_safe(pnode, pn_node, &(*list)->head, node) {
		list_del_init(&pnode->node);
		kfree(pnode);
	}

	if (!list_empty(&(*list)->head)) {
		spin_unlock_irqrestore(&(*list)->lock, flags);
		MESSAGE_DBG("clear_message_list:unlock list 0x%x", (u64)*list);
		MESSAGE_ERR("%s,message list not empty after delete!", (*list)->name);
		return false;
	}
	MESSAGE_LOG("%s,message list delete!", (*list)->name);
	if ((*list)->name) {
		kfree((*list)->name);
	}
	if ((*list)->memory_buf) {
		kfree((*list)->memory_buf);
	}
	spin_unlock_irqrestore(&(*list)->lock, flags);
	MESSAGE_DBG("clear_message_list:unlock list 0x%x", (u64)*list);
	kfree(*list);
	*list = NULL;
	return true;
}


