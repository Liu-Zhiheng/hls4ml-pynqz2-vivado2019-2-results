#ifndef HLS4ML_CPU_BASELINE_NN_INFERENCE_H
#define HLS4ML_CPU_BASELINE_NN_INFERENCE_H

#include "generated/model_data.h"

void nn_predict(const float input[NN_INPUT_SIZE], float output[NN_OUTPUT_SIZE]);
unsigned int nn_argmax(const float output[NN_OUTPUT_SIZE]);

#endif
