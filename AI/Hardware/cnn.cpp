/*
 * CNN Inference in C++ (Fixed-point Q8.8)
 * Uses weights from cnn_weights.h (Python-exported)
 * Input: 60 timesteps × 6 channels
 * Output: 7 classes
 */

#include <cstdint>
#include <cmath>
#include "cnn_weights.h"

// -------------------- Model Parameters --------------------
#define INPUT_LEN    60    // timesteps per window
#define INPUT_CH     6     // input channels
#define KSIZE        5     // Conv1D kernel size (adjust to match your model)
#define POOL_SIZE    2     // MaxPool size
#define Q8_8_SHIFT   8     // Fixed-point scaling

#define CONV1_OUT    32    // Conv1D output filters
#define DENSE1_OUT   128   // Dense hidden layer
#define NUM_CLASSES  7     // output classes

#define FLAT_LEN ((INPUT_LEN/POOL_SIZE) * CONV1_OUT)

// -------------------- Utility Layers --------------------
void relu_layer(int16_t* data, int size) {
    for (int i = 0; i < size; i++) {
        if (data[i] < 0) data[i] = 0;
    }
}

void softmax_layer(int16_t* data, int size) {
    float temp[size];
    float maxval = -1e9f;

    for (int i = 0; i < size; i++) {
        temp[i] = data[i] / (float)(1 << Q8_8_SHIFT);
        if (temp[i] > maxval) maxval = temp[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        temp[i] = expf(temp[i] - maxval);
        sum += temp[i];
    }

    for (int i = 0; i < size; i++) {
        float prob = temp[i] / sum;
        data[i] = (int16_t)(prob * (1 << Q8_8_SHIFT));
    }
}

// Conv1D layer
void conv1d_layer(const int16_t input[INPUT_LEN][INPUT_CH],
                  const int16_t weights[CONV1_OUT][INPUT_CH][KSIZE],
                  const int16_t bias[CONV1_OUT],
                  int16_t output[INPUT_LEN][CONV1_OUT]) {
    for (int t = 0; t < INPUT_LEN; t++) {
        for (int oc = 0; oc < CONV1_OUT; oc++) {
            int32_t acc = (bias[oc] << Q8_8_SHIFT);
            for (int ic = 0; ic < INPUT_CH; ic++) {
                for (int k = 0; k < KSIZE; k++) {
                    int idx = t + k - KSIZE/2;
                    if (idx >= 0 && idx < INPUT_LEN) {
                        acc += (int32_t)input[idx][ic] *
                               weights[oc][ic][k];
                    }
                }
            }
            output[t][oc] = (int16_t)(acc >> Q8_8_SHIFT);
        }
    }
}

// MaxPool1D layer
void maxpool1d_layer(const int16_t input[INPUT_LEN][CONV1_OUT],
                     int16_t output[INPUT_LEN/POOL_SIZE][CONV1_OUT]) {
    for (int oc = 0; oc < CONV1_OUT; oc++) {
        for (int t = 0; t < INPUT_LEN; t += POOL_SIZE) {
            int16_t maxval = input[t][oc];
            for (int k = 1; k < POOL_SIZE; k++) {
                if (t + k < INPUT_LEN && input[t+k][oc] > maxval) {
                    maxval = input[t+k][oc];
                }
            }
            output[t/POOL_SIZE][oc] = maxval;
        }
    }
}

// Dense layer
void dense_layer(const int16_t* input,
                 const int16_t* weights,  // flattened [in_dim * out_dim]
                 const int16_t* bias,
                 int16_t* output,
                 int in_dim, int out_dim) {
    for (int o = 0; o < out_dim; o++) {
        int32_t acc = (bias[o] << Q8_8_SHIFT);
        for (int i = 0; i < in_dim; i++) {
            acc += (int32_t)input[i] * weights[i*out_dim + o];
        }
        output[o] = (int16_t)(acc >> Q8_8_SHIFT);
    }
}

// -------------------- CNN Inference --------------------
void cnn_inference(const int16_t input_raw[INPUT_LEN][INPUT_CH],
                   int16_t output[NUM_CLASSES]) {
    static int16_t conv1_out[INPUT_LEN][CONV1_OUT];
    static int16_t pool1_out[INPUT_LEN/POOL_SIZE][CONV1_OUT];
    static int16_t flat[FLAT_LEN];
    static int16_t dense1_out[DENSE1_OUT];
    static int16_t dense2_out[NUM_CLASSES];

    // Conv1 + ReLU
    conv1d_layer(input_raw,
                 (const int16_t (*)[INPUT_CH][KSIZE])conv1d_12_param0,
                 conv1d_12_param1,
                 conv1_out);
    relu_layer(&conv1_out[0][0], INPUT_LEN * CONV1_OUT);

    // MaxPool
    maxpool1d_layer(conv1_out, pool1_out);

    // Flatten
    for (int t = 0; t < INPUT_LEN/POOL_SIZE; t++) {
        for (int c = 0; c < CONV1_OUT; c++) {
            flat[t*CONV1_OUT + c] = pool1_out[t][c];
        }
    }

    // Dense1 + ReLU
    dense_layer(flat, dense_8_param0, dense_8_param1,
                dense1_out, FLAT_LEN, DENSE1_OUT);
    relu_layer(dense1_out, DENSE1_OUT);

    // Dense2 + Softmax
    dense_layer(dense1_out, dense_9_param0, dense_9_param1,
                dense2_out, DENSE1_OUT, NUM_CLASSES);
    softmax_layer(dense2_out, NUM_CLASSES);

    // Copy output
    for (int i = 0; i < NUM_CLASSES; i++) {
        output[i] = dense2_out[i];
    }
}
