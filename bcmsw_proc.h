/*
 * bcmsw_proc.h
 *
 *  Created on: May 23, 2012
 *      Author: jhkim
 */

#ifndef BCMSW_PROC_H_
#define BCMSW_PROC_H_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

typedef struct bcmsw_proc_s {
	int mode;
	char* name;
	int (*read) (char* page, char** atart, off_t off, int count, int *eof, void *data);
	/*int (*write) (char* page, char** atart, off_t off, int count, int *eof, void *data)*/
} bcmsw_proc_t;

int proc_init(void);
void proc_uninit(void);

int proc_register(const bcmsw_proc_t* node);

#endif /* BCMSW_PROC_H_ */
