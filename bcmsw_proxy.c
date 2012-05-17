/*
 * bcmsw_proxy.c
 *
 *  Created on: May 17, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

static int init_bcmsw_module(void)
{
	printk("init_bcmsw_module\n");
	return 0;
}

static void deinit_bcmsw_module(void)
{
	printk("deinit\n");
}

module_init( init_bcmsw_module );
module_exit( deinit_bcmsw_module );

MODULE_LICENSE("GPL");
