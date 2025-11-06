#include "cnn_weights.h"
#include "test_data.h"
#include "cnn_model.h"
#include <iostream>
#include <iomanip>
#include "hls_stream.h"

// Change this to test with fewer samples (1 or 2 for quick testing)
#ifndef TEST_NUM_SAMPLES
#define TEST_NUM_SAMPLES 1
#endif

void print_array(const char* name, const float* arr, int size){
    std::cout << name << ": [";
    for(int i=0;i<size;i++){
        std::cout << std::fixed << std::setprecision(6) << arr[i];
        if(i<size-1) std::cout << ", ";
    }
    std::cout << "]\n";
}

int argmax_float(const float* data, int size){
    float max_val = data[0];
    int max_idx = 0;
    for(int i=1;i<size;i++){
        if(data[i] > max_val){
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx;
}

// --- Wrapper: Calls cnn_gd() with only input/output streams ---
void cnn_gd_wrapper(const float input_data[INPUT_LEN][INPUT_CH],
                    float output_data[NUM_CLASSES])
{
    hls::stream<AXIS_wLAST> input_stream("input_stream");
    hls::stream<AXIS_wLAST> output_stream("output_stream");

    // --- Write input stream ---
    for(int t=0;t<INPUT_LEN;t++){
        for(int c=0;c<INPUT_CH;c++){  
#pragma HLS PIPELINE II=1  
            AXIS_wLAST pkt;
            union { float f; uint32_t i; } conv;
            conv.f = input_data[t][c];
            pkt.data = conv.i;
            pkt.last = (t==INPUT_LEN-1 && c==INPUT_CH-1);
            input_stream.write(pkt);
        }
    }

    // --- Call HLS top-level IP  ---
    cnn_gd(input_stream, output_stream);

    // --- Read final output stream (probabilities) ---
    for(int i=0;i<NUM_CLASSES;i++){
#pragma HLS PIPELINE II=1
        AXIS_wLAST pkt = output_stream.read();
        union { float f; uint32_t i; } conv;
        conv.i = pkt.data;
        output_data[i] = conv.f; 
    }
}

// --- Main testbench ---
int main(){
    std::cout << "=== CNN Gesture Detection Test ===" << std::endl;
    std::cout << "Testing with " << TEST_NUM_SAMPLES << " sample(s)" << std::endl;
    std::cout.flush();

    const char* class_names[NUM_CLASSES] = {
        "Circle", "Infinity", "None", "Square", "Triangle", "Wave", "Zigzag"
    };

    float output[NUM_CLASSES];
    int correct_predictions = 0;

    int num_test_samples = (TEST_NUM_SAMPLES < NUM_SAMPLES) ? TEST_NUM_SAMPLES : NUM_SAMPLES;
    for(int sample=0; sample<num_test_samples; sample++){
        std::cout << "--- Test Sample " << sample+1 << " ---" << std::endl;
        std::cout.flush();

        cnn_gd_wrapper(test_inputs[sample], output);

        print_array("Output probabilities", output, NUM_CLASSES);
        print_array("Expected", test_outputs[sample], NUM_CLASSES);

        int pred_class = argmax_float(output, NUM_CLASSES);
        int exp_class = argmax_float(test_outputs[sample], NUM_CLASSES);

        std::cout << "Predicted: " << class_names[pred_class]
                  << " (class " << pred_class << ")\n";
        std::cout << "Expected:  " << class_names[exp_class]
                  << " (class " << exp_class << ")\n";

        if(pred_class == exp_class){
            std::cout << "✓ CORRECT\n";
            correct_predictions++;
        } else {
            std::cout << "✗ INCORRECT\n";
        }
        std::cout << "\n";
    }

    std::cout << "=== Results ===\n";
    std::cout << "Accuracy: " << correct_predictions << "/" << num_test_samples
              << " (" << (100.0*correct_predictions/num_test_samples) << "%)\n";

    return 0;
}
