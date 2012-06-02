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
#include <linux/workqueue.h>
#include <linux/jiffies.h>

#include "bcmsw_snoop.h"
#include "bcmsw_mii.h"
#include "bcmsw_proc.h"

//#define _DEBUG_

#ifdef _DEBUG_
#include "bcmsw_kwatch.h"
#endif

#define DEFUALT_G_PORTMAP 	(0x0102)
#define HW_PORT_WAN			(1)
#define HW_PORT_LAN			(2)
#define HW_PORT_IMP			(8)

typedef struct
{
	__be32 group;
	unsigned short port_bitmap;
	struct delayed_work work;
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
	/* main thread's priority */
	struct task_struct *task;

	/* set/get ip_node */
	struct list_head ip_list;
	spinlock_t ip_lock;
	wait_queue_head_t ip_wait;
	int insertable;

	/* mc_node list manager */
	struct list_head mc_list;
	spinlock_t mc_lock;
	struct workqueue_struct* wq;

	/* LAN connect check */
	struct timer_list phy_timer;
	struct work_struct phy_work;

	/* global port map */
	spinlock_t portmap_lock;
	unsigned short g_portmap;

} bcmsw_snoop;

/* IMPORT EXPORTED SYMBOL */

/* STATIC FUNCTION */
static bcmsw_snoop* get_snoop_instance(void);
static void free_snoop(bcmsw_snoop* s);
static ip_node* get_ip_node(bcmsw_snoop* s);
static int set_mc_node(ip_node* _ip_node);
static void mc_node_expiried(struct work_struct* work);
static void update_g_portmap(int type, unsigned short portmap);
static int show_mac_table(char* page, char** atart, off_t off, int count, int *eof, void *data);
static void mac_table_update_direct(ip_node* _ip_node);
static void bcmsw_gphy_link_timer(unsigned long data);
static void bcmsw_gphy_link_status(struct work_struct *work);

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
	{
		// get ip node
		igmp_node = get_ip_node(snoop);
#ifdef _DEBUG_
		kwatch_lap_sec(w_get_set_ip);
#endif
		if(!snoop->insertable)
			break;

		mac_table_update_direct(igmp_node);
		// create mac_node
		set_mc_node(igmp_node);

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
		spin_lock_init(&snoop->portmap_lock);
		spin_lock_init(&snoop->mc_lock);
		// list init
		INIT_LIST_HEAD(&snoop->ip_list);
		INIT_LIST_HEAD(&snoop->mc_list);
		// cond init
		init_waitqueue_head(&snoop->ip_wait);
		snoop->insertable = 1;
		snoop->g_portmap = DEFUALT_G_PORTMAP;		// default port map (IMP , WAN port)

		// create timer
		init_timer(&snoop->phy_timer);
		snoop->phy_timer.data = (unsigned long)snoop;
		snoop->phy_timer.function = bcmsw_gphy_link_timer;
		mod_timer(&snoop->phy_timer, jiffies);

		INIT_WORK(&snoop->phy_work, bcmsw_gphy_link_status);

		/*snoop->wq = create_workqueue("mc_list_queue");*/
		snoop->wq = create_singlethread_workqueue("mc_list_queue");
		if(snoop->wq == NULL)
			return NULL;
	}
	return snoop;
}

