#include "main_functions.h"
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include "model.hpp"
#include <zephyr/kernel.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/system_setup.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/mem_manage.h>  // For memory info
#include <zephyr/sys/time_units.h>  // For precise timing
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/sys_heap.h>



constexpr int kTensorArenaSize = 10 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

struct sys_heap my_heap;
struct sys_memory_stats stats;

#define STACK_SIZE 10*1024
// Define thread stack and data
K_THREAD_STACK_DEFINE(inference_stack, STACK_SIZE);
struct k_thread inference_thread;

void run_inference_int8(tflite::MicroInterpreter &interpreter, int8_t input_data[13]) {
    TfLiteTensor* input = interpreter.input(0);
    if (!input) {
        printf("Failed to get input tensor\n");
        return;
    }

    if (input->type != kTfLiteInt8) {
        printf("Input tensor type is not INT8\n");
        return;
    }

    float input_scale = input->params.scale;
    int input_zero_point = input->params.zero_point;

    // Quantize raw input data using input tensor's scale and zero_point
    for (int i = 0; i < 13; i++) {
        float real_val = (float)input_data[i]; // interpret incoming as float-ish
        input->data.int8[i] = (int8_t)(real_val / input_scale + input_zero_point);
    }

    // Run inference
    if (interpreter.Invoke() != kTfLiteOk) {
        printf("Model inference failed\n");
        return;
    }

    TfLiteTensor* output = interpreter.output(0);
    if (!output) {
        printf("Failed to get output tensor\n");
        return;
    }

    if (output->type != kTfLiteInt8) {
        printf("Output tensor type is not INT8\n");
        return;
    }

    int8_t* output_data = output->data.int8;
    float output_scale = output->params.scale;
    int output_zero_point = output->params.zero_point;

    const char* labels[] = { "Low Resource", "Borderline", "Anomalous" };
    int output_len = output->dims->data[output->dims->size - 1];

    printf("Model Output:\n");

    float max_score = -1000.0f;
    int max_index = -1;

    for (int i = 0; i < output_len; i++) {
        float value = (output_data[i] - output_zero_point) * output_scale;
        printf("Detection_Consumption[%d]: %f\n", i, value);

        if (value > max_score) {
            max_score = value;
            max_index = i;
        }
    }

    if (max_index >= 0 && max_index < 3) {
        printf("Result: %s (index %d, score %f)\n", labels[max_index], max_index, max_score);
    } else {
        printf("Result: Unknown (%d)\n", max_index);
    }
}

void run_inference_float(tflite::MicroInterpreter &interpreter, int8_t input_data[13]) {
    TfLiteTensor* input = interpreter.input(0);
    if (!input) {
        printf("Failed to get input tensor\n");
        return;
    }

    if (input->type != kTfLiteFloat32) {
        printf("Input tensor type is not FLOAT32\n");
        return;
    }

    // Convert int8_t input to float before passing to model
    for (int i = 0; i < 13; i++) {
        input->data.f[i] = (float)input_data[i];
    }

    if (interpreter.Invoke() != kTfLiteOk) {
        printf("Model inference failed\n");
        return;
    }

    TfLiteTensor* output = interpreter.output(0);
    if (!output) {
        printf("Failed to get output tensor\n");
        return;
    }

    if (output->type != kTfLiteFloat32) {
        printf("Output tensor type is not FLOAT32\n");
        return;
    }

    float* output_data = output->data.f;
    const char* labels[] = { "Low Resource", "Borderline", "Anomalous" };
    int output_len = output->dims->data[output->dims->size - 1];

    printf("Model Output:\n");

    float max_score = -1000.0f;
    int max_index = -1;

    for (int i = 0; i < output_len; i++) {
        float value = output_data[i];
        printf("Detection_Consumption[%d]: %f\n", i, value);

        if (value > max_score) {
            max_score = value;
            max_index = i;
        }
    }

    if (max_index >= 0 && max_index < 3) {
        printf("Result: %s (index %d, score %f)\n", labels[max_index], max_index, max_score);
    } else {
        printf("Result: Unknown (%d)\n", max_index);
    }
}


void analyze_message(int8_t input_data[13]) {
    printf("Starting TensorFlow Lite Micro Inference...\n");

    // Increase the max ops if needed
    tflite::MicroMutableOpResolver<10> resolver;

    // Register required operations
    resolver.AddFullyConnected();
    resolver.AddSoftmax();

    const tflite::Model* model = tflite::GetModel(gmodel_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("Model schema version mismatch\n");
        return;
    }

    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize, nullptr);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("Failed to allocate tensors\n");
        return;
    }
    
	uint32_t start_cycles = k_cycle_get_32();
	
    //run_inference_int8(interpreter, input_data);
    run_inference_float(interpreter, input_data);
    
    // Get CPU cycle count at end
    uint32_t end_cycles = k_cycle_get_32();
    // Convert cycles to time in microseconds (µs)
    float elapsed_us = (float)k_cyc_to_us_floor32(end_cycles - start_cycles);
    // Convert to milliseconds (ms) for readability
    float elapsed_ms = elapsed_us / 1000.0f;
    
    printf("Inference Analysis Time Per Msg: %.2f ms (%.0f µs)\r\n", elapsed_ms, elapsed_us);
    
}