/*
 * bcmsw_mii.h
 *
 *  Created on: May 18, 2012
 *      Author: jhkim
 */

#ifndef BCMSW_MII_H_
#define BCMSW_MII_H_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>

#include <net/net_namespace.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/protocol.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/checksum.h>

struct net_device* net_get_device(void);

int net_dev_mii_write(unsigned char* mac, unsigned short portmap);
unsigned int net_dev_get_up(void);
void net_dev_set_dfl_map(unsigned short portmap);
int net_dev_phy_link_ok(int port);

#if 0
void net_dev_rx_ucst_pkts(unsigned char* buf);
void net_dev_rx_mcst_pkts(unsigned char* buf);
#endif

#if 0	// this is test code
void net_dev_test_mii_rw(struct net_device*);
int net_dev_mii_write(struct net_device*, );
int net_dev_mii_read (struct net_device*, );
#endif

#endif /* BCMSW_MII_H_ */
