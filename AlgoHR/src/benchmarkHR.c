#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h> // Per gestire eventuali errori
 
#ifdef _WIN32
    #include <direct.h> // Per _mkdir su Windows
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #define MKDIR(path) mkdir(path, 0755) // Usa mkdir su UNIX-like, con i permessi appropriati
#endif
 
 
#include "./dummy/dummyHeartRate.h"
#include "./types.h"
#include "./rolling_stats.h"
 
typedef struct Algo {
    char *name;
    Stats *stats;
    void (*init)();
    bpm_t (*hr_process)(time_delta_ms_t delta_ms, ppg_t ppg_raw);
    bpm_t bpm;
} Algo;
 
const int algoN = 1; // Numero di algoritmi
Algo *algos;
 
void createAlgos() {
    algos = malloc(algoN * sizeof(Algo));
    algos[0] = (Algo){
        .name = "Dummy",
        .stats = malloc(sizeof(Stats)),
        .init = dummy_heartrate_init,
        .hr_process = dummy_heartrate,
        .bpm = 0,
    };
/*
    algos[1] = (Algo){
        .name = "Banglestepcount",
        .stats = malloc(sizeof(Stats)),
        .init = banglestepcount_init,
        .hr_process = banglestepcount,
        .bpm = 0,
    };*/
}
 
// Funzione per creare la directory dei risultati per ogni algoritmo
void create_result_directory(const char *algorithm_name) {
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), "Results_%s", algorithm_name);
 
    struct stat st = {0};
    if (stat(directory_path, &st) == -1) {
        if (MKDIR(directory_path) != 0 && errno != EEXIST) {
            perror("Cannot create results directory");
        }
    }
}
 
int find_closest_bpm(FILE *polar_fp, uint64_t timestamp) {
    char polar_line[1024];
    uint64_t polar_timestamp;
    int polar_bpm;
    uint64_t closest_diff = UINT_MAX;
    int closest_bpm = -1;
 
    rewind(polar_fp);
 
    while (fgets(polar_line, sizeof(polar_line), polar_fp)) {
        if (sscanf(polar_line, "%llu,%d", &polar_timestamp, &polar_bpm) == 2) {
            uint64_t diff = abs((int64_t)(timestamp - polar_timestamp));
            if (diff < closest_diff) {
                closest_diff = diff;
                closest_bpm = polar_bpm;
            }
            if (polar_timestamp > timestamp) {
                break;
            }
        }
    }
    return closest_bpm;
}
 
int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <bpm_smartwatch_dir> <bpm_polar_dir> <file_mapping> <results_dir>\n", argv[0]);
        return 1;
    }
 
    DIR *smartwatch_dir = opendir(argv[1]);
    if (smartwatch_dir == NULL) {
        perror("Cannot open smartwatch directory");
        return 1;
    }
 
    DIR *polar_dir = opendir(argv[2]);
    if (polar_dir == NULL) {
        perror("Cannot open polar directory");
        closedir(smartwatch_dir);
        return 1;
    }
 
    FILE *mapping_fp = fopen(argv[3], "r");
    if (mapping_fp == NULL) {
        perror("Cannot open file mapping");
        closedir(smartwatch_dir);
        closedir(polar_dir);
        return 1;
    }
 
    createAlgos();
 
    // Creiamo una directory per ciascun algoritmo
    for (int i = 0; i < algoN; i++) {
        create_result_directory(algos[i].name);
    }
 
    char mapping_line[1024];
    fgets(mapping_line, sizeof(mapping_line), mapping_fp);
 
    struct dirent *sw_entry;
 
    while ((sw_entry = readdir(smartwatch_dir)) != NULL) {
        if (strstr(sw_entry->d_name, ".csv") == 0) {
            continue;
        }
 
        char polar_filename[256] = {0};
        int found = 0;
 
        rewind(mapping_fp);
        while (fgets(mapping_line, sizeof(mapping_line), mapping_fp) != NULL) {
            char bangle_file[256] = {0}, polar_file[256] = {0};
 
            int parsed_fields = sscanf(mapping_line, "%*[^;];%*[^;];%[^;];%[^;\n]", bangle_file, polar_file);
 
            if (parsed_fields == 2) {
                if (strcasecmp(bangle_file, sw_entry->d_name) == 0) {
                    // Rimuovi eventuali estensioni ".csv" da polar_file
                    char *dot = strrchr(polar_file, '.');
                    if (dot && strcmp(dot, ".csv") == 0) {
                        *dot = '\0';
                    }
 
                    strcpy(polar_filename, polar_file);
                    found = 1;
                    break;
                }
            }
        }
 
        if (!found) {
            printf("Cannot find reference file for %s\n", sw_entry->d_name);
            continue;
        }
 
        char sw_filepath[512];
        snprintf(sw_filepath, sizeof(sw_filepath), "%s/%s", argv[1], sw_entry->d_name);
 
        char polar_filepath[512];
        snprintf(polar_filepath, sizeof(polar_filepath), "%s/%s.csv", argv[2], polar_filename);
 
        FILE *sw_fp = fopen(sw_filepath, "r");
        if (sw_fp == NULL) {
            perror("Cannot open smartwatch file");
            continue;
        }
 
        FILE *polar_fp = fopen(polar_filepath, "r");
        if (polar_fp == NULL) {
            perror("Cannot open polar file");
            fclose(sw_fp);
            continue;
        }
 
        // Per ciascun algoritmo, processa i dati e salva i risultati
        for (int i = 0; i < algoN; i++) {
            char results_filepath[1024];
            snprintf(results_filepath, sizeof(results_filepath), "Results_%s/%.100s_vs_%.100s.csv", algos[i].name, sw_entry->d_name, polar_filename);
 
            printf("Trying to create output file: %s\n", results_filepath);
 
            FILE *output_fp = fopen(results_filepath, "w");
            if (output_fp == NULL) {
                perror("Cannot open output file");
                printf("Failed to create file: %s\n", results_filepath);
                continue;
            }
 
            fprintf(output_fp, "timestamp,bpm_bangle,bpm_polar\n");
 
            char sw_line[1024];
            uint64_t sw_timestamp;
            int sw_ppg;
 
            fgets(sw_line, sizeof(sw_line), sw_fp);
 
            while (fgets(sw_line, sizeof(sw_line), sw_fp) != NULL) {
                if (sscanf(sw_line, "%llu,%d", &sw_timestamp, &sw_ppg) == 2) {
                    algos[i].bpm = algos[i].hr_process(0, sw_ppg);
                    int polar_bpm = find_closest_bpm(polar_fp, sw_timestamp);
                    fprintf(output_fp, "%llu,%d,%d\n", sw_timestamp, algos[i].bpm, polar_bpm);
                }
            }
 
            fclose(output_fp);
            printf("Successfully created file: %s\n", results_filepath);
        }
 
        fclose(sw_fp);
        fclose(polar_fp);
    }
 
    closedir(smartwatch_dir);
    closedir(polar_dir);
    fclose(mapping_fp);
 
    return 0;
}