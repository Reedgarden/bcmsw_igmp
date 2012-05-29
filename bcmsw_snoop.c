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
#include <linux/igmp.h>

#include "bcmsw_snoop.h"
#include "bcmsw_mii.h"
#include "bcmsw_proc.h"

#define _DEBUG_

#ifdef _DEBUG_
#include "bcmsw_kwatch.h"
#endif

typedef struct
{
	__be32 group;
	unsigned short port_bitmap;
	struct list_head mc_list_node;
}  mc_node;

typedef struct
{
	__u8 type;
	__be32 group;
	__u16 port;
	struct list_head ip_list_node;
} ip_node;

typedef struct {
	struct task_struct *task;
	struct list_head ip_list;
	struct list_head mc_list;
	spinlock_t ip_lock;
	wait_queue_head_t ip_wait;
	int insertable;
	int mac_table_cnt;
	unsigned short g_portmap;
} bcmsw_snoop;

static bcmsw_snoop* get_snoop_instance(void);
static void free_snoop(bcmsw_snoop* s);

static ip_node* get_ip_node(bcmsw_snoop* s);
static mc_node* get_mc_node(ip_node* _ip_node);
static void set_mc_portmap(mc_node* _mc, ip_node* _ip);
static void update_g_portmap(int type, unsigned short portmap);

static int show_mac_table(char* page, char** atart, off_t off, int count, int *eof, void *data);
static void mac_table_update_direct(ip_node* _ip_node);

const bcmsw_proc_t proc_snoop = {
		0444, 	//mode
		"switch_mac_table",
		show_mac_table
};

#ifdef _DEBUG_
kwatch* w_get_set_ip;
kwatch* w_mac_tableu;
#endif

int thread_function(void *data)
{
	bcmsw_snoop* snoop = (bcmsw_snoop*)data;
	ip_node* igmp_node;

	while(1)
	{	// get ip node
		igmp_node = get_ip_node(snoop);

#ifdef _DEBUG_
		kwatch_lap_sec(w_get_set_ip);
#endif

		if(!snoop->insertable)
			break;

		mac_table_update_direct(igmp_node);

		kfree(igmp_node);		// i don't need anymore ..

		if(kthread_should_stop())
			break;
	}
	return 0;
}

void node_init(void)
{
	bcmsw_snoop* snoop = get_snoop_instance();
	struct sched_param sp = { .sched_priority = 10 };
	int ret;

	// create kthread
	snoop->task = kthread_create(&thread_function, (void*)snoop, "nodes_thread");

	// set priority
	ret = sched_setscheduler(snoop->task, SCHED_RR , &sp);
	if(ret == -1) {
		printk("ERR!!\n");
		return ;
	}

	wake_up_process(snoop->task);

	// register proc mac_table_read
	proc_register(&proc_snoop);
}

void node_uninit(void)
{
	bcmsw_snoop* snoop = get_snoop_instance();
	free_snoop(snoop);
	kthread_stop(snoop->task);
	kfree(snoop);
}

static bcmsw_snoop* get_snoop_instance(void)
{
	static bcmsw_snoop* snoop;
	if(snoop == NULL)
	{
		snoop = (bcmsw_snoop*)kmalloc(sizeof(bcmsw_snoop), GFP_KERNEL);
		// spin init
		spin_lock_init(&snoop->ip_lock);
		// list init
		INIT_LIST_HEAD(&snoop->ip_list);
		INIT_LIST_HEAD(&snoop->mc_list);
		// cond init
		init_waitqueue_head(&snoop->ip_wait);
		snoop->insertable = 1;
		snoop->mac_table_cnt=0;
		snoop->g_portmap = 0x0102;		// default port map
	}
	return snoop;
}

static void free_snoop(bcmsw_snoop* s)
{
	struct list_head *ptr, *node;
	ip_node* tmp;

	s->insertable = 0;
	node = &s->ip_list;

	wake_up_interruptible(&s->ip_wait);
	/*
	 * free ip_node
	 */
	spin_lock(&s->ip_lock);
	for(ptr = node->next; ptr != node; ) {
		tmp = list_entry(ptr, ip_node, ip_list_node);
		ptr = ptr->next;
		list_del(&tmp->ip_list_node);
		kfree(tmp);
	}
	spin_unlock(&s->ip_lock);
}

static ip_node* get_ip_node(bcmsw_snoop* s)
{
	struct list_head *ptr = &s->ip_list;
	ip_node* node;

	// spin lock
	spin_lock(&s->ip_lock);
	for(;;)
	{
		if( !s->insertable )
			return NULL;

		if( ptr->next != &s->ip_list )
			break;

		//unlock spin & wait
		spin_unlock(&s->ip_lock);
		interruptible_sleep_on(&s->ip_wait);
	}
	// go get it ..
	node = list_entry(ptr->next, ip_node, ip_list_node);
	list_del(ptr->next);
	spin_unlock(&s->ip_lock);
	return node;
}

