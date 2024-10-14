/*
 * ----------------------------------------------------------------------------
 * Dummy Heart Rate Monitor
 * ----------------------------------------------------------------------------
 */
#include <stdint.h>
#include <math.h>

#include "dummyhr.h"
#include "../types.h"
// Time passed for simulation
long long dummy_time_passed = 0;
const float dummy_hr_rate_per_sec = 1.2; // Simulate heart rate change

/// Initialise heart rate calculation
void dummy_heart_rate_init()
{
    dummy_time_passed = 0;
}

/// Process the PPG data and return heart rate (in BPM)
int dummy_heart_rate(time_delta_ms_t delta_ms, int ppg_raw)
{
    if (delta_ms > 0)
    {
        // Simulate time passing
        dummy_time_passed += delta_ms;
    }

    // Convert time passed into seconds
    float timepassed_secs = (float)dummy_time_passed / 1000;

    // Dummy heart rate logic: simulate BPM increasing or decreasing based on time
    // Adjust the heart rate every second slightly to simulate fluctuation
    int simulated_bpm = 60 + (int)(dummy_hr_rate_per_sec * timepassed_secs);

    // Simple cap to prevent unreasonable simulated values
    if (simulated_bpm > 180) simulated_bpm = 180;
    if (simulated_bpm < 40) simulated_bpm = 40;

    return simulated_bpm;
}
