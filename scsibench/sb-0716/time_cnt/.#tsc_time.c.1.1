#include <stdio.h>
#include <string.h>
#include "tsc_time.h"

float MHZ_RATE=100.;

void time_tsc_init(){
	mhz_rate_init();
}

float mhz_rate(){
	return MHZ_RATE;
}

int mhz_rate_init(){
	float res, mhz;
	char line[256], *s, search_str[] = "cpu MHz";
	FILE *f;

	/* open proc/cpuinfo */
	if((f = fopen("/proc/cpuinfo", "r")) == NULL)
		return -1;

	/* ignore all lines until we reach MHz information */
	while(fgets(line, 256, f) != NULL){
		if(strstr(line, search_str) != NULL){
			/* ignore all characters in line up to : */
			for(s = line; *s && (*s != ':'); ++s);
			/* get MHz number */
			if(*s && (sscanf(s+1, "%f", &mhz) == 1)){
				res = mhz;
				break;
			}
		}
	}
	MHZ_RATE=res;
	return 0;
}

inline long long rdtsc(void){
  unsigned int tmp[2];

  __asm__ ("rdtsc"
	   : "=a" (tmp[1]), "=d" (tmp[0])
	   : "c" (0x10) );
  
  return ( ((long long)tmp[0] << 32 | tmp[1]) );
}

float ftime(long long time2, long long time1){
	return (time2-time1)>0 ? (time2-time1)/MHZ_RATE : (time1-time2)/MHZ_RATE ;
}

