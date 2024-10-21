#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "./types.h"

#include "./rolling_stats.h"

#include "./dummy/dummyHeartRate.h"
// #include "./bangle_simple/bangle_simple.h"
// #include "./espruino/espruino.h"
// #include "./oxford/oxford.h"
// #include "./panTompkins/pt.h"
// #include "./autocorrelation/autocorrelation.h"

typedef struct Algo
{
    char *name;
    Stats *stats;
    void (*init)();
    bpm_t (*bpm_calculate)(time_delta_ms_t delta_ms, ppg_raw_t ppg);
    bpm_t bpm;
} Algo;

////////////////////////////////////
// START MODIFY HERE TO ADD NEW ALGO

// all algorithms:
const int algoN = 1; // change this to algo number!
Algo algos[algoN];

void createAlgos()
{
    algos[0] = (Algo){
        .name = "Dummy",
        .stats = malloc(sizeof(Stats)),
        .init = dummy_heartrate_init,
        .bpm_calculate = dummy_heartrate, // Adattato per BPM
        .bpm = 0,
    };
}

// END MODIFY HERE TO ADD NEW ALGO
////////////////////////////////////

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("Usage: %s <bangle_dir> <polar_dir> <reference_csv> <results>\n", argv[0]);
        return 1;
    }

    DIR *bangle_dir;
    DIR *polar_dir;
    struct dirent *bangle_entry;
    struct dirent *polar_entry;

    // Open the directories
    bangle_dir = opendir(argv[1]);
    if (bangle_dir == NULL)
    {
        perror("Cannot open Bangle directory");
        return 1;
    }

    polar_dir = opendir(argv[2]);
    if (polar_dir == NULL)
    {
        perror("Cannot open Polar directory");
        return 1;
    }

    // Open reference CSV file
    FILE *ref_fp = fopen(argv[3], "r");
    if (ref_fp == NULL)
    {
        perror("Cannot open reference file");
        return 1;
    }

    // Create output file
    FILE *out_fp = fopen(argv[4], "w");
    if (out_fp == NULL)
    {
        perror("Error creating results file");
        return 1;
    }

    createAlgos();

    // Write header of output file
    fprintf(out_fp, "FILENAME,Reference,");
    for (int i = 0; i < algoN; i++)
    {
        fprintf(out_fp, "%s", algos[i].name);
        if (i < algoN - 1)
            fprintf(out_fp, ",");
    }
    fprintf(out_fp, "\n");

    // Initialize all stats
    for (int i = 0; i < algoN; i++)
    {
        rolling_stats_reset(algos[i].stats);
    }

    // Prepare to process files
    char ref_line[1024];
    char line[1024];
    
    // Read through each Bangle file (Smartwatch data)
    while ((bangle_entry = readdir(bangle_dir)) != NULL)
    {
        if (strstr(bangle_entry->d_name, ".csv") == 0)
        {
            continue; // Exclude non-CSV files
        }

        char bangle_filename[256];
        snprintf(bangle_filename, sizeof(bangle_filename), "%s/%s", argv[1], bangle_entry->d_name);

        FILE *bangle_fp = fopen(bangle_filename, "r");
        if (bangle_fp == NULL)
        {
            perror("Cannot open Bangle file");
            continue;
        }

        char polar_filename[256] = {0};
        int found_reference = 0;
        
        // Find the corresponding Polar file from the reference CSV
        while (fgets(ref_line, sizeof(ref_line), ref_fp) != NULL)
        {
            ref_line[strcspn(ref_line, "\n")] = 0;

            if (strstr(ref_line, bangle_entry->d_name) != NULL)
            {
                sscanf(ref_line, "%*[^,],%*[^,],%[^,]", polar_filename);
                found_reference = 1;
                break;
            }
        }

        if (!found_reference || strlen(polar_filename) == 0)
        {
            printf("Cannot find reference for %s\n", bangle_entry->d_name);
            fclose(bangle_fp);
            continue;
        }

        // Open the corresponding Polar file
        char polar_fullpath[256];
        snprintf(polar_fullpath, sizeof(polar_fullpath), "%s/%s", argv[2], polar_filename);

        FILE *polar_fp = fopen(polar_fullpath, "r");
        if (polar_fp == NULL)
        {
            perror("Cannot open Polar file");
            fclose(bangle_fp);
            continue;
        }

        // Reset algorithms
        for (int i = 0; i < algoN; i++)
        {
            algos[i].bpm = 0;
            algos[i].init();
        }

        // Read Bangle data and process PPG
        unsigned int bangle_lineN = 0;
        unsigned int previous_ms = 0;

        while (fgets(line, sizeof(line), bangle_fp) != NULL)
        {
            if (bangle_lineN++ == 0)
                continue; // Skip header

            unsigned long timestamp_bangle;
            int bpm_bangle, ppg_raw;

            if (sscanf(line, "%lu,%d,%d", &timestamp_bangle, &bpm_bangle, &ppg_raw) != 3)
            {
                printf("Error parsing Bangle data: %s\n", line);
                continue;
            }

            // Find the closest Polar BPM based on timestamp
            unsigned long timestamp_polar;
            int bpm_polar, best_bpm_polar = -1;
            unsigned long closest_diff = -1;

            rewind(polar_fp);
            while (fgets(ref_line, sizeof(ref_line), polar_fp) != NULL)
            {
                if (sscanf(ref_line, "%lu,%d", &timestamp_polar, &bpm_polar) != 2)
                {
                    printf("Error parsing Polar data: %s\n", ref_line);
                    continue;
                }

                unsigned long diff = labs(timestamp_bangle - timestamp_polar);
                if (diff < closest_diff)
                {
                    closest_diff = diff;
                    best_bpm_polar = bpm_polar;
                }
            }

            if (best_bpm_polar == -1)
            {
                printf("No matching reference for %s\n", bangle_entry->d_name);
                continue;
            }

            int delta_ms = timestamp_bangle - previous_ms;

            // Process PPG raw data through all algorithms
            for (int i = 0; i < algoN; i++)
            {
                algos[i].bpm = algos[i].bpm_calculate(delta_ms, ppg_raw);
            }

            previous_ms = timestamp_bangle;
        }

        // Compare and calculate errors
        for (int i = 0; i < algoN; i++)
        {
            int error = abs(algos[i].bpm - best_bpm_polar);
            rolling_stats_addValue(algos[i].stats, error);
        }

        fclose(bangle_fp);
        fclose(polar_fp);
    }

    // Write final stats
    for (int i = 0; i < algoN; i++)
    {
        printf("%s: Mean error = %f, StdDev = %f\n", algos[i].name, algos[i].stats->mean, algos[i].stats->stddev);
    }

    fclose(out_fp);
    fclose(ref_fp);
    closedir(bangle_dir);
    closedir(polar_dir);

    return 0;
}
