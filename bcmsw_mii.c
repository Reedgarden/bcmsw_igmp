#include "bcmsw_mii.h"

struct net_device *bcmemac_get_device(void);
int bcmsw_reg_get_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned char* buff, unsigned int* data);
//int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, int port, int pull);
int bcmsw_reg_set_igmp_entry(struct net_device *dev, unsigned char* mac, unsigned short portmap);

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

#if 0
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
#endif

int net_dev_mii_write(unsigned char* mac, unsigned short portmap)
{
	struct net_device* dev = net_get_device();
	bcmsw_reg_set_igmp_entry(dev, mac, portmap);		// 0 - IGMP_JOIN
	return 0;
}
