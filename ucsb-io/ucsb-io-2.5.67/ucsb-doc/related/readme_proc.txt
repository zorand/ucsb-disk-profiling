example for proc stdout:

proc_misc.c
	create_seq_entry()
	kstat_read_proc():
		sprintf(page, "cpu  %u %u %u %u %u\n",
                  jiffies_to_clock_t(user),
                  jiffies_to_clock_t(nice),
                  jiffies_to_clock_t(system),
                  jiffies_to_clock_t(idle),
                  jiffies_to_clock_t(iowait));

example for stdin (Mohit TODO):

/proc/sys/fs/file-max
	
fs/root.c:
	proc_root_init(void)
		handles mkdir in proc fs...

kernel/sysctl.c:
	proc_dointvec() generic function, probably all we need to use! grep
		for file-max in the same file to see how it is registered. 

	