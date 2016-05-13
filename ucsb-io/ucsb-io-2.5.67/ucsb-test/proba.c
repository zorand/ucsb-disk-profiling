#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "../include/linux/ucsb_io.h"

#define O_DIRECT         040000 /* direct disk access hint */

char direct_buf[512*4*11];
main(int argc, char **argv){
	int ret;
	int rate,priority;
	struct ucsb_io qos;
	int fd;
	char *buf,buf2[1000*4096+4096];
	
	
	buf = (char *) (((long)buf2+4095) & (-1L << 12));
	printf("%p %p\n",buf2, buf);
	
	
	if (argc<2) exit(-1);
	
	fd = open(argv[1],O_RDONLY | O_DIRECT);
	if (fd<0) exit(-1);
	
	rate = 1203;
//	ret = ioctl(fd,UCSB_IO_SOFT_RT,&rate);
//	ret = ioctl(fd,UCSB_IO_GET_QOS,&qos);
//	if (ret){
//		printf("Error in ioctl call\n");
//		exit(-1);
//	}
//	printf("QoS: %d, %d, %d\n",qos.type,qos.priority,qos.rate);

	while ((ret=read(fd,buf,400*4096+512)) > 0){
//		printf("%d bytes read\n",ret); 
	}
	if (ret<0) perror("file read:");
	

//	priority = 321;
//	ret = ioctl(fd,UCSB_IO_PRIORITY,&priority);
//	ret = ioctl(fd,UCSB_IO_GET_QOS,&qos);
//	if (ret){
//		printf("Error in ioctl call\n");
//		exit(-1);
//	}
//	
//	printf("QoS: %d, %d, %d\n",qos.type,qos.priority,qos.rate);
	
}
