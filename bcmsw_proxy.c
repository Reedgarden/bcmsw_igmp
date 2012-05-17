/*
 * bcmsw_proxy.c
 *
 *  Created on: May 17, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bcmsw_proxy.h"
/*
 * export_symbol from bcmgenet.c
 */
struct net_device* net_root_dev;
struct net_device *bcmemac_get_device(void);
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, int port, int pull);

/*
 * STATIC FUNCTIONS
 */
static void init_net_device(void);
static void net_dev_test_mii_rw(void);

static int init_bcmsw_module(void)
{
	printk("init_bcmsw_module\n");
	init_net_device();
	net_dev_test_mii_rw();

	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
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

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
