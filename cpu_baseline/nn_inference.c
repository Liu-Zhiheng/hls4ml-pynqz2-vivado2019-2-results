#include "nn_inference.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>


static float quantized_relu_6(float value) {
    const float maximum = 63.0f / 64.0f;
    if (value <= 0.0f) {
        return 0.0f;
    }
    if (value >= maximum) {
        return maximum;
    }
    return nearbyintf(value * 64.0f) / 64.0f;
}


static void sparse_dense(
    const float *input,
    float *output,
    unsigned int output_size,
    const uint16_t *row_ptr,
    const uint8_t *column_indices,
    const float *values,
    const float *bias,
    int apply_quantized_relu
) {
    for (unsigned int output_index = 0; output_index < output_size; ++output_index) {
        float accumulator = bias[output_index];
        for (uint16_t index = row_ptr[output_index]; index < row_ptr[output_index + 1]; ++index) {
            accumulator += input[column_indices[index]] * values[index];
        }
        output[output_index] = apply_quantized_relu ? quantized_relu_6(accumulator) : accumulator;
    }
}


void nn_predict(const float input[NN_INPUT_SIZE], float output[NN_OUTPUT_SIZE]) {
    float layer_1[FC1_OUTPUT_SIZE];
    float layer_2[FC2_OUTPUT_SIZE];
    float layer_3[FC3_OUTPUT_SIZE];
    float logits[OUTPUT_OUTPUT_SIZE];

    sparse_dense(input, layer_1, FC1_OUTPUT_SIZE, fc1_row_ptr, fc1_column_indices, fc1_values, fc1_bias, 1);
    sparse_dense(layer_1, layer_2, FC2_OUTPUT_SIZE, fc2_row_ptr, fc2_column_indices, fc2_values, fc2_bias, 1);
    sparse_dense(layer_2, layer_3, FC3_OUTPUT_SIZE, fc3_row_ptr, fc3_column_indices, fc3_values, fc3_bias, 1);
    sparse_dense(layer_3, logits, OUTPUT_OUTPUT_SIZE, output_row_ptr, output_column_indices, output_values, output_bias, 0);

    float maximum = logits[0];
    for (unsigned int index = 1; index < NN_OUTPUT_SIZE; ++index) {
        if (logits[index] > maximum) {
            maximum = logits[index];
        }
    }

    float total = 0.0f;
    for (unsigned int index = 0; index < NN_OUTPUT_SIZE; ++index) {
        output[index] = expf(logits[index] - maximum);
        total += output[index];
    }
    for (unsigned int index = 0; index < NN_OUTPUT_SIZE; ++index) {
        output[index] /= total;
    }
}


unsigned int nn_argmax(const float output[NN_OUTPUT_SIZE]) {
    unsigned int result = 0;
    for (unsigned int index = 1; index < NN_OUTPUT_SIZE; ++index) {
        if (output[index] > output[result]) {
            result = index;
        }
    }
    return result;
}
