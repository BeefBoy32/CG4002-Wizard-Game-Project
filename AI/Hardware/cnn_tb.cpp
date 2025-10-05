/*
 * Testbench for CNN Inference with multiple test samples
 * Uses data from test_data.h
 */

#include <iostream>
#include <cstdint>
#include "cnn_weights.h"
#include "test_data.h"

// Forward declare DUT
void cnn_inference(const int16_t input_raw[INPUT_LEN],
                   int16_t output[NUM_CLASSES]);

int main() {
    for (int n = 0; n < NUM_SAMPLES; n++) {
        int16_t output[NUM_CLASSES];
        cnn_inference(&test_inputs[n][0][0], output);

        std::cout << "==== Sample " << n << " ====\n";
        for (int i = 0; i < NUM_CLASSES; i++) {
            float prob_fpga = output[i] / 256.0f;
            float prob_ref  = test_outputs[n][i] / 256.0f;
            std::cout << "Class " << i
                      << " | FPGA=" << prob_fpga
                      << " | Expected=" << prob_ref << "\n";
        }
    }
    return 0;
}
