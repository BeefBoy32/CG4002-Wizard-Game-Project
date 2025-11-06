#ifndef CNN_MODEL_H
#define CNN_MODEL_H

#include <cstdint>
#include "hls_stream.h"
#include <ap_axi_sdata.h>

typedef ap_axiu<32,0,0,0, AXIS_ENABLE_DATA|AXIS_ENABLE_LAST, true> AXIS_wLAST;

// --- Model Parameters ---
#define INPUT_LEN       60      // Time steps (window length)
#define INPUT_CH        6       // Number of input channels
#define KSIZE1          3
#define KSIZE2          3       
#define POOL_SIZE       2
#define CONV1_OUT       16
#define CONV2_OUT       24
#define DENSE1_OUT      24
#define NUM_CLASSES     7
#define FLAT_LEN CONV2_OUT

// --- Core CNN inference ---
void cnn_inference(const float input_raw[INPUT_LEN][INPUT_CH],
                   float output[NUM_CLASSES]);

// Top-level HLS function 
void cnn_gd(
    hls::stream<AXIS_wLAST> &input_stream,
    hls::stream<AXIS_wLAST> &output_stream
);

#endif // CNN_MODEL_H
