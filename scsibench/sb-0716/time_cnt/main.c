#include <stdio.h>
#include "tsc_time.h"

main(){
	long long time1, time2;

	time_tsc_init();
	
	time1 = rdtsc();
	
	sleep(1);
	
	time2 = rdtsc();
	
	printf("%f \n",(time2-time1)/mhz_rate() );
	printf("%10lu\n",time2-time1);
	
	printf("%f\n",mhz_rate());
	
}
