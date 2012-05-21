/*
 * bcmsw_snoop.c
 *
 *  Created on: May 21, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include "bcmsw_snoop.h"

#define ETH_ALEN 6
struct mac_node
{
	unsigned char eth_addr[ETH_ALEN];
	unsigned short port_bitmap;
	int sync_s;
};
struct ip_node
{
	__u8 type;
	__be32 group;
	__u16 port;
	struct list_head list_node;
};

struct bcmsw_snoop {
	struct task_struct *task;
	struct list_head ipm_list;
	struct list_head macm_list;
	spinlock_t ip_lock;
	spinlock_t mac_lock;
	wait_queue_head_t ip_wait;
	int insertable;
};

/**
 * delete ip nodes list
 * init list
 * kfree all nodes
 */
static struct bcmsw_snoop* get_snoop_instance(void);
static void free_snoop(struct bcmsw_snoop* s);
static struct ip_node* get_ip_node(struct bcmsw_snoop* s);

int thread_function(void *data)
{
	struct bcmsw_snoop* snoop = (struct bcmsw_snoop*)data;
	struct ip_node* node;

	while(1)
	{
		// get ip node
		node = get_ip_node(snoop);
		if(!snoop->insertable)
			break;

		printk("%s %d ... t 0x%x g 0x%x %d \n", __func__, __LINE__, node->type, node->group, node->port);

		kfree(node);		// i don't need anymore ..
		if(kthread_should_stop())
			break;
	}
	return 0;
}

void node_init(void)
{
	struct bcmsw_snoop* snoop = get_snoop_instance();
	// create kthread
	snoop->task = kthread_create(&thread_function, (void*)snoop, "nodes_thread");
	wake_up_process(snoop->task);
}

void node_uninit(void)
{
	struct bcmsw_snoop* snoop = get_snoop_instance();
	free_snoop(snoop);
	kthread_stop(snoop->task);
	kfree(snoop);
}

static struct bcmsw_snoop* get_snoop_instance(void)
{
	static struct bcmsw_snoop* snoop;
	if(snoop == NULL)
	{
		snoop = (struct bcmsw_snoop*)kmalloc(sizeof(struct bcmsw_snoop), GFP_KERNEL);
		spin_lock_init(&snoop->ip_lock);
		spin_lock_init(&snoop->mac_lock);
		INIT_LIST_HEAD(&snoop->ipm_list);
		INIT_LIST_HEAD(&snoop->macm_list);
		init_waitqueue_head(&snoop->ip_wait);
		snoop->insertable = 1;
	}
	return snoop;
}

static void free_snoop(struct bcmsw_snoop* s)
{
	struct list_head *ptr, *node;
	struct ip_node* tmp;

	s->insertable = 0;
	node = &s->ipm_list;

	wake_up_interruptible(&s->ip_wait);
	/*
	 * free ip_node
	 */
	spin_lock(&s->ip_lock);
	for(ptr = node->next; ptr != node; ) {
		tmp = list_entry(ptr, struct ip_node, list_node);
		ptr = ptr->next;
		list_del(&tmp->list_node);
		kfree(tmp);
	}
	spin_unlock(&s->ip_lock);
}

static struct ip_node* get_ip_node(struct bcmsw_snoop* s)
{
	struct list_head *ptr = &s->ipm_list;
	struct ip_node* node;

	// spin lock
	spin_lock(&s->ip_lock);
	for(;;)
	{
		if( !s->insertable )
			return NULL;

		if( ptr->next != &s->ipm_list )
			break;

		printk("+%s+ %d .. waiting ... \n", __func__, __LINE__);
		//unlock spin & wait
		spin_unlock(&s->ip_lock);
		/*wait_event(s->ip_wait, 0);*/
		interruptible_sleep_on(&s->ip_wait);
		printk("+%s+ %d .. wait event ... \n", __func__, __LINE__);
	}
	// go get it ..
	node = list_entry(ptr->next, struct ip_node, list_node);
	list_del(ptr->next);
	spin_unlock(&s->ip_lock);
	return node;
}

int set_ip_node(__u8 type, __be32 group, __u16 port )
{
	struct bcmsw_snoop* snoop = get_snoop_instance();
	struct ip_node* node;

	if(!snoop->insertable)	// closing..
		return -ENOMEM;

	// kmalloc
	node = (struct ip_node* )kmalloc(sizeof(struct ip_node), GFP_KERNEL);
	if(node == NULL)
	{
		printk(KERN_ERR "out-of-memory\n");
		return -ENOMEM;
	}

	node->type = type;
	node->group = group;
	node->port = port;

	/*printk("%s %d ... t 0x%x g 0x%x %d \n", __func__, __LINE__, node->type, node->group, node->port);*/

	// add
	spin_lock(&snoop->ip_lock);
	list_add_tail(&node->list_node, &snoop->ipm_list);
	wake_up_interruptible(&snoop->ip_wait);			// wake up
	spin_unlock(&snoop->ip_lock);

	return 0;
}