static void free_snoop(bcmsw_snoop* s)
{
	struct list_head *ptr, *node;
	ip_node* tmp;
	int ret;

	s->insertable = 0;
	node = &s->ip_list;

	// dispose of ..
	wake_up_interruptible(&s->ip_wait);

	// cancel timer
	ret = del_timer(&s->phy_timer);
	if(ret) printk("timer cannot delete .. \n");
	cancel_work_sync(&s->phy_work);

	// destroy work queue
	flush_workqueue(s->wq);
	destroy_workqueue(s->wq);

	// free ip_node
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

#define MC_DELAY_MSEC (135000)
static int set_mc_node(ip_node* _ip_node)
{
	bcmsw_snoop* snoop = get_snoop_instance();
	struct list_head *ptr, *node;
	mc_node* tmp;

	node = &snoop->mc_list;
	// search from list
	for(ptr = node->next; ptr != node; ptr = ptr->next) {
		tmp = list_entry(ptr, mc_node, mc_list_node);
		if(tmp->group == _ip_node->group) {
			switch (_ip_node->type) {
				case IGMP_HOST_MEMBERSHIP_REPORT:
				case IGMPV2_HOST_MEMBERSHIP_REPORT:
					tmp->port_bitmap |= (1<<_ip_node->port);
					break;
				case IGMP_HOST_LEAVE_MESSAGE:
					tmp->port_bitmap &= ~(1<<_ip_node->port);
					break;
			}
			// renewal
			cancel_delayed_work(&tmp->work);
			queue_delayed_work(snoop->wq,&tmp->work, msecs_to_jiffies(MC_DELAY_MSEC));
			return 0;
		}
	}

	// not exist, kmalloc
	tmp = (mc_node* )kmalloc(sizeof(mc_node), GFP_KERNEL);
	tmp->group = _ip_node->group;
	tmp->port_bitmap = DEFUALT_G_PORTMAP;
	switch (_ip_node->type) {
		case IGMP_HOST_MEMBERSHIP_REPORT:
		case IGMPV2_HOST_MEMBERSHIP_REPORT:
			tmp->port_bitmap |= (1<<_ip_node->port);
			break;
		case IGMP_HOST_LEAVE_MESSAGE:
			tmp->port_bitmap &= ~(1<<_ip_node->port);
			break;
	}
	INIT_DELAYED_WORK(&tmp->work, mc_node_expiried);
	queue_delayed_work(snoop->wq,&tmp->work, msecs_to_jiffies(MC_DELAY_MSEC));

	spin_lock(&snoop->mc_lock);
	list_add_tail(&tmp->mc_list_node, &snoop->mc_list);
	spin_unlock(&snoop->mc_lock);

	return 0;
}

static void update_g_portmap(int type, unsigned short portmap)
{
	bcmsw_snoop* snoop =  get_snoop_instance();

	switch(type)
	{
		case IGMP_HOST_MEMBERSHIP_REPORT:
		case IGMPV2_HOST_MEMBERSHIP_REPORT:
			spin_lock(&snoop->portmap_lock);
			snoop->g_portmap |= (1<<portmap);
			spin_unlock(&snoop->portmap_lock);
			break;
		case IGMP_HOST_LEAVE_MESSAGE:
			spin_lock(&snoop->portmap_lock);
			snoop->g_portmap &= ~(1<<portmap);
			spin_unlock(&snoop->portmap_lock);
			break;
		default:
			// none
			break;
	}
	net_dev_set_dfl_map(snoop->g_portmap);	// mii write!
}

int set_ip_node(__u8 type, __be32 group, __u16 port )
{
	bcmsw_snoop* snoop = get_snoop_instance();
	ip_node* node;

#ifdef _DEBUG_
	w_get_set_ip = kwatch_start("w_get_set_ip");
#endif

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
	bcmsw_snoop* snoop = get_snoop_instance();
	mc_node* tmp;
	struct list_head *ptr, *mac_head;
	int len;

	mac_head = &snoop->mc_list;

	len= sprintf(page    , " Group IP \t\t MCST addr \t\t Port Map \n");
	len+=sprintf(page+len, "---------------------------------------------------------\n");

	spin_lock(&snoop->mc_lock);
	for(ptr = mac_head->next; ptr != mac_head; ptr = ptr->next) {
		tmp = list_entry(ptr, mc_node, mc_list_node);
		// group ip
		len += sprintf( page+len, "%3d.", 	(tmp->group) & 0x000000ff );
		len += sprintf( page+len, "%3d.", 	(tmp->group>>8) & 0x000000ff );
		len += sprintf( page+len, "%3d.", 	(tmp->group>>16) & 0x000000ff );
		len += sprintf( page+len, "%3d \t", 	(tmp->group>>24) & 0x000000ff );

		// MCST addr
		len += sprintf( page+len, "01:00:5e:");
		len += sprintf( page+len, "%02x:", (tmp->group>>8) & 0x0000007f );
		len += sprintf( page+len, "%02x:", (tmp->group>>16) & 0x000000ff );
		len += sprintf( page+len, "%02x \t", (tmp->group>>24) & 0x000000ff );

		len += sprintf( page+len , " 0x%x", tmp->port_bitmap);
		len += sprintf( page+len , "\n");
	}

	spin_unlock(&snoop->mc_lock);

	*eof = 1;
	return len;
}

/* --------------------------------------------------------------------------
Name: mac_table_update_direct
-------------------------------------------------------------------------- */
static void mac_table_update_direct(ip_node* _ip_node)
{
#ifdef _DEBUG_
	w_mac_tableu = kwatch_start("w_mac_tableu");
#endif

	update_g_portmap(_ip_node->type, _ip_node->port);

#ifdef _DEBUG_
	kwatch_lap_sec(w_mac_tableu);
#endif
}


/* --------------------------------------------------------------------------
Name: bcmsw_gphy_link_timer
Purpose: LAN link status monitoring timer function
-------------------------------------------------------------------------- */
static void bcmsw_gphy_link_timer(unsigned long data)
{
	bcmsw_snoop * dev = (bcmsw_snoop *)data;
	schedule_work(&dev->phy_work);
	mod_timer(&dev->phy_timer, jiffies + HZ);		// every 1 second
}

/* --------------------------------------------------------------------------
Name: bcmsw_gphy_link_status
Purpose: LAN link status monitoring task
-------------------------------------------------------------------------- */
static void bcmsw_gphy_link_status(struct work_struct *work)
{
	bcmsw_snoop *snoop = container_of(work, bcmsw_snoop, phy_work);
	int link;
	link = net_dev_phy_link_ok(HW_PORT_LAN);		// 1:WAN PORT, 2:LAN PORT, 8:IMP PORT

	// turn off portmap!
	if(!link) {
		spin_lock(&snoop->portmap_lock);
		snoop->g_portmap &= DEFUALT_G_PORTMAP;
		spin_unlock(&snoop->portmap_lock);
	}

}

/* --------------------------------------------------------------------------
Name: mc_node_expiried
Purpose: occurr this function when mac node expired (default:135 sec)
-------------------------------------------------------------------------- */
static void mc_node_expiried(struct work_struct* w)
{
	bcmsw_snoop * snoop = get_snoop_instance();
	mc_node* mc = container_of(w, mc_node, work);

	struct list_head *ptr, *node;
	mc_node* tmp;
	node = &snoop->mc_list;

	// free ip_node
	spin_lock(&snoop->mc_lock);
	for(ptr = node->next; ptr != node; ) {
		tmp = list_entry(ptr, mc_node, mc_list_node);
		ptr = ptr->next;

		if(tmp == mc) {
			list_del(&tmp->mc_list_node);
			kfree(tmp);
			break;
		}
	}
	spin_unlock(&snoop->mc_lock);
}
