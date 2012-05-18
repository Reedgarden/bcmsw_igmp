/*
 * bcmsw_igmp.h
 *
 *  Created on: May 18, 2012
 *      Author: jhkim
 */

#ifndef BCMSW_IGMP_H_
#define BCMSW_IGMP_H_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/igmp.h>
#include <linux/inetdevice.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <net/net_namespace.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/checksum.h>
#include <linux/netfilter_ipv4.h>

void igmp_wrap_init(void);
void igmp_wrap_deinit(void);
int igmp_w_rcv(struct sk_buff *skb);

#endif
