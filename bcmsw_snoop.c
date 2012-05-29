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

#define MC_TABLE_STATUS_NONE 	0
#define MC_TABLE_STATUS_CHANGED 1
#define MC_NODE_STATUS_DONE 	0
#define MC_NODE_STATUS_CHANGED  1

//#define _DEBUG_

#ifdef _DEBUG_
#include "bcmsw_kwatch.h"
kwatch* w_setget_ip_node;
kwatch* w_mac_table_update_direct;
#endif

#define ETH_ALEN 6
struct mac_node
{
	unsigned char eth_addr[ETH_ALEN];
	unsigned short port_bitmap;
	int	type;
	int sync_s;
	struct list_head mc_list_node;
};
struct ip_node
{
	__u8 type;
	__be32 group;
	__u16 port;
	struct list_head ip_list_node;
};

struct bcmsw_snoop {
	struct task_struct *task;
	struct list_head ipm_list;
	struct list_head macm_list;
	spinlock_t ip_lock;
	spinlock_t mac_lock;
	wait_queue_head_t ip_wait;
	int insertable;
	int mac_table_cnt;
};

static struct bcmsw_snoop* get_snoop_instance(void);
static void free_snoop(struct bcmsw_snoop* s);
static struct ip_node* get_ip_node(struct bcmsw_snoop* s);
static struct mac_node* get_mac_from_ip_node(struct ip_node* n);
static int update_mac_table(struct bcmsw_snoop* s, struct mac_node* mac_node);
static int set_mac_node(struct mac_node* node, struct mac_node* new);
static void mac_table_updated(struct bcmsw_snoop* s);
static int show_mac_table(char* page, char** atart, off_t off, int count, int *eof, void *data);
static void mac_table_update_direct(struct ip_node* _node);

const bcmsw_proc_t proc_snoop = {
		0444, 	//mode
		"switch_mac_table",
		show_mac_table
};

int thread_function(void *data)
{
	struct bcmsw_snoop* snoop = (struct bcmsw_snoop*)data;
	struct ip_node* igmp_node;
	struct mac_node* mac_node;
	int oos;	//	out-of-sync


	while(1)
	{	// get ip node
		igmp_node = get_ip_node(snoop);
		if(!snoop->insertable)
			break;

		/*printk("%s %d ... t 0x%x g 0x%x %d \n", __func__, __LINE__, igmp_node->type, igmp_node->group, igmp_node->port);*/
		//if( already_exist(&snoop->macm_list, mac_node) )
		// kfree(igmp_node);
		// continue;

#ifdef _DEBUG_
	kwatch_lap_sec(w_setget_ip_node);
#endif

#if 1
		mac_table_update_direct(igmp_node);
#else
		mac_node = get_mac_from_ip_node(igmp_node);
		oos = update_mac_table(snoop, mac_node);
		if( oos != MC_TABLE_STATUS_NONE )
			mac_table_updated(snoop);
#endif

		kfree(igmp_node);		// i don't need anymore ..
//		kfree(mac_node);
		if(kthread_should_stop())
			break;
	}
	return 0;
}

void node_init(void)
{
	struct bcmsw_snoop* snoop = get_snoop_instance();
	struct sched_param sp = { .sched_priority = 10 };
	int ret;

	// create kthread
	snoop->task = kthread_create(&thread_function, (void*)snoop, "nodes_thread");

	// set priority
	ret = sched_setscheduler(snoop->task->pid, SCHED_BATCH , &sp);
	if(ret == -1) {
		printk("ERR!!\n");
		return 1;
	}


	wake_up_process(snoop->task);



	// register proc mac_table_read
	proc_register(&proc_snoop);
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
		// spin init
		spin_lock_init(&snoop->ip_lock);
		spin_lock_init(&snoop->mac_lock);
		// list init
		INIT_LIST_HEAD(&snoop->ipm_list);
		INIT_LIST_HEAD(&snoop->macm_list);
		// cond init
		init_waitqueue_head(&snoop->ip_wait);
		snoop->insertable = 1;
		snoop->mac_table_cnt=0;
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
		tmp = list_entry(ptr, struct ip_node, ip_list_node);
		ptr = ptr->next;
		list_del(&tmp->ip_list_node);
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

		//unlock spin & wait
		spin_unlock(&s->ip_lock);
		interruptible_sleep_on(&s->ip_wait);
	}
	// go get it ..
	node = list_entry(ptr->next, struct ip_node, ip_list_node);
	list_del(ptr->next);
	spin_unlock(&s->ip_lock);
	return node;
}

