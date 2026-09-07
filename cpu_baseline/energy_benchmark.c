#define _POSIX_C_SOURCE 200809L

#include "nn_inference.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


static volatile float benchmark_sink = 0.0f;


static double timestamp_seconds(clockid_t clock_id) {
    struct timespec value;
    if (clock_gettime(clock_id, &value) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)value.tv_sec + (double)value.tv_nsec * 1.0e-9;
}


static void *read_exact_file(const char *path, size_t expected_size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    void *buffer = malloc(expected_size);
    if (buffer == NULL) {
        fprintf(stderr, "Cannot allocate %zu bytes for %s\n", expected_size, path);
        exit(EXIT_FAILURE);
    }
    size_t bytes_read = fread(buffer, 1, expected_size, file);
    int extra_byte = fgetc(file);
    fclose(file);
    if (bytes_read != expected_size || extra_byte != EOF) {
        fprintf(stderr, "%s has an unexpected size (expected %zu bytes)\n", path, expected_size);
        exit(EXIT_FAILURE);
    }
    return buffer;
}


static void usage(const char *program) {
    fprintf(stderr, "Usage: %s X_test_f32.bin [minimum_seconds]\n", program);
}


int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t sample_count = 166000;
    const double minimum_seconds = argc == 3 ? strtod(argv[2], NULL) : 20.0;
    if (!(minimum_seconds > 0.0)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t input_bytes = sample_count * NN_INPUT_SIZE * sizeof(float);
    const float *inputs = read_exact_file(argv[1], input_bytes);
    float output[NN_OUTPUT_SIZE];

    /* Warm caches before the long, steady-state interval. */
    for (size_t sample = 0; sample < sample_count; ++sample) {
        nn_predict(inputs + sample * NN_INPUT_SIZE, output);
        benchmark_sink += output[sample % NN_OUTPUT_SIZE];
    }

    size_t inferences = 0;
    const double wall_start = timestamp_seconds(CLOCK_MONOTONIC_RAW);
    const double cpu_start = timestamp_seconds(CLOCK_PROCESS_CPUTIME_ID);
    double elapsed = 0.0;
    do {
        for (size_t sample = 0; sample < sample_count; ++sample) {
            nn_predict(inputs + sample * NN_INPUT_SIZE, output);
            benchmark_sink += output[sample % NN_OUTPUT_SIZE];
        }
        inferences += sample_count;
        elapsed = timestamp_seconds(CLOCK_MONOTONIC_RAW) - wall_start;
    } while (elapsed < minimum_seconds);
    const double cpu_elapsed = timestamp_seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;

    printf("implementation=portable_sparse_float32_qkeras6\n");
    printf("measurement=steady_state_energy_workload\n");
    printf("inferences=%zu\n", inferences);
    printf("wall_seconds=%.9f\n", elapsed);
    printf("inferences_per_second=%.6f\n", (double)inferences / elapsed);
    printf("process_cpu_seconds=%.9f\n", cpu_elapsed);
    printf("one_core_utilization_percent=%.6f\n", cpu_elapsed / elapsed * 100.0);
    printf("benchmark_sink=%.9f\n", benchmark_sink);

    free((void *)inputs);
    return EXIT_SUCCESS;
}
