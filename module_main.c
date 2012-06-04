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
#include "bcmsw_proc.h"
#include "bcmsw_statistics.h"

static int init_bcmsw_module(void)
{
	proc_init();
	node_init();
	igmp_wrap_init();
	statistics_init();
	printk("@@@ bcmsw_module inserted @@@\n");

#if 0 // test codes
	struct net_device* dev;
	dev = net_get_device();
	net_dev_test_mii_rw(dev);
	// test codes
	set_ip_node(0x01,0xffffffef,0x01);
#endif
	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("@@@ bcmsw_module deinit @@@\n");
	statistics_init();
	igmp_wrap_deinit();
	node_uninit();
	proc_uninit();
}


module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
