#include <cstdint>
#include <cmath>
#include "cnn_weights.h"
#include "cnn_model.h"

void relu_layer(int16_t* data, int size) {
    for (int i = 0; i < size; i++) {
        if (data[i] < 0) data[i] = 0;
    }
}

void softmax_layer(int16_t* data, int size) {
    float temp[NUM_CLASSES];
#pragma HLS ARRAY_PARTITION variable=temp complete
    float maxval = -1e9f;
    for (int i = 0; i < size; i++) {
        temp[i] = data[i] / (float)(1 << Q8_8_SHIFT);
        if (temp[i] > maxval) maxval = temp[i];
    }

    for (int i = 0; i < size; i++) {
        float x = temp[i] - maxval;
        if (x > 8.0f) x = 8.0f;
        if (x < -8.0f) x = -8.0f;
        temp[i] = expf(x);
    }

    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
#pragma HLS UNROLL
        sum += temp[i];
    }

    for (int i = 0; i < size; i++) {
        float prob = temp[i] / sum;
        data[i] = (int16_t)(prob * (1 << Q8_8_SHIFT));
    } 
}

template<int IN_LEN, int IN_CH, int OUT_CH, int KSIZE>
void conv1d_layer(
    const int16_t input[IN_LEN][IN_CH],
    const int16_t weights[OUT_CH][IN_CH][KSIZE],
    const int32_t bias[OUT_CH],
    int16_t output[IN_LEN][OUT_CH]
) {
#pragma HLS INLINE off
    for (int t = 0; t < IN_LEN; t++) {
        for (int oc = 0; oc < OUT_CH; oc++) {
            int32_t acc = bias[oc];
            for (int ic = 0; ic < IN_CH; ic++) {
                for (int k = 0; k < KSIZE; k++) {
                    int idx = t + k - KSIZE/2;
                    if (idx >= 0 && idx < IN_LEN) {
                        acc += (int32_t)input[idx][ic] * (int32_t)weights[oc][ic][k];
                    }
                }
            }
            acc >>= Q8_8_SHIFT;
            if (acc > 32767) acc = 32767;
            if (acc < -32768) acc = -32768;
            output[t][oc] = (int16_t)acc;
        }
    }
}

template<int IN_LEN, int IN_CH, int POOL>
void maxpool1d_layer(
    const int16_t input[IN_LEN][IN_CH],
    int16_t output[IN_LEN/POOL][IN_CH]
) {
#pragma HLS INLINE off
    for (int oc = 0; oc < IN_CH; oc++) {
        for (int t = 0; t < IN_LEN; t += POOL) {
            int16_t maxval = input[t][oc];
            for (int k = 1; k < POOL; k++) {
                if (t + k < IN_LEN && input[t + k][oc] > maxval) {
                    maxval = input[t + k][oc];
                }
            }
            output[t / POOL][oc] = maxval;
        }
    }
}

void dense_layer(const int16_t* input,
                 const int16_t* weights,
                 const int32_t* bias,
                 int16_t* output,
                 int in_dim, int out_dim) {
    for (int o = 0; o < out_dim; o++) {
        int32_t acc = bias[o];
        for (int i = 0; i < in_dim; i++) {
            acc += (int32_t)input[i] * (int32_t)weights[i * out_dim + o];
        }
        acc >>= Q8_8_SHIFT; // convert Q16.16 -> Q8.8
        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        output[o] = (int16_t)acc;
    }
}

void cnn_inference(const int16_t input_raw[INPUT_LEN][INPUT_CH], int16_t output[NUM_CLASSES]) {
    int16_t conv1_out[INPUT_LEN][CONV1_OUT];
    int16_t pool1_out[INPUT_LEN/POOL_SIZE][CONV1_OUT];

    int16_t conv2_out[INPUT_LEN/POOL_SIZE][CONV2_OUT];
    int16_t pool2_out[INPUT_LEN/(POOL_SIZE*POOL_SIZE)][CONV2_OUT];

    int16_t flat[FLAT_LEN];
    int16_t dense1_out[DENSE1_OUT];
    int16_t dense2_out[NUM_CLASSES];

    // --- Conv1 ---
    conv1d_layer<INPUT_LEN, INPUT_CH, CONV1_OUT, KSIZE1>(
        input_raw,
        (const int16_t (*)[INPUT_CH][KSIZE1])conv1d_11_param0,
        conv1d_11_param1,
        conv1_out
    );
    relu_layer(&conv1_out[0][0], INPUT_LEN * CONV1_OUT);
    maxpool1d_layer<INPUT_LEN, CONV1_OUT, POOL_SIZE>(conv1_out, pool1_out);

    // --- Conv2 ---
    conv1d_layer<INPUT_LEN/POOL_SIZE, CONV1_OUT, CONV2_OUT, KSIZE2>(
        pool1_out,
        (const int16_t (*)[CONV1_OUT][KSIZE2])conv1d_12_param0,
        conv1d_12_param1,
        conv2_out
    );
    relu_layer(&conv2_out[0][0], (INPUT_LEN/POOL_SIZE) * CONV2_OUT);
    maxpool1d_layer<INPUT_LEN/POOL_SIZE, CONV2_OUT, POOL_SIZE>(conv2_out, pool2_out);

    // --- Flatten ---
    for (int t = 0; t < INPUT_LEN/(POOL_SIZE*POOL_SIZE); t++) {
        for (int c = 0; c < CONV2_OUT; c++) {
            flat[t*CONV2_OUT + c] = pool2_out[t][c];
        }
    }

    // --- Dense1 ---
    dense_layer(flat, dense_8_param0, dense_8_param1, dense1_out, FLAT_LEN, DENSE1_OUT);
    relu_layer(dense1_out, DENSE1_OUT);

    // --- Dense2 ---
    dense_layer(dense1_out, dense_9_param0, dense_9_param1, dense2_out, DENSE1_OUT, NUM_CLASSES);
    softmax_layer(dense2_out, NUM_CLASSES);

    for (int i = 0; i < NUM_CLASSES; i++) {
        output[i] = dense2_out[i];
    }
}

