/*
 * bcmsw_kwatch.c
 *
 *  Created on: May 29, 2012
 *      Author: jhkim
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>

#include "bcmsw_kwatch.h"

typedef struct
{
	struct list_head _list_head;
	int cnt;
} kwatch_list;

typedef struct
{
	char* name;
	struct timeval t_time;
	struct list_head _node;

} kwatch_t;

static kwatch_list* get_kwatch_list(void);
static kwatch_t* kwatch_new(const char* name);
static void put_kwatch_list(kwatch_list* list, kwatch_t* node);

///////////////////////// API functions
kwatch kwatch_start(const char* name)
{
	// check already exist
	kwatch_list* list = get_kwatch_list();
	kwatch_t* watch;
	struct list_head* head = &list->_list_head;
	struct list_head* ptr;

	// for loop should be changed!!
//	if(list->cnt != 0) {
//		for(ptr = head->next; ptr != head; ptr = ptr->next) {
//			watch = list_entry(ptr, kwatch_t, _node);
//			if(strncmp(watch->name,name, (strlen(name)) == 0))
//				return watch;
//		}
//	}

	//cannot find
	watch = kwatch_new(name);
	put_kwatch_list(list, watch);
	return watch;
}

unsigned long kwatch_lap_sec(kwatch handle)
{
	kwatch_t* watch;
	watch = (kwatch_t*)handle;
	struct timeval _time;
	unsigned long old_sec , new_sec, result;

	old_sec = (watch->t_time.tv_sec*1000000) +  watch->t_time.tv_usec;

	if(watch == NULL)
		return -1;

	do_gettimeofday(&_time);
	new_sec = (_time.tv_sec*1000000) +  _time.tv_usec;
	result = (new_sec - old_sec);
	printk("@@@ %s @@@ [%u] us \n",watch->name,(new_sec-old_sec));

	return result;
}

int kwatch_stop(kwatch handle)
{
	kwatch_t* watch;
	int sec;

	kwatch_t* _victim;
	kwatch_list* list = get_kwatch_list();
	struct list_head* head = &list->_list_head;
	struct list_head* ptr;

	watch = (kwatch_t*)handle;
	sec = watch->t_time.tv_sec;

	for(ptr = head->next; ptr != head; ) {
		_victim = list_entry(ptr, kwatch_t, _node);
		ptr = ptr->next;

		if(strncmp(watch->name,_victim->name, (strlen(watch->name)) == 0))
		{
			list_del(&watch->_node);
			kfree(watch);
			handle = NULL;
		}
	}

	return sec;
}

void kwatch_reset(kwatch handle)
{
	kwatch_t* watch;
	watch = (kwatch_t*)handle;

	do_gettimeofday(&watch->t_time);
}



///////////////////////// static functions
static kwatch_list* get_kwatch_list(void)
{
	static kwatch_list* _list;
	if(_list == NULL)
	{
		// mem alloc
		_list = (kwatch_list*)kmalloc(sizeof(kwatch_list), GFP_KERNEL);
		//list init
		INIT_LIST_HEAD(&_list->_list_head);
		_list->cnt=0;
	}
	return _list;
}

static void put_kwatch_list(kwatch_list* list, kwatch_t* node)
{
	list_add_tail(&node->_node, &list->_list_head);
	list->cnt++;
}

static kwatch_t* kwatch_new(const char* name)
{
	kwatch_t* watch;
//	printk("***kwatch_new watch created with %s\n", name);
	watch = (kwatch_t*)kmalloc(sizeof(kwatch_t), GFP_KERNEL);
	watch->name = (char*)name;

	//start timmer
	do_gettimeofday(&watch->t_time);
	return watch;
}
