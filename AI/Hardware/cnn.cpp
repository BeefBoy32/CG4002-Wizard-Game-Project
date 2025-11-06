#include <cstdint>
#include <cmath>
#include "cnn_weights.h"
#include "cnn_model.h"
#include <hls_stream.h>
#include <hls_math.h>
#include <cstdio>

// --- ReLU in-place ---
void relu_inplace(float* data, int size) {
#pragma HLS INLINE off
    for(int i=0;i<size;i++){
#pragma HLS PIPELINE II=1
        if(data[i]<0.0f) data[i]=0.0f;
    }
}

// --- Conv1D ---
template<int IN_LEN,int IN_CH,int OUT_CH,int KSIZE>
void conv1d_layer(const float input[IN_LEN][IN_CH],
                  const float weights[OUT_CH*IN_CH*KSIZE],
                  const float bias[OUT_CH],
                  float output[IN_LEN][OUT_CH])
{
#pragma HLS INLINE off
    for(int t=0; t<IN_LEN; t++){
        for(int oc=0; oc<OUT_CH; oc++){
#pragma HLS PIPELINE II=1
            float acc = bias[oc];
            for(int ic=0; ic<IN_CH; ic++){
                for(int k=0; k<KSIZE; k++){
                    int idx = t + k - KSIZE/2;
                    if(idx>=0 && idx<IN_LEN)
                        acc += input[idx][ic]*weights[oc*IN_CH*KSIZE + ic*KSIZE + k];
                }
            }
            output[t][oc] = acc;
        }
    }
}

// --- MaxPool1D ---
template<int IN_LEN,int CH,int POOL>
void maxpool1d(const float input[IN_LEN][CH],
               float output[IN_LEN/POOL][CH])
{
#pragma HLS INLINE off
    for(int c=0; c<CH; c++){
        for(int t=0; t<IN_LEN; t+=POOL){
#pragma HLS PIPELINE II=1
            float max_val = input[t][c];
            for(int p=1; p<POOL; p++){
                if(t+p<IN_LEN && input[t+p][c]>max_val) max_val=input[t+p][c];
            }
            output[t/POOL][c] = max_val;
        }
    }
}

// --- Global Average Pooling ---
template<int IN_LEN,int CH>
void global_average_pool1d(const float input[IN_LEN][CH], float output[CH]){
#pragma HLS INLINE off
    for(int c=0; c<CH; c++){
        float acc = 0.0f;
        for(int t=0; t<IN_LEN; t++){
#pragma HLS PIPELINE II=1
            acc += input[t][c];
        }
        output[c] = acc / IN_LEN;
    }
}

// --- Dense Layer ---
template<int IN_LEN,int OUT_LEN>
void dense_layer(const float input[IN_LEN], const float weights[IN_LEN*OUT_LEN],
                 const float bias[OUT_LEN], float output[OUT_LEN])
{
#pragma HLS INLINE off
    for(int o=0; o<OUT_LEN; o++){
        float acc = bias[o];
        for(int i=0; i<IN_LEN; i++){
#pragma HLS PIPELINE II=1
            acc += input[i]*weights[i*OUT_LEN+o];
        }
        output[o] = acc;
    }
}

// --- Softmax ---
template<int SIZE>
void softmax(float output[SIZE])
{
#pragma HLS INLINE off
    float softmax_sum = 0.0f;
    for(int i=0; i<SIZE; i++){
#pragma HLS PIPELINE II=1
        softmax_sum += hls::expf(output[i]);
    }
    for(int i=0; i<SIZE; i++){
#pragma HLS PIPELINE II=1
        output[i] = hls::expf(output[i]) / softmax_sum;
    }
}

// --- CNN Inference ---
void cnn_inference(float input_raw[INPUT_LEN][INPUT_CH],
                   float output[NUM_CLASSES])
{
    float conv1_out[INPUT_LEN][CONV1_OUT];
    float pool1_out[INPUT_LEN/POOL_SIZE][CONV1_OUT];
    float conv2_out[INPUT_LEN/POOL_SIZE][CONV2_OUT];
    float pool2_out[INPUT_LEN/(POOL_SIZE*POOL_SIZE)][CONV2_OUT];  
    float gap_out[CONV2_OUT];                                    
    float dense1_out[DENSE1_OUT];

    // Conv1 + ReLU + MaxPool (BatchNorm folded into weights)
    conv1d_layer<INPUT_LEN, INPUT_CH, CONV1_OUT, KSIZE1>(input_raw, conv1d_5_param0, conv1d_5_param1, conv1_out);
    relu_inplace(&conv1_out[0][0], INPUT_LEN*CONV1_OUT);
    maxpool1d<INPUT_LEN, CONV1_OUT, POOL_SIZE>(conv1_out, pool1_out);

    // Conv2 + ReLU + MaxPool (BatchNorm folded into weights)
    conv1d_layer<INPUT_LEN/POOL_SIZE, CONV1_OUT, CONV2_OUT, KSIZE2>(pool1_out, conv1d_6_param0, conv1d_6_param1, conv2_out);
    relu_inplace(&conv2_out[0][0], (INPUT_LEN/POOL_SIZE)*CONV2_OUT);
    maxpool1d<INPUT_LEN/POOL_SIZE, CONV2_OUT, POOL_SIZE>(conv2_out, pool2_out);

    // Global Average Pooling
    global_average_pool1d<INPUT_LEN/(POOL_SIZE*POOL_SIZE), CONV2_OUT>(pool2_out, gap_out);

    // Dense + ReLU (first dense layer has no bias, use zeros)
    float dense1_bias[DENSE1_OUT] = {0.0f};  // Zero-initialize all elements
    dense_layer<CONV2_OUT, DENSE1_OUT>(gap_out, dense_4_param0, dense1_bias, dense1_out);
    relu_inplace(dense1_out, DENSE1_OUT);
    dense_layer<DENSE1_OUT, NUM_CLASSES>(dense1_out, dense_5_param0, dense_5_param1, output);

    // Softmax
    softmax<NUM_CLASSES>(output);
}

// --- Top-level streaming function ---
void cnn_gd(
    hls::stream<AXIS_wLAST> &input_stream,
    hls::stream<AXIS_wLAST> &output_stream
){
#pragma HLS INTERFACE axis port=input_stream 
#pragma HLS INTERFACE axis port=output_stream 
#pragma HLS INTERFACE s_axilite port=return bundle=control

    float input_buf[INPUT_LEN][INPUT_CH];
    float output_buf[NUM_CLASSES];

    // Read input stream
    read_input_loop: for(int t=0; t<INPUT_LEN; t++){
        read_channel_loop: for(int c=0; c<INPUT_CH; c++){
#pragma HLS PIPELINE II=1
            AXIS_wLAST pkt = input_stream.read();
            union { float f; uint32_t u; } conv;
            conv.u = (uint32_t)pkt.data;
            input_buf[t][c] = conv.f;
        }
    }

    cnn_inference(input_buf, output_buf);

    // Write output stream
    write_output_loop: for(int i=0; i<NUM_CLASSES; i++){
#pragma HLS PIPELINE II=1
        AXIS_wLAST pkt;
        union { float f; uint32_t u; } conv;
        conv.f = output_buf[i];
        pkt.data = conv.u;
        pkt.last = (i==NUM_CLASSES-1);
        output_stream.write(pkt);
    }
}
