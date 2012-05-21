/*
 * bcmsw_proxy.c
 *
 *  Created on: May 17, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include "bcmsw_mii.h"
#include "bcmsw_igmp.h"
#include "bcmsw_snoop.h"

//schedule
void ethsw_igmp_peek(unsigned long node);
void delay_loop(unsigned long t);


static int init_bcmsw_module(void)
{
/*	struct net_device* dev;
	dev = net_get_device();*/
	/*net_dev_test_mii_rw(dev);*/

	node_init();

	// test codes
	set_ip_node(0x01,0xffffffef,0x01);

	igmp_wrap_init();

	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
	igmp_wrap_deinit();
	node_uninit();
}


module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
