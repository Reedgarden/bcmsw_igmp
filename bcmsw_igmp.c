/*
 * bcmsw_igmp.c
 *
 *  Created on: May 18, 2012
 *      Author: jhkim
 */

#include "bcmsw_igmp.h"
#include "bcmsw_mii.h"
#include "bcmsw_snoop.h"

/* sys_open() system wrapping */
int (*original_handler)(struct sk_buff *skb);
static struct net_protocol* original_protocol;
static inline __u16 net_get_port(struct sk_buff *skb) {	return (skb_rtable(skb)->fl.iif == 0)? 8 : 2; }

void igmp_wrap_init(void)
{
	int hash = IPPROTO_IGMP & (MAX_INET_PROTOS -1);
	original_protocol = inet_protos[ hash ];

	original_handler = original_protocol->handler;
	original_protocol->handler = igmp_w_rcv;
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
	switch(ih->type) {
	case IGMP_HOST_MEMBERSHIP_REPORT:
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
	case IGMP_HOST_LEAVE_MESSAGE:
		if( set_ip_node(ih->type, ih->group, net_get_port(skb)) != 0 )
			printk(KERN_ERR "WARN!! Fail igmp Hooking!! \n");
		break;
	/* case another protocol */
	default:
		break;
	}
//	printk("+%s+ (%d) type 0x%0x group 0x%x \n",__func__, __LINE__, ih->type , ih->group );

	original_handler(skb);
	return 0;

toss:
	original_handler(skb);
	return 0;
}