static mc_node* get_mc_node(ip_node* _ip_node)
{
	bcmsw_snoop* snoop = get_snoop_instance();
	struct list_head *ptr, *node;
	mc_node* tmp;

	node = &snoop->mc_list;

	// search from list
	for(ptr = node->next; ptr != node; ptr = ptr->next) {
		tmp = list_entry(ptr, mc_node, mc_list_node);
		if(tmp->group == _ip_node->group)
			return tmp;
	}

	// not exist, kmalloc
	tmp = (mc_node* )kmalloc(sizeof(mc_node), GFP_KERNEL);
	tmp->group = _ip_node->group;
	tmp->port_bitmap = 0;
	list_add_tail(&tmp->mc_list_node, &snoop->mc_list);

	return tmp;
}

static void set_mc_portmap(mc_node* _mc, ip_node* _ip)
{
#if 0
	switch(node->type)
	{
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
	case IGMPV3_HOST_MEMBERSHIP_REPORT:
		if( !(node->port_bitmap & new->port_bitmap) ) {
			node->port_bitmap |= new->port_bitmap;
			node->sync_s=MC_NODE_STATUS_CHANGED;	// node status has changed !
			oos++;
		}
		break;

	case IGMP_HOST_LEAVE_MESSAGE:
		if( node->port_bitmap & new->port_bitmap ) {
			node->port_bitmap &= ~new->port_bitmap;
			node->sync_s=MC_NODE_STATUS_CHANGED;	// node status has changed !
			oos++;
		}
		break;

	default:
		break;
	}
#endif
}

static void update_g_portmap(int type, unsigned short portmap)
{
	bcmsw_snoop* snoop =  get_snoop_instance();

	switch(type)
	{
		case IGMP_HOST_MEMBERSHIP_REPORT:
		case IGMPV2_HOST_MEMBERSHIP_REPORT:
			snoop->g_portmap |= (1<<portmap);
			break;
		case IGMP_HOST_LEAVE_MESSAGE:
			snoop->g_portmap &= ~(1<<portmap);
			break;
		default:
			// none
			break;
	}
	printk("@@@ %s @@@ type 0x%x [0x%x] \n",__func__,type, snoop->g_portmap);
	net_dev_set_dfl_map(snoop->g_portmap);	// mii write!
}

int set_ip_node(__u8 type, __be32 group, __u16 port )
{
#ifdef _DEBUG_
	w_get_set_ip = kwatch_start("w_get_set_ip");
#endif

	bcmsw_snoop* snoop = get_snoop_instance();
	ip_node* node;

	if(!snoop->insertable)	// closing..
		return -ENOMEM;

	// kmalloc
	node = (ip_node* )kmalloc(sizeof(ip_node), GFP_KERNEL);
	if(node == NULL)
	{
		printk(KERN_ERR "out-of-memory\n");
		return -ENOMEM;
	}

	node->type = type;
	node->group = group;
	node->port = port;

	// add
	spin_lock(&snoop->ip_lock);
	list_add_tail(&node->ip_list_node, &snoop->ip_list);
	wake_up_interruptible(&snoop->ip_wait);			// wake up
	spin_unlock(&snoop->ip_lock);

	return 0;
}

static int show_mac_table(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len, i;
#if 0
	struct bcmsw_snoop* s = get_snoop_instance();
	struct list_head *ptr, *mac_head;
	struct mac_node* tmp;

	mac_head = &s->macm_list;

	len= sprintf(page    , " mac \t\t\t\t portmap \t type    \n");
	len+=sprintf(page+len, "======================================================\n");
	for(ptr = mac_head->next; ptr != mac_head; ptr = ptr->next) {
		tmp = list_entry(ptr, struct mac_node, mc_list_node);

		for(i=ETH_ALEN-1; i>=0; i--) {
			len += sprintf( page+len , "0x%02x", tmp->eth_addr[i]);
			if(i!=0)
				len += sprintf( page+len , ":");
			else
				len += sprintf( page+len , "\t");
		}

		len += sprintf( page+len , " 0x%x\t\t", tmp->port_bitmap);
		len += sprintf( page+len , " 0x%x\n", tmp->type);
	}

	*eof = 1;
#endif
	return len;
}

static void mac_table_update_direct(ip_node* _ip_node)
{
#ifdef _DEBUG_
	w_mac_tableu = kwatch_start("w_mac_tableu");
#endif

//	mc_node* _mc_node;
	update_g_portmap(_ip_node->type, _ip_node->port);

	// get mac_node
//	_mc_node = get_mc_node(_ip_node);
//	set_mc_portmap(_mc_node, _ip_node);

#ifdef _DEBUG_
	kwatch_lap_sec(w_mac_tableu);
#endif
}

