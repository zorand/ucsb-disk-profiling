#ifndef _UCSB_IO_H
#define _UCSB_IO_H

#include <linux/fs.h>

#define UCSB_IO
#define UCSB_DEBUG

#define UCSB_DEF_TIMECYCLE 1000		/* in milliseconds */
#define UCSB_DEF_LOGFLAG 0		/* tunr off ucsb_debug at boot time */

typedef enum ucsb_QoS {
	UCSB_BESTEFFORT = 0,	
	UCSB_SOFT_RT	= 1,
	UCSB_HARD_RT	= 2,		/* not yet used */
	UCSB_PREEMPT	= 3,		/* not yet used */
} ucsb_QoS_t;

#define UCSB_DEF_PRIORITY 16

struct ucsb_io {
	ucsb_QoS_t 	type;
	int 		priority;
	int		rate;		/* not yet used */
	struct file	*filp;		/* associated file if exists */
};

/* ioctl commands: */
#define UCSB_IOCTL_BASE		'z'
#define UCSB_IO_GET_QOS		_IOR(UCSB_IOCTL_BASE,0,sizeof(struct ucsb_io))
#define UCSB_IO_BESTEFFORT	_IO(UCSB_IOCTL_BASE,1)
#define UCSB_IO_SOFT_RT		_IOW(UCSB_IOCTL_BASE,2,sizeof(int))
#define UCSB_IO_HARD_RT		_IOW(UCSB_IOCTL_BASE,3,sizeof(int))
#define UCSB_IO_PRIORITY	_IOW(UCSB_IOCTL_BASE,4,sizeof(int))



/* Stat and global state support */
struct ucsb_io_stat {
	int logflag;
	int time_cycle;		/* not yet used - for kernel cycle-based scheduling */
};


#ifndef UCSB_STAT_DEF 
extern struct ucsb_io_stat ucsb_stat;
#endif

/* Debug helper macros */
#ifdef UCSB_DEBUG
#define ucsb_debug(fmt,arg...) \
        printk(KERN_DEBUG "UCSB " fmt,##arg)
#else
#define ucsb_debug(fmt,arg...) \
	do { } while (0)
#endif

#define ucsb_log(fmt,arg...) do {if(ucsb_stat.logflag) ucsb_debug(fmt,##arg); } while (0)
                
#endif  /* _UCSB_IO_H */
