/*
 * bcmsw_igmp.c
 *
 *  Created on: May 18, 2012
 *      Author: jhkim
 */

#include <linux/ip.h>

#include "bcmsw_igmp.h"
#include "bcmsw_mii.h"
#include "bcmsw_snoop.h"

#define IGMP_REG_MODE_OFF 	(0)
#define IGMP_REG_MODE_ON 	(1)

#define IGMP_PORT_NUM_1		(1)
#define IGMP_PORT_NUM_2		(2)
#define IGMP_PORT_NUM_IMP	(8)

/* IMPORT EXPORTED SYMBOL */
int ethsw_igmp_mode(struct net_device *dev, int mode);

/* sys_open() system wrapping */
int (*original_handler)(struct sk_buff *skb);
static struct net_protocol* original_protocol;
static int igmp_packet_check_loop(struct sk_buff *skb);
static inline __u16 net_get_port(struct sk_buff *skb) {	return (skb_rtable(skb)->fl.iif == 0)? 8 : 2; }

void igmp_wrap_init(void)
{
	struct net_device* dev;
	int hash = IPPROTO_IGMP & (MAX_INET_PROTOS -1);

	// igmp protocol hook
	original_protocol = inet_protos[ hash ];
	original_handler = original_protocol->handler;
	original_protocol->handler = igmp_w_rcv;

	// igmp mode setting
	dev = net_get_device();
	ethsw_igmp_mode(dev, IGMP_REG_MODE_OFF);
}

void igmp_wrap_deinit(void)
{
	original_protocol->handler = original_handler;
}

/*
 * imp igmp hooking function
 */
int igmp_w_rcv(struct sk_buff *skb)
{
	struct igmphdr *ih;
	struct iphdr *iph;
	struct in_device *in_dev = __in_dev_get_rcu(skb->dev);
	int result;

	if (in_dev == NULL)
		goto toss;

	if (!pskb_may_pull(skb, sizeof(struct igmphdr)))
		goto toss;

	switch (skb->ip_summed) {
	case CHECKSUM_COMPLETE:		// what is this mean?
		if (!csum_fold(skb->csum))
			break;
		/* fall through */
	case CHECKSUM_NONE:
		skb->csum = 0;
		if (__skb_checksum_complete(skb))
			goto toss;
	}

	ih = igmp_hdr(skb);
	iph = ip_hdr(skb);

	switch(ih->type) {
	case IGMP_HOST_MEMBERSHIP_REPORT:
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
	case IGMP_HOST_LEAVE_MESSAGE:
		if( set_ip_node(ih->type, ih->group, igmp_packet_check_loop(skb)) != 0 )
			printk(KERN_ERR "WARN!! Fail igmp Hooking!! \n");
#if 0
		if( set_ip_node(ih->type, ih->group, net_get_port(skb)) != 0 )
			printk(KERN_ERR "WARN!! Fail igmp Hooking!! \n");
#endif
		break;
	/* case another protocol */
	default:
		break;
	}

	original_handler(skb);
	return 0;

toss:
	original_handler(skb);
	return 0;
}

static int igmp_packet_check_loop(struct sk_buff *skb)
{
	unsigned int lo_addr = net_dev_get_up();	// get local host address
	struct iphdr *iph = ip_hdr(skb);
	return (lo_addr == iph->saddr) ? IGMP_PORT_NUM_IMP : IGMP_PORT_NUM_2;
}
