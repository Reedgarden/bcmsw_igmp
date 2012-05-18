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

/*
 * export_symbol from bcmgenet.c
 */
struct net_device *bcmemac_get_device(void);
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, int port, int pull);

/*
 * STATIC FUNCTIONS
 */
//bcmemac//dev//mii//
static struct net_device* net_get_device(void);
static void net_dev_test_mii_rw(struct net_device*);
//igmp//hook//
static void igmp_wrap_init(void);
static void igmp_wrap_deinit(void);
static int igmp_w_rcv(struct sk_buff *skb);

/* sys_open() system wrapping */
int (*original_handler)(struct sk_buff *skb);
struct net_protocol* original_protocol;

static int init_bcmsw_module(void)
{
	struct net_device* dev;

	dev = net_get_device();
	net_dev_test_mii_rw(dev);

	igmp_wrap_init();
	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
	igmp_wrap_deinit();
}

static struct net_device* net_get_device(void)
{
	struct net_device* net_root_dev;
	net_root_dev = bcmemac_get_device();
	if(net_root_dev == NULL){
		printk( KERN_ALERT "cannot get device of bcmemac \n");
		return NULL;
	}
	return net_root_dev;
}

static void net_dev_test_mii_rw(struct net_device* dev)
{
	unsigned char mac[6] = {0x01, 0x02, 0x03, 0x5e, 0x00, 0x01};
	unsigned char buff[8];
	unsigned int  data;
	int i,j;

	bcmsw_reg_set_igmp_entry(dev, mac, 8, 0);		// 0 - IGMP_JOIN
	bcmsw_reg_get_igmp_entry(dev, mac, buff, &data);

	for(i=0,j=0; i<6; i++) {
//		printk("0x%02x 0x%02x \n", buff[i], mac[i]);
		if(buff[i] != mac[i])
			j++;
	}

	printk(KERN_ALERT "[%s] %s \n", __func__, (j==0)?"SUCCESS":"FAILED" );
}

static void igmp_wrap_init(void)
{
	int hash = IPPROTO_IGMP & (MAX_INET_PROTOS -1);
	original_protocol = inet_protos[ hash ];

	original_handler = original_protocol->handler;
	original_protocol->handler = igmp_w_rcv;
}

static void igmp_wrap_deinit(void)
{
	original_protocol->handler = original_handler;
}

/*
 * imp igmp hooking function
 */
static int igmp_w_rcv(struct sk_buff *skb)
{
	struct igmphdr *ih;
	struct sk_buff *inner_skb = skb_copy(skb, NULL);		// copy buff to inner buff
	struct in_device *in_dev = __in_dev_get_rcu(inner_skb->dev);
	int len = inner_skb->len;

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

drop:
	kfree_skb(skb);
	return 0;
}

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
