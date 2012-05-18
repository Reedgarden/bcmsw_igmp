/*
 * bcmsw_proxy.c
 *
 *  Created on: May 17, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bcmsw_mii.h"
#include "bcmsw_igmp.h"

//schedule
static void tasklet_start(void);
void ethsw_igmp_peek(unsigned long node);

static int init_bcmsw_module(void)
{
	struct net_device* dev;

	dev = net_get_device();
	net_dev_test_mii_rw(dev);

	igmp_wrap_init();

	tasklet_start();
	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
	igmp_wrap_deinit();
}

static void tasklet_start(void)
{
	DECLARE_TASKLET(peek_igmp_send, ethsw_igmp_peek, NULL);
}
void ethsw_igmp_peek(unsigned long node)
{
	while(1)
	{
		printk("moduel ... \n");
		msleep(1000);
	}
}

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
