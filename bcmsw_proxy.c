/*
 * bcmsw_proxy.c
 *
 *  Created on: May 17, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/igmp.h>
#include <linux/inetdevice.h>
#include <net/protocol.h>

#include <linux/delay.h>
#include <linux/syscalls.h>

#include "bcmsw_proxy.h"

/*
 * export_symbol from bcmgenet.c
 */
struct net_device* net_root_dev;
struct net_device *bcmemac_get_device(void);
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, int port, int pull);
struct net_protocol* get_inet_protos(void);

extern int igmp_rcv(struct sk_buff *);

/*
 * STATIC FUNCTIONS
 */
static void init_net_device(void);
static void net_dev_test_mii_rw(void);
static void protos_hook_igmp(void);
static int imp_igmp_rcv(struct sk_buff *skb);

/* sys_open() system wrapping */
void **sys_call_table;
asmlinkage int (*org_sys_open)( const char*, int, int );
asmlinkage int (*org_sys_getuid)( void );

int (*original_handler)(struct sk_buff *skb);

asmlinkage int sys_our_open( char *fname, int flags, int mode )
{
	printk( KERN_ALERT "%s file is opened by %d\n", fname, org_sys_getuid() );
	return ( org_sys_open(fname, flags, mode) );
}

static void **find_system_call_table(void)
{
#if 0
	unsigned long ptr;
//	extern int loops_per_jiffy;
	unsigned long *p;

	for( ptr = (unsigned long)&loops_per_jiffy;
			ptr < (unsigned long)&boot_cpu_data; ptr += sizeof(void*) )
	{
		p = (unsigned long*)ptr;
		if( p[6] == (unsigned long) sys_close )
			return (void**)p;
	}
#endif
	return NULL;
}

int sct_init(void)
{
#if 0
	sys_call_table = find_system_call_table();
	if(sys_call_table != NULL)
	{
		printk(KERN_ALERT "I think sys_call_table is at 0x%p \n", sys_call_table );
		org_sys_open = sys_call_table[__NR_open];
		sys_call_table[__NR_open] = sys_our_open;

		org_sys_getuid = sys_call_table[__NR_getuid];
	}
	{
		printk(KERN_ALERT "Failed to find sys_call_table\n");
	}
#endif
	return 0;
}

void sct_exit(void)
{
//	sys_call_table[__NR_open] = org_sys_open;
}

/*
 * struct
 */
struct net_protocol* original_protocol;
static const struct net_protocol imp_igmp_protocol = {
	.handler =	imp_igmp_rcv,
	.netns_ok =	1,
};

static int init_bcmsw_module(void)
{
	printk("init_bcmsw_module\n");
	init_net_device();
	net_dev_test_mii_rw();

	protos_hook_igmp();
	/*sct_init();*/
	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
	sct_exit();
}

static void init_net_device(void)
{
	net_root_dev = bcmemac_get_device();
	if(net_root_dev == NULL){
		printk( KERN_ALERT "cannot get device of bcmemac \n");
		return;
	}
}

static void net_dev_test_mii_rw(void)
{
	unsigned char mac[6] = {0x01, 0x02, 0x03, 0x5e, 0x00, 0x01};
	unsigned char buff[8];
	unsigned int  data;
	int i,j;

	bcmsw_reg_set_igmp_entry(net_root_dev, mac, 8, IGMP_JOIN);		// 0 - test port
	bcmsw_reg_get_igmp_entry(net_root_dev, mac, buff, &data);

	for(i=0,j=0; i<6; i++) {
//		printk("0x%02x 0x%02x \n", buff[i], mac[i]);
		if(buff[i] != mac[i])
			j++;
	}

	printk(KERN_ALERT "[%s] %s \n", __func__, (j==0)?"SUCCESS":"FAILED" );
}

static void protos_hook_igmp(void)
{
	/*original_protocol = get_inet_protos();*/
	int hash = IPPROTO_IGMP & (MAX_INET_PROTOS -1);
	original_protocol = inet_protos[ hash ];

	original_handler = original_protocol->handler;
	original_protocol->handler = imp_igmp_rcv;
	/*inet_protos[ hash ] = &imp_igmp_protocol;*/
	printk("igmp rcv function pointer 0x%p\n", original_handler);
	printk("igmp imp_rcv function pointer 0x%p\n", imp_igmp_rcv);
}

/*
 * imp igmp hooking function
 */
static int imp_igmp_rcv(struct sk_buff *skb)
{
	struct igmphdr *ih;
	struct sk_buff *inner_skb = skb_copy(skb, NULL);		// copy buff to inner buff
	struct in_device *in_dev = __in_dev_get_rcu(inner_skb->dev);
	int len = inner_skb->len;

	// toss to original protocol
//	printk("%s %d ..\n",__func__,__LINE__);
//	original_protocol->handler(skb);
//	printk("%s %d ..\n",__func__,__LINE__);
//	igmp_rcv(skb);
	original_handler(skb);

	if (in_dev == NULL)
		goto drop;

	if (!pskb_may_pull(inner_skb, sizeof(struct igmphdr)))
		goto drop;

	switch (inner_skb->ip_summed) {
	case CHECKSUM_COMPLETE:
		if (!csum_fold(inner_skb->csum))
			break;
		/* fall through */
	case CHECKSUM_NONE:
		inner_skb->csum = 0;
		if (__skb_checksum_complete(inner_skb))
			goto drop;
	}

	ih = igmp_hdr(inner_skb);
	printk("|%s| (%d) type 0x%0x group 0x%d \n",__func__, __LINE__, ih->type , ih->group );


	kfree_skb(inner_skb);
	return 0;

#if 0
	switch (ih->type) {
	case IGMP_HOST_MEMBERSHIP_QUERY:
		igmp_heard_query(in_dev, skb, len);
		break;
	case IGMP_HOST_MEMBERSHIP_REPORT:
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
		/* Is it our report looped back? */
		if (skb_rtable(skb)->fl.iif == 0)
			break;
		/* don't rely on MC router hearing unicast reports */
		if (skb->pkt_type == PACKET_MULTICAST ||
			skb->pkt_type == PACKET_BROADCAST)
			igmp_heard_report(in_dev, ih->group);
		break;
	case IGMP_PIM:
#ifdef CONFIG_IP_PIMSM_V1
		return pim_rcv_v1(skb);
#endif
	case IGMPV3_HOST_MEMBERSHIP_REPORT:
	case IGMP_DVMRP:
	case IGMP_TRACE:
	case IGMP_HOST_LEAVE_MESSAGE:
	case IGMP_MTRACE:
	case IGMP_MTRACE_RESP:
		break;
	default:
		break;
	}
#endif

drop:
	kfree_skb(skb);
	return 0;
}

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
