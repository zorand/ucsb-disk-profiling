/* returns MHz-rate of the processor (or -1 if not available)                 */

void time_tsc_init();
int mhz_rate_init();

float ftime(long long, long long);
inline long long rdtsc();
float mhz_rate();
