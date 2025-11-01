#include "cnn_weights.h"
#include "test_data.h"
#include "cnn_model.h"
#include <iostream>
#include <iomanip>
#include <cmath>

void cnn_inference_wrapper(const int16_t input_data[INPUT_LEN][INPUT_CH],
                           float output_data[NUM_CLASSES])
{
    hls::stream<axis_t> input_stream;
    hls::stream<axis_t> output_stream;

    // --- Stream input directly as int16_t (Q8.8) ---
    for (int t = 0; t < INPUT_LEN; t++) {
        for (int c = 0; c < INPUT_CH; c++) {
#pragma HLS PIPELINE II=1
            axis_t pkt;
            pkt.data = (int32_t)input_data[t][c];  // Q8.8 value
            pkt.keep = -1;
            pkt.last = (t == INPUT_LEN - 1 && c == INPUT_CH - 1);
            input_stream.write(pkt);
        }
    }

    // --- Run HLS CNN ---
    cnn_gd(input_stream, output_stream);

    // --- Convert output Q8.8 → float probabilities ---
    for (int i = 0; i < NUM_CLASSES; i++) {
#pragma HLS PIPELINE II=1
        axis_t pkt = output_stream.read();
        output_data[i] = (float)((int16_t)pkt.data) / 256.0f;  // Q8.8 → float
    }
}

void print_array(const char* name, const float* arr, int size) {
    std::cout << name << ": [";
    for (int i = 0; i < size; i++) {
        std::cout << std::fixed << std::setprecision(6) << arr[i];
        if (i < size - 1) std::cout << ", ";
    }
    std::cout << "]\n";
}

int argmax_float(const float* data, int size) {
    float max_val = data[0];
    int max_idx = 0;
    for (int i = 1; i < size; i++) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx;
}

int main() {
    std::cout << "=== CNN Gesture Detection Test ===\n";
    std::cout << "Model: " << INPUT_LEN << "×" << INPUT_CH
              << " -> Conv(" << CONV1_OUT << ") -> Dense(" << DENSE1_OUT
              << ") -> " << NUM_CLASSES << " classes\n\n";

    const char* class_names[NUM_CLASSES] = {
        "Circle", "Infinity", "None", "Square", "Triangle", "Wave", "Zigzag"
    };

    float output[NUM_CLASSES];
    int correct_predictions = 0;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        std::cout << "--- Test Sample " << (sample + 1) << " ---\n";

        cnn_inference_wrapper(test_inputs[sample], output);

        print_array("Output probabilities", output, NUM_CLASSES);

        float expected[NUM_CLASSES];
        for (int i = 0; i < NUM_CLASSES; i++) {
            expected[i] = test_outputs[sample][i] / 256.0f;
        }
        print_array("Expected", expected, NUM_CLASSES);

        int pred_class = argmax_float(output, NUM_CLASSES);
        int exp_class = argmax_float(expected, NUM_CLASSES);

        std::cout << "Predicted: " << class_names[pred_class]
                  << " (class " << pred_class << ")\n";
        std::cout << "Expected:  " << class_names[exp_class]
                  << " (class " << exp_class << ")\n";

        if (pred_class == exp_class) {
            std::cout << "CORRECT\n";
            correct_predictions++;
        } else {
            std::cout << "INCORRECT\n";
        }
        std::cout << "\n";
    }

    std::cout << "=== Results ===\n";
    std::cout << "Accuracy: " << correct_predictions << "/" << NUM_SAMPLES
              << " (" << (100.0 * correct_predictions / NUM_SAMPLES) << "%)\n";

    return 0;
}