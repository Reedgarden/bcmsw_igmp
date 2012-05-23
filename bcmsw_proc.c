/*
 * bcmsw_proc.c
 *
 *  Created on: May 23, 2012
 *      Author: jhkim
 */
#include <linux/list.h>
#include "bcmsw_proc.h"

#define PROCFS_DIR 	"bcmswitch"
#define PROC_NAME_L 20

typedef struct bcmsw_proc_node {
	char name[PROC_NAME_L];
	struct proc_dir_entry* proc;
	struct list_head _list;
} bcmsw_proc_node;

typedef struct bcmsw_proc_dev_s {
	char* dev_name;
	struct proc_dir_entry* dev_dir;
	struct list_head list_head;
} bcmsw_proc_dev_s;


struct bcmsw_proc_dev_s* get_bcmsw_dir(void)
{
	static bcmsw_proc_dev_s dev;
	if(dev.dev_dir == NULL)
	{
		dev.dev_dir = proc_mkdir(PROCFS_DIR, NULL);	// mkdir in /proc/*
		dev.dev_name = PROCFS_DIR;
		INIT_LIST_HEAD(&dev.list_head);
	}
	return &dev;
}

int proc_init(void)
{
	bcmsw_proc_dev_s* dev;
	dev = get_bcmsw_dir();			// create dir with init
	return 0;	/* everything is ok */
}

void proc_uninit(void)
{
	struct list_head *ptr;
	bcmsw_proc_node *tmp;

	bcmsw_proc_dev_s*dev;
	dev = get_bcmsw_dir();

	if(&dev->list_head != dev->list_head.next)
	{
		for(ptr = dev->list_head.next; ptr != &dev->list_head; ) {
			tmp = list_entry(ptr, bcmsw_proc_node, _list);
			ptr = ptr->next;
			// del from list
			list_del(&tmp->_list);
			// delete proc file
			remove_proc_entry(tmp->name, dev->dev_dir);
			kfree(tmp);
		}
	}

	remove_proc_entry(dev->dev_name, NULL);
}

int proc_register(const bcmsw_proc_t* node)
{
	bcmsw_proc_node* i_node;
	bcmsw_proc_dev_s* dev;
	dev = get_bcmsw_dir();

	if(strlen(node->name) >= PROC_NAME_L)
		return -1;

	// kmalloc
	i_node = (bcmsw_proc_node*)kmalloc(sizeof(bcmsw_proc_node), GFP_KERNEL);
	if(i_node == NULL) {
		printk(KERN_ALERT "Error: Could not malloc \n");
		return -ENOMEM;
	}

	// create proc file system
	i_node->proc = create_proc_entry(node->name, node->mode, dev->dev_dir);
	if(i_node->proc == NULL) {
		remove_proc_entry(node->name, dev->dev_dir);
		printk(KERN_ALERT "Error: Could not initialize ..\n");
		return -ENOMEM;
	}

	memcpy(i_node->name, node->name, PROC_NAME_L);

	i_node->proc->read_proc = node->read;
	i_node->proc->mode		= S_IFREG | S_IRUGO;
	i_node->proc->uid		= 0;
	i_node->proc->gid		= 0;
	i_node->proc->size		= 100;	// ?? ..

	// add to list
	list_add_tail(&i_node->_list, &dev->list_head);
	return 0;
}