void cnn_inference_debug(const int16_t input_raw[INPUT_LEN][INPUT_CH], int16_t output[NUM_CLASSES]) {
    int16_t conv1_out[INPUT_LEN][CONV1_OUT];
    int16_t pool1_out[INPUT_LEN/POOL_SIZE][CONV1_OUT];

    int16_t conv2_out[INPUT_LEN/POOL_SIZE][CONV2_OUT];
    int16_t pool2_out[INPUT_LEN/(POOL_SIZE*POOL_SIZE)][CONV2_OUT];

    int16_t flat[FLAT_LEN];
    int16_t dense1_out[DENSE1_OUT];
    int16_t dense2_out[NUM_CLASSES];

    // --- Conv1 ---
    conv1d_layer<INPUT_LEN, INPUT_CH, CONV1_OUT, KSIZE1>(
        input_raw,
        (const int16_t (*)[INPUT_CH][KSIZE1])conv1d_11_param0,
        conv1d_11_param1,
        conv1_out
    );

    // Debug: print first 5 conv1 outputs
    std::cout << "--- Conv1 Output (first 5 timesteps) ---\n";
    for (int t = 0; t < 5; t++) {
        for (int c = 0; c < CONV1_OUT; c++)
            std::cout << conv1_out[t][c] << " ";
        std::cout << "\n";
    }

    relu_layer(&conv1_out[0][0], INPUT_LEN * CONV1_OUT);
    maxpool1d_layer<INPUT_LEN, CONV1_OUT, POOL_SIZE>(conv1_out, pool1_out);

    // --- Conv2 ---
    conv1d_layer<INPUT_LEN/POOL_SIZE, CONV1_OUT, CONV2_OUT, KSIZE2>(
        pool1_out,
        (const int16_t (*)[CONV1_OUT][KSIZE2])conv1d_12_param0,
        conv1d_12_param1,
        conv2_out
    );

    // Debug: print first 5 conv2 outputs
    std::cout << "--- Conv2 Output (first 5 timesteps) ---\n";
    for (int t = 0; t < 5; t++) {
        for (int c = 0; c < CONV2_OUT; c++)
            std::cout << conv2_out[t][c] << " ";
        std::cout << "\n";
    }

    relu_layer(&conv2_out[0][0], (INPUT_LEN/POOL_SIZE) * CONV2_OUT);
    maxpool1d_layer<INPUT_LEN/POOL_SIZE, CONV2_OUT, POOL_SIZE>(conv2_out, pool2_out);

    // --- Flatten ---
    for (int t = 0; t < INPUT_LEN/(POOL_SIZE*POOL_SIZE); t++) {
        for (int c = 0; c < CONV2_OUT; c++) {
            flat[t*CONV2_OUT + c] = pool2_out[t][c];
        }
    }

    // --- Dense1 ---
    dense_layer(flat, dense_8_param0, dense_8_param1, dense1_out, FLAT_LEN, DENSE1_OUT);

    // Debug: print first 10 dense1 outputs
    std::cout << "--- Dense1 Output (first 10) ---\n";
    for (int i = 0; i < 10; i++) std::cout << dense1_out[i] << " ";
    std::cout << "\n";

    relu_layer(dense1_out, DENSE1_OUT);

    // --- Dense2 ---
    dense_layer(dense1_out, dense_9_param0, dense_9_param1, dense2_out, DENSE1_OUT, NUM_CLASSES);

    // Debug: print dense2 outputs before softmax
    std::cout << "--- Dense2 Output (before softmax) ---\n";
    for (int i = 0; i < NUM_CLASSES; i++) std::cout << dense2_out[i] << " ";
    std::cout << "\n";

    softmax_layer(dense2_out, NUM_CLASSES);

    for (int i = 0; i < NUM_CLASSES; i++) {
        output[i] = dense2_out[i];
    }
}

void cnn_gd(hls::stream<axis_t> &input_stream, hls::stream<axis_t> &output_stream) {
#pragma HLS INTERFACE axis port=input_stream
#pragma HLS INTERFACE axis port=output_stream
#pragma HLS INTERFACE s_axilite port=return bundle=control

    int16_t input_q[INPUT_LEN][INPUT_CH];
    int16_t output_q[NUM_CLASSES];

read_loop:
    for (int t = 0; t < INPUT_LEN; t++) {
        for (int c = 0; c < INPUT_CH; c++) {
#pragma HLS PIPELINE II=1
            axis_t pkt = input_stream.read();
            input_q[t][c] = (int16_t)pkt.data;  // Direct read Q8.8
        }
    }

    cnn_inference_debug(input_q, output_q);

write_loop:
    for (int i = 0; i < NUM_CLASSES; i++) {
#pragma HLS PIPELINE II=1
        axis_t pkt;
        pkt.data = (int32_t)output_q[i];  // Q8.8 output
        pkt.keep = -1;
        pkt.last = (i == NUM_CLASSES - 1);
        output_stream.write(pkt);
    }
}