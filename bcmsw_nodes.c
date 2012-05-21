/*
 * bcmsw_nodes.c
 *
 *  Created on: May 21, 2012
 *      Author: jhkim
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>

struct task_struct *task;
int data;
int ret;
int thread_function(void *data)
{
	while(1)
	{
		printk(KERN_INFO" Hi! thread! \n");
		msleep(1000);
		if(kthread_should_stop())
			break;
	}
	return 0;
}

void node_init(void)
{
	task = kthread_create(&thread_function, (void*)data, "nodes_thread");
	wake_up_process(task);
	printk(KERN_INFO"Kernel Thread : %s \n", task->comm);
}
void node_uninit(void)
{
	ret = kthread_stop(task);
}
