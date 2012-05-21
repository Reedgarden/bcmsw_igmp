/*
 * bcmsw_snoop.h
 *
 *  Created on: May 21, 2012
 *      Author: jhkim
 */

#ifndef BCMSW_SNOOP_H_
#define BCMSW_SNOOP_H_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock_types.h>

void node_init(void);
void node_uninit(void);

int set_ip_node(__u8 type, __be32 group, __u16 port );

#endif /* BCMSW_SNOOP_H_ */