int set_ip_node(__u8 type, __be32 group, __u16 port )
{
#ifdef _DEBUG_
	w_setget_ip_node = kwatch_start("set/get_ip_node");
#endif

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

	// add
	spin_lock(&snoop->ip_lock);
	list_add_tail(&node->ip_list_node, &snoop->ipm_list);
	wake_up_interruptible(&snoop->ip_wait);			// wake up
	spin_unlock(&snoop->ip_lock);

	return 0;
}

static struct mac_node* get_mac_from_ip_node(struct ip_node* n)
{
	struct mac_node* m_node = (struct mac_node*)kmalloc(sizeof(struct mac_node), GFP_KERNEL);
	m_node->type = n->type;

	m_node->port_bitmap = (1<<n->port);	// be careful!
	m_node->eth_addr[5] = 0x01;
	m_node->eth_addr[4] = 0x00;
	m_node->eth_addr[3] = 0x5E;
	m_node->eth_addr[2] = (n->group>>8) & 0x000000ff;
	m_node->eth_addr[1] = (n->group>>16) & 0x000000ff;;
	m_node->eth_addr[0] = (n->group>>24) & 0x000000ff;
	m_node->sync_s=MC_NODE_STATUS_CHANGED;		// table has changed!

	return m_node;
}

static int update_mac_table(struct bcmsw_snoop* s, struct mac_node* mac_node)
{
	struct list_head* mac_head = &s->macm_list;
	struct list_head* ptr;
	struct mac_node* entry;

	if(mac_head == mac_head->next)	// if nothing in list ..
	{
		spin_lock(&s->mac_lock);
		list_add_tail(&mac_node->mc_list_node, &s->macm_list);
		s->mac_table_cnt++;
		spin_unlock(&s->mac_lock);
		return MC_TABLE_STATUS_CHANGED;				// table has changed!
	}

	// for loop should be changed!!
	for(ptr = mac_head->next; ptr != mac_head; ptr = ptr->next) {
		entry = list_entry(ptr, struct mac_node, mc_list_node);
		if(memcmp(entry->eth_addr, mac_node->eth_addr, ETH_ALEN) == 0) {
			return set_mac_node(entry, mac_node);
		}
		/*
		 * else
		 * if( entry->eth_addr == 0 )
		 * delete!!
		 */
	}

	// if not exist..
	spin_lock(&s->mac_lock);
	list_add_tail(&mac_node->mc_list_node, &s->macm_list);
	s->mac_table_cnt++;
	spin_unlock(&s->mac_lock);
	return MC_TABLE_STATUS_CHANGED;		// table has changed!
}

static int set_mac_node(struct mac_node* node, struct mac_node* new)
{
	int oos=0;

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
	return oos;
}

static void mac_table_updated(struct bcmsw_snoop* s)
{
	struct list_head *ptr, *mac_head;
	struct mac_node* tmp;

	mac_head = &s->macm_list;

	for(ptr = mac_head->next; ptr != mac_head; ptr = ptr->next) {
		tmp = list_entry(ptr, struct mac_node, mc_list_node);

		if(tmp->sync_s == MC_NODE_STATUS_CHANGED)
		{
			//write!!
			/*printk("*%s* %d .. 0x%x\n", __func__, __LINE__, tmp->port_bitmap);*/
			net_dev_mii_write(tmp->eth_addr, tmp->port_bitmap);
			tmp->sync_s = MC_NODE_STATUS_DONE;
			s->mac_table_cnt--;
		}
	}

}

static int show_mac_table(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len, i;
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
	return len;
}

static void mac_table_update_direct(struct ip_node* _node)
{
#ifdef _DEBUG_
	w_mac_table_update_direct = kwatch_start("mac_table_update");
#endif
	unsigned short port_map = (1<<_node->port);
	unsigned char  eth_addr[6];

	eth_addr[5] = 0x01;
	eth_addr[4] = 0x00;
	eth_addr[3] = 0x5E;
	eth_addr[2] = (_node->group>>8) & 0x000000ff;
	eth_addr[1] = (_node->group>>16) & 0x000000ff;;
	eth_addr[0] = (_node->group>>24) & 0x000000ff;

	net_dev_mii_write(eth_addr, port_map);

#ifdef _DEBUG_
	kwatch_lap_sec(w_mac_table_update_direct);
#endif
}

