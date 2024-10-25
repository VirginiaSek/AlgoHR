#ifndef DUMMY_HEART_RATE
#define DUMMY_HEART_RATE
#include "../types.h"

// Definizione per abilitare la stampa dei risultati e la creazione delle directory

#define ENABLE_RESULT_OUTPUT

#ifdef ENABLE_RESULT_OUTPUT
void dummy_create_result_directory(const char *algorithm_name);
void dummy_save_results(const char *file_path, uint64_t timestamp, int bpm_bangle, int bpm_polar);
#endif

void dummy_heartrate_init();
int dummy_heartrate(time_delta_ms_t delta_ms, int ppg_raw);

#endif
