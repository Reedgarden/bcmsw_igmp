/*
 * bcmsw_qos.c
 *
 *  Created on: May 31, 2012
 *      Author: jhkim
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bcmsw_proc.h"
#include "bcmsw_mii.h"

static int show_rx_unipkts(char* page, char** atart, off_t off, int count, int *eof, void *data);
static int show_rx_multipkts(char* page, char** atart, off_t off, int count, int *eof, void *data);
static int show_tx_unipkts(char* page, char** atart, off_t off, int count, int *eof, void *data);
static int show_tx_multipkts(char* page, char** atart, off_t off, int count, int *eof, void *data);

const bcmsw_proc_t proc_rx_ucstpkt = {
		0444, 	//mode
		"RxUnicastPkts",
		show_rx_unipkts
};

const bcmsw_proc_t proc_rx_mcstpkt = {
		0444, 	//mode
		"RxMulticastPkts",
		show_rx_multipkts
};

const bcmsw_proc_t proc_tx_ucstpkt = {
		0444, 	//mode
		"TxUnicastPkts",
		show_tx_unipkts
};

const bcmsw_proc_t proc_tx_mcstpkt = {
		0444, 	//mode
		"TxMulticastPkts",
		show_tx_multipkts
};

void statistics_init(void)
{
	// TODO : QOS Enable here
	// port based qos enable call!!

	// register proc mac_table_read
	proc_register(&proc_rx_ucstpkt);
	proc_register(&proc_rx_mcstpkt);
	proc_register(&proc_tx_ucstpkt);
	proc_register(&proc_tx_mcstpkt);

}

void statistics_uninit(void)
{
	// none .. proc will be freed by proc manager
}

static int show_rx_unipkts(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len;
	unsigned int pkts;
	net_dev_rx_ucst_pkts((unsigned char*)&pkts);
	len= sprintf(page,"%d\n", pkts);
	*eof = 1;
	return len;
}


static int show_rx_multipkts(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len;
	unsigned int pkts;
	net_dev_rx_mcst_pkts((unsigned char*)&pkts);
	len= sprintf(page,"%d\n", pkts);
	*eof = 1;
	return len;
}

static int show_tx_unipkts(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len;
	unsigned int pkts;
	net_dev_tx_ucst_pkts((unsigned char*)&pkts);
	len= sprintf(page,"%d\n", pkts);
	*eof = 1;
	return len;
}


static int show_tx_multipkts(char* page, char** atart, off_t off, int count, int *eof, void *data)
{
	int len;
	unsigned int pkts;
	net_dev_tx_mcst_pkts((unsigned char*)&pkts);
	len= sprintf(page,"%d\n", pkts);
	*eof = 1;
	return len;
}

