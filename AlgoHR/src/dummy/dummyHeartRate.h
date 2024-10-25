#ifndef DUMMY_HEART_RATE
#define DUMMY_HEART_RATE
#include "../types.h"
/**
* Initialize and reset the heart rate monitor.
*/
void dummy_heartrate_init();
 
/**
* Registers a new PPG data point for heart rate calculation.
*
* delta_ms: time difference in milliseconds between this sample and the previous one.
* ppg_raw: raw PPG signal value.
*
* returns: the simulated heart rate (BPM) calculated so far.
*/
int dummy_heartrate(time_delta_ms_t delta_ms, int ppg_raw);
 
#endif

