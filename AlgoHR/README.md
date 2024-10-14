Algorithms for BangleJS
=======================

Algorithms for step counting and heart rate for the Bangle JS.

Compile:

```bash
make
```

Run:

```bash
./build/benchmarkSC ../tests/ references.csv results.csv
```

Where `../tests` is the folder where the acceleration csv files are located, `references.csv` is the file with the reference step count and `results.csv` is the filename of the output file. Change accordingly.

Acceleration files are expected to he in a flat folder (no subfolders), with the following structure:
`ms, accx, accy, accz`

The reference file has the following structure:
`subject number, testname (same as the acceleration, minus csv), step count from Bangle, step count reference, activity`

## Add a new algorithm

- Create a folder within src.
- Add 2 functions: `void myalgo_init()` and `int myalgo_step_count(int delta_ms, int accx, int accy, int accz)`.
- In the benchmarkSC.c file, add +1 to algoN
- Add a new element inside the `createAlgos()` function, such as:
```c
    algos[N] = (Algo){
        .name = "MYALGO",
        .stats = malloc(sizeof(Stats)),
        .init = myalgo_init,
        .step_count = myalgo_stepcount,
        .counter = 0,
    };
```