#include "bcmsw_mii.h"

struct net_device *bcmemac_get_device(void);
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, int port, int pull);

struct net_device* net_get_device(void)
{
	struct net_device* net_root_dev;
	net_root_dev = bcmemac_get_device();
	if(net_root_dev == NULL){
		printk( KERN_ALERT "cannot get device of bcmemac \n");
		return NULL;
	}
	return net_root_dev;
}

void net_dev_test_mii_rw(struct net_device* dev)
{
	unsigned char mac[6] = {0x01, 0x02, 0x03, 0x5e, 0x00, 0x01};
	unsigned char buff[8];
	unsigned int  data;
	int i,j;

	bcmsw_reg_set_igmp_entry(dev, mac, 8, 0);		// 0 - IGMP_JOIN
	bcmsw_reg_get_igmp_entry(dev, mac, buff, &data);

	for(i=0,j=0; i<6; i++) {
//		printk("0x%02x 0x%02x \n", buff[i], mac[i]);
		if(buff[i] != mac[i])
			j++;
	}

	printk(KERN_ALERT "[%s] %s \n", __func__, (j==0)?"SUCCESS":"FAILED" );
}

//void net_set_mac_node(__u8 type, __be32 group, __u16 port )
//{
//	printk(KERN_ALERT "+%s+ %d Type 0x%x Group 0x%x port %d \n", __func__, __LINE__, type, group, port );
//}
