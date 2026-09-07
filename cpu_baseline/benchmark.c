#define _POSIX_C_SOURCE 200809L

#include "nn_inference.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
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


static int compare_double(const void *left, const void *right) {
    double a = *(const double *)left;
    double b = *(const double *)right;
    return (a > b) - (a < b);
}


static double percentile(const double *sorted_values, size_t count, double fraction) {
    if (count == 0) {
        return NAN;
    }
    double position = fraction * (double)(count - 1);
    size_t lower = (size_t)position;
    size_t upper = lower + (lower + 1 < count);
    double weight = position - (double)lower;
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight;
}


static void usage(const char *program) {
    fprintf(
        stderr,
        "Usage: %s X_test_f32.bin y_test_labels_u8.bin qkeras_reference_scores_f32.bin "
        "[minimum_throughput_seconds] [latency_samples] [c_scores_output.bin]\n",
        program
    );
}


int main(int argc, char **argv) {
    if (argc < 4 || argc > 7) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const size_t sample_count = 166000;
    const double minimum_seconds = argc >= 5 ? strtod(argv[4], NULL) : 3.0;
    const size_t latency_sample_count = argc >= 6 ? (size_t)strtoull(argv[5], NULL, 10) : 10000;
    const char *score_output_path = argc >= 7 ? argv[6] : NULL;

    const size_t input_bytes = sample_count * NN_INPUT_SIZE * sizeof(float);
    const size_t label_bytes = sample_count * sizeof(uint8_t);
    const size_t score_bytes = sample_count * NN_OUTPUT_SIZE * sizeof(float);
    const float *inputs = read_exact_file(argv[1], input_bytes);
    const uint8_t *labels = read_exact_file(argv[2], label_bytes);
    const float *reference_scores = read_exact_file(argv[3], score_bytes);
    float *c_scores = malloc(score_bytes);
    if (c_scores == NULL) {
        fprintf(stderr, "Cannot allocate C score output\n");
        return EXIT_FAILURE;
    }

    size_t correct = 0;
    size_t reference_agreement = 0;
    double absolute_error_sum = 0.0;
    float maximum_absolute_error = 0.0f;
    for (size_t sample = 0; sample < sample_count; ++sample) {
        float *output = c_scores + sample * NN_OUTPUT_SIZE;
        const float *reference = reference_scores + sample * NN_OUTPUT_SIZE;
        nn_predict(inputs + sample * NN_INPUT_SIZE, output);
        unsigned int predicted = nn_argmax(output);
        unsigned int reference_predicted = nn_argmax(reference);
        correct += predicted == labels[sample];
        reference_agreement += predicted == reference_predicted;
        for (size_t index = 0; index < NN_OUTPUT_SIZE; ++index) {
            float error = fabsf(output[index] - reference[index]);
            absolute_error_sum += error;
            if (error > maximum_absolute_error) {
                maximum_absolute_error = error;
            }
        }
    }

    if (score_output_path != NULL) {
        FILE *score_output = fopen(score_output_path, "wb");
        if (score_output == NULL || fwrite(c_scores, 1, score_bytes, score_output) != score_bytes) {
            fprintf(stderr, "Cannot write %s\n", score_output_path);
            return EXIT_FAILURE;
        }
        fclose(score_output);
    }

    const size_t warmup_count = 10000;
    float temporary_output[NN_OUTPUT_SIZE];
    for (size_t iteration = 0; iteration < warmup_count; ++iteration) {
        size_t sample = iteration % sample_count;
        nn_predict(inputs + sample * NN_INPUT_SIZE, temporary_output);
        benchmark_sink += temporary_output[iteration % NN_OUTPUT_SIZE];
    }

    double timer_overhead_sum = 0.0;
    for (size_t iteration = 0; iteration < latency_sample_count; ++iteration) {
        double start = timestamp_seconds(CLOCK_MONOTONIC_RAW);
        double end = timestamp_seconds(CLOCK_MONOTONIC_RAW);
        timer_overhead_sum += end - start;
    }
    const double mean_timer_overhead = timer_overhead_sum / (double)latency_sample_count;

    double *latencies = malloc(latency_sample_count * sizeof(double));
    if (latencies == NULL) {
        fprintf(stderr, "Cannot allocate latency samples\n");
        return EXIT_FAILURE;
    }
    double latency_sum = 0.0;
    for (size_t iteration = 0; iteration < latency_sample_count; ++iteration) {
        size_t sample = iteration % sample_count;
        double start = timestamp_seconds(CLOCK_MONOTONIC_RAW);
        nn_predict(inputs + sample * NN_INPUT_SIZE, temporary_output);
        double end = timestamp_seconds(CLOCK_MONOTONIC_RAW);
        double latency = end - start;
        latencies[iteration] = latency;
        latency_sum += latency;
        benchmark_sink += temporary_output[iteration % NN_OUTPUT_SIZE];
    }
    qsort(latencies, latency_sample_count, sizeof(double), compare_double);

    size_t throughput_inferences = 0;
    double wall_start = timestamp_seconds(CLOCK_MONOTONIC_RAW);
    double cpu_start = timestamp_seconds(CLOCK_PROCESS_CPUTIME_ID);
    double elapsed = 0.0;
    do {
        for (size_t sample = 0; sample < sample_count; ++sample) {
            nn_predict(inputs + sample * NN_INPUT_SIZE, temporary_output);
            benchmark_sink += temporary_output[sample % NN_OUTPUT_SIZE];
        }
        throughput_inferences += sample_count;
        elapsed = timestamp_seconds(CLOCK_MONOTONIC_RAW) - wall_start;
    } while (elapsed < minimum_seconds);
    double cpu_elapsed = timestamp_seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_start;

    printf("implementation=portable_sparse_float32_qkeras6\n");
    printf("samples=%zu\n", sample_count);
    printf("accuracy=%.12f\n", (double)correct / (double)sample_count);
    printf("qkeras_class_agreement=%.12f\n", (double)reference_agreement / (double)sample_count);
    printf("qkeras_score_mean_absolute_error=%.12g\n", absolute_error_sum / (double)(sample_count * NN_OUTPUT_SIZE));
    printf("qkeras_score_maximum_absolute_error=%.12g\n", (double)maximum_absolute_error);
    printf("latency_samples=%zu\n", latency_sample_count);
    printf("timer_mean_overhead_us=%.9f\n", mean_timer_overhead * 1.0e6);
    printf("batch_one_mean_us=%.9f\n", latency_sum / (double)latency_sample_count * 1.0e6);
    printf("batch_one_median_us=%.9f\n", percentile(latencies, latency_sample_count, 0.50) * 1.0e6);
    printf("batch_one_p95_us=%.9f\n", percentile(latencies, latency_sample_count, 0.95) * 1.0e6);
    printf("batch_one_p99_us=%.9f\n", percentile(latencies, latency_sample_count, 0.99) * 1.0e6);
    printf("throughput_inferences=%zu\n", throughput_inferences);
    printf("throughput_wall_seconds=%.9f\n", elapsed);
    printf("throughput_inferences_per_second=%.6f\n", (double)throughput_inferences / elapsed);
    printf("throughput_process_cpu_seconds=%.9f\n", cpu_elapsed);
    printf("throughput_one_core_utilization_percent=%.6f\n", cpu_elapsed / elapsed * 100.0);
    printf("benchmark_sink=%.9f\n", benchmark_sink);

    free(latencies);
    free(c_scores);
    free((void *)reference_scores);
    free((void *)labels);
    free((void *)inputs);
    return EXIT_SUCCESS;
}
