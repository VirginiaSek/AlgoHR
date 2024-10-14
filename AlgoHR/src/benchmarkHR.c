#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "./dummy/dummyhr.h"
#include "./types.h"
#include "./rolling_stats.h"

typedef struct Algo
{
    char *name;
    Stats *stats;
    void (*init)();
    int (*heart_rate)(time_delta_ms_t delta_ms, int ppg_signal); // Function to compute heart rate (BPM)
    int bpm; // To store calculated heart rate (BPM)
} Algo;

// Structure to hold reference heart rate values along with timestamps
typedef struct {
    unsigned int ms; // Timestamp in milliseconds
    int heart_rate;  // Reference heart rate at this timestamp
} ReferenceHR;

////////////////////////////////////
// MODIFY HERE TO ADD HEART RATE ALGORITHMS

const int algoN = 1; // Number of heart rate algorithms
Algo algos[1];

// Function to create heart rate algorithms
void createAlgos()
{
    algos[0] = (Algo){
        .name = "DummyHR",
        .stats = malloc(sizeof(Stats)),
        .init = dummy_heart_rate_init,
        .heart_rate = dummy_heart_rate,
        .bpm = 0,
    };
}

////////////////////////////////////

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: %s <directory> <referenceshr_directory> <results>\n", argv[0]);
        return 1;
    }

    DIR *dir;
    struct dirent *entry;

    // Open the input directory with PPG data files
    dir = opendir(argv[1]);
    if (dir == NULL)
    {
        perror("Cannot open directory");
        return 1;
    }

    // Create output file
    FILE *out_fp;
    out_fp = fopen(argv[3], "w");
    if (out_fp == NULL)
    {
        perror("Error creating results file\n");
        return 1;
    }

    createAlgos();

    // Write the header of the output file
    fprintf(out_fp, "FILENAME,Reference,");
    for (int i = 0; i < algoN; i++)
    {
        fprintf(out_fp, "%s", algos[i].name);
        if (i < algoN - 1)
            fprintf(out_fp, ",");
    }
    fprintf(out_fp, "\n");

    // Initialize all stats for each algorithm
    for (int i = 0; i < algoN; i++)
    {
        rolling_stats_reset(algos[i].stats);
    }

    // Read each file in the directory containing PPG data
    while ((entry = readdir(dir)) != NULL)
    {
        if (strstr(entry->d_name, ".csv") == 0)
        {
            // Skip non-CSV files
            continue;
        }

        // Add path to the PPG filename
        char filename[PATH_MAX];
        snprintf(filename, sizeof(filename), "%s/%s", argv[1], entry->d_name);

        // Open the PPG input file
        FILE *ppg_fp = fopen(filename, "r");
        if (ppg_fp == NULL)
        {
            perror("Cannot open file");
            printf("%s", filename);
            continue;
        }

        // Prepare to read the reference file corresponding to the current PPG file
        char ref_filename[PATH_MAX];
        snprintf(ref_filename, sizeof(ref_filename), "%s/%s", argv[2], entry->d_name); // Assuming the reference file has the same name as the PPG file

        FILE *ref_fp = fopen(ref_filename, "r");
        if (ref_fp == NULL)
        {
            perror("Cannot open reference file");
            printf("%s", ref_filename);
            fclose(ppg_fp);
            continue;
        }

        // Parse the reference heart rate data and store it
        ReferenceHR *reference_hrs = NULL;
        size_t reference_count = 0;
        char ref_line[1024];

        while (fgets(ref_line, sizeof(ref_line), ref_fp) != NULL)
        {
            // Remove trailing newline
            ref_line[strcspn(ref_line, "\n")] = 0;

            unsigned int timestamp;
            int heart_rate;

            // Parse the reference heart rate and its corresponding timestamp
            if (sscanf(ref_line, "%u,%d", &timestamp, &heart_rate) == 2)
            {
                reference_hrs = realloc(reference_hrs, sizeof(ReferenceHR) * (reference_count + 1));
                reference_hrs[reference_count].ms = timestamp;
                reference_hrs[reference_count].heart_rate = heart_rate;
                reference_count++;
            }
        }
        fclose(ref_fp); // Close the reference file after reading

        // Initialize algorithms for each PPG file
        for (int i = 0; i < algoN; i++)
        {
            algos[i].bpm = 0;
            algos[i].init();
        }

        // Variables for processing PPG data
        unsigned int lineN = 0;
        unsigned int previous_ms = 0;

        // Read each line of the PPG input file
        char line[1024];
        while (fgets(line, sizeof(line), ppg_fp) != NULL)
        {
            lineN++;
            // Skip the first line (header)
            if (lineN > 1)
            {
                // Remove trailing newline
                line[strcspn(line, "\n")] = 0;

                int ms, raw, filt, bpm, confidence;

                // Parse the PPG signal values
                if (sscanf(line, "{\"ms\":%d,\"raw\":%d,\"filt\":%d,\"bpm\":%d,\"confidence\":%d}", &ms, &raw, &filt, &bpm, &confidence) != 5)
                {
                    printf("Error parsing line: %s\n", line);
                    continue;
                }

                int delta_ms = 0;
                if (lineN > 2)
                    delta_ms = ms - previous_ms;

                // Call all algorithms to compute heart rate
                for (int i = 0; i < algoN; i++)
                {
                    algos[i].bpm = algos[i].heart_rate(delta_ms, raw); // Pass raw PPG signal
                }

                // Find corresponding reference heart rate for this timestamp
                int ref_heart_rate = -1;
                for (size_t i = 0; i < reference_count; i++)
                {
                    if (reference_hrs[i].ms >= ms) // Find the closest matching reference timestamp
                    {
                        ref_heart_rate = reference_hrs[i].heart_rate;
                        break;
                    }
                }

                if (ref_heart_rate == -1)
                {
                    printf("No matching reference heart rate found for timestamp %d\n", ms);
                    continue;
                }

                // Write the results to the output file
                fprintf(out_fp, "%s,%d,", entry->d_name, ref_heart_rate);
                for (int i = 0; i < algoN; i++)
                {
                    fprintf(out_fp, "%d", algos[i].bpm);

                    if (i < algoN - 1)
                        fprintf(out_fp, ",");
                }
                fprintf(out_fp, "\n");

                // Add the error to the stats for each algorithm
                for (int i = 0; i < algoN; i++)
                {
                    double error = (double)algos[i].bpm - (double)ref_heart_rate;
                    rolling_stats_addValue(error, algos[i].stats); // Track error for this algorithm
                }

                previous_ms = ms;
            }
        }
        fclose(ppg_fp); // Close the PPG file

        // Free the reference heart rate memory
        free(reference_hrs);
    }

    closedir(dir); // Close the directory

    fclose(out_fp); // Close the output file

    // Print final statistics for each algorithm
    for (int i = 0; i < algoN; i++)
    {
        printf("%s Mean Error: %.1f, Std Dev: %.1f\n",
               algos[i].name,
               rolling_stats_get_mean(algos[i].stats),
               rolling_stats_get_standard_deviation(algos[i].stats, 0));
    }

    return 0;
}

