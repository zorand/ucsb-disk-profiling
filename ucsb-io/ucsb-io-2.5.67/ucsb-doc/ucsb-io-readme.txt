
(C) Zoran Dimitrijevic, March 2003
    zoran@cs.ucsb.edu

UCSB_IO: Disk QoS for Linux

This is the first draft of future implementation of QoS for disk access. 
The idea is to add minimal support in Linux so that lower layers (disk 
scheduler) has more information about the IOs it services. Our approach 
is to add QoS parameters per each file descriptor. We added pointer to 
this QoS structure in struct bio. 

For now, all existing IOs will be considered as best effort and serviced 
by default disk scheduler. If some IOs are marked with our QoS structure, 
then we put them to our ucsb_queue (priority based for now). Application 
can specify QoS per file descriptor using ioctl(). 

We plan to write paper about this and to show how we can implement 
guaranteed-rate time-cycle-based scheduler in user space with user-level
admission control. If we have to, then we will implement cycle-based
scheduler in kernel. This meta-scheduling can be a module. Also, some
kernel IOs (for example from virtual memory subsystem) may be more
important, so we can flag them with our QoS parameters as higher
priority. 

TODO: The ioctl() doesn't have any permissions/security. And the place
where we implemented disk QoS is probably not the best one... O:)

TODO: Add #define in menuconfig (we have #defines througout the patch)

TODO: Make the actuall diff file... 0000;)

TODO: Write user-level cycle-based scheduling for guaranteed-rate streams
(videos, other RT...) See if we need to put it in kernel.

For more info please refer to our related publications:

A paper about preemptible disk IOs (Usenix File and Storage Technology 
2003) and we plan to implement it in Linux software RAID and maybe even 
in regular disk scheduler if needed. For that we need these info.

We also have paper at IEEE Multimedia about user-level admission control
for disks. We need priorities in kernel scheduler to be able to guarantee 
anything.

Papers are at:
http://www.cs.ucsb.edu/~zoran/www/publications.html

