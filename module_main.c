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

//schedule
static void tasklet_start(void);
void ethsw_igmp_peek(unsigned long node);
void delay_loop(unsigned long t);

char my_tasklet_data[]="my_tasklet_function was called";
DECLARE_TASKLET(peek_igmp_send, ethsw_igmp_peek, (unsigned long)&my_tasklet_data);

static int init_bcmsw_module(void)
{
	struct net_device* dev;

	dev = net_get_device();
	net_dev_test_mii_rw(dev);

	tasklet_start();

	igmp_wrap_init();

	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
	igmp_wrap_deinit();

	tasklet_kill(&peek_igmp_send);
}

static void tasklet_start(void)
{

	tasklet_schedule(&peek_igmp_send);
}
void ethsw_igmp_peek(unsigned long node)
{
//	while(1)
//	{
		printk("moduel ... %s \n", (char*)node);
//		delay_loop(1);
//	}
}

void delay_loop(unsigned long t)
{
	unsigned long delay = jiffies + t * HZ;
	while( time_before( jiffies, delay ));
}

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
