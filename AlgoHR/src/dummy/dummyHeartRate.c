/*
* ----------------------------------------------------------------------------
* Dummy Heart Rate Monitor
* ----------------------------------------------------------------------------
*/
#include <stdint.h>
#include <math.h>
 
#include "dummyHeartRate.h"
#include "../types.h"
// Time passed for simulation
long long dummy_time_passed = 0;
 
/// Initialise heart rate calculation
void dummy_heartrate_init()
{
    dummy_time_passed = 0;
}
 
/// Process the PPG data and return heart rate (always 80 BPM)
int dummy_heartrate(time_delta_ms_t delta_ms, int ppg_raw)
{
    // Simulate time passing (if needed for other purposes)
    if (delta_ms > 0)
    {
        dummy_time_passed += delta_ms;
    }
 
    // Return constant heart rate value of 80 BPM
    return 80;
}