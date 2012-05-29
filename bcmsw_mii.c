/*
 * bcmsw_mii.c
 *
 *  Created on: May 18, 2012
 *      Author: jhkim
 */

#include "bcmsw_mii.h"
#include <linux/inetdevice.h>

struct net_device *bcmemac_get_device(void);

/* IMPORT EXPORTED SYMBOL */
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned short portmap);
void ethsw_igmp_mcst_dfl_map(struct net_device *dev, unsigned short portmap);

struct net_device* net_get_device(void)
{
	struct net_device* net_root_dev;
	net_root_dev = bcmemac_get_device();
	if(net_root_dev == NULL){
		printk( KERN_ALERT "cannot get device of bcmemac \n");
		return NULL;
	}
	return net_root_dev;
}

void net_dev_set_dfl_map(unsigned short portmap)
{
	struct net_device* dev;
	dev = bcmemac_get_device();
	ethsw_igmp_mcst_dfl_map(dev, portmap);
}

int net_dev_mii_write(unsigned char* mac, unsigned short portmap)
{
	struct net_device* dev = net_get_device();
	bcmsw_reg_set_igmp_entry(dev, mac, portmap);		// 0 - IGMP_JOIN
	/*ethsw_igmp_arl_entry_set(dev, mac, portmap);*/
	return 0;
}
unsigned int net_dev_get_up(void)
{
	struct net_device* dev = net_get_device();
	__be32 saddr=0;
	saddr = inet_select_addr(dev,0,0);	// get local host ip address
	return saddr;
}

#if 0	// this is test code
void net_dev_test_mii_rw(struct net_device* dev)
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
#endif
