#include "NvInfer.h"
#include "cuda_runtime_api.h"
#include "logging.h"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <chrono>
#include <cmath>
#include <dirent.h>
#include <numeric>
#include <opencv2/opencv.hpp>
#include <bitset>
#include "mem_utils.h"
#define CHECK(status) \
    do\
    {\
        auto ret = (status);\
        if (ret != 0)\
        {\
            std::cerr << "Cuda failure: " << ret << std::endl;\
            abort();\
        }\
    } while (0)
// stuff we know about the network and the input/output blobs
static const int INPUT_H = 224;
static const int INPUT_W = 224;
static const int OUTPUT_SIZE = 45;

const char* INPUT_BLOB_NAME = "data";
const char* OUTPUT_BLOB_NAME = "prob";
const char* OUTPUT_BLOB_NAME_INDEX = "index";

const std::array<float, 3> IMAGENET_DEFAULT_MEAN = {0.485f, 0.456f, 0.406f};
const std::array<float, 3> IMAGENET_DEFAULT_STD = {0.229f, 0.224f, 0.225f};

using namespace nvinfer1;

static inline cv::Mat preprocess_img(const cv::Mat& img) {
    cv::Mat resized_img;
    cv::resize(img, resized_img, cv::Size(INPUT_W, INPUT_H), 0, 0, cv::INTER_CUBIC);
    resized_img.convertTo(resized_img, CV_32FC3);
    resized_img /= 255.0;
    cv::Scalar mean = cv::Scalar(IMAGENET_DEFAULT_MEAN[2], IMAGENET_DEFAULT_MEAN[1], IMAGENET_DEFAULT_MEAN[0]);
    cv::Scalar std = cv::Scalar(IMAGENET_DEFAULT_STD[2], IMAGENET_DEFAULT_STD[1], IMAGENET_DEFAULT_STD[0]);
    cv::subtract(resized_img, mean, resized_img);
    cv::divide(resized_img, std, resized_img);
    return resized_img;
}
static Logger gLogger;  

std::vector<std::string> read_classes(std::string file_name) {
  std::vector<std::string> classes;
  std::ifstream ifs(file_name, std::ios::in);
  if (!ifs.is_open()) {
    std::cerr << file_name << " is not found, pls refer to README and download it." << std::endl;
    assert(0);
  }
  std::string s;
  while (std::getline(ifs, s)) {
    classes.push_back(s);
  }
  ifs.close();
  return classes;
}

void doInference(IExecutionContext& context, float* input, float* output, int* output_index, int batchSize)
{
    const ICudaEngine& engine = context.getEngine();

    CHECK(engine.getNbIOTensors()!=3);
    // std::cout << "Number of I/O tensors: " << nbTensors << std::endl;
    
    // // Get tensor names and print their info
    // for (int i = 0; i < nbTensors; i++) {
    //     const char* tensorName = engine.getIOTensorName(i);
    //     TensorIOMode mode = engine.getTensorIOMode(tensorName);
    //     std::cout << "Tensor " << i << ": " << tensorName << " (Mode: " << (mode == TensorIOMode::kINPUT ? "Input" : "Output") << ")" << std::endl;
    // }

    // Get tensor names
    const char* inputName = INPUT_BLOB_NAME;
    // const char* softmaxName = "softmax_output";  // New tensor for debugging
    const char* outputName = OUTPUT_BLOB_NAME;
    const char* outputIndexName = OUTPUT_BLOB_NAME_INDEX;

    // Check if tensors exist and are inputs/outputs
    assert(engine.getTensorIOMode(inputName) == TensorIOMode::kINPUT);
    assert(engine.getTensorIOMode(outputName) == TensorIOMode::kOUTPUT);
    assert(engine.getTensorIOMode(outputIndexName) == TensorIOMode::kOUTPUT);

    // Get tensor shapes
    Dims inputDims = engine.getTensorShape(inputName);
    Dims outputDims = engine.getTensorShape(outputName);
    Dims outputIdxDims = engine.getTensorShape(outputIndexName);

    // Create GPU buffers on device
    void* inputBuffer;
    void* outputBuffer;
    void* outputIndexBuffer;
    // void* softmaxBuffer;
    
    CHECK(cudaMalloc(&inputBuffer, batchSize * 3 * INPUT_H * INPUT_W * sizeof(float)));
    CHECK(cudaMalloc(&outputBuffer, batchSize * sizeof(float)));
    CHECK(cudaMalloc(&outputIndexBuffer, batchSize * sizeof(int32_t)));
    // CHECK(cudaMalloc(&softmaxBuffer, batchSize * OUTPUT_SIZE * sizeof(float)));

    // Set tensor addresses (this is the new way in TensorRT 10.3)
    context.setTensorAddress(inputName, inputBuffer);
    context.setTensorAddress(outputName, outputBuffer);
    context.setTensorAddress(outputIndexName, outputIndexBuffer);
    // context.setTensorAddress(softmaxName, softmaxBuffer);

    // Create stream
    cudaStream_t stream;
    CHECK(cudaStreamCreate(&stream));

    // Copy input data to device
    CHECK(cudaMemcpyAsync(inputBuffer, input, batchSize * 3 * INPUT_H * INPUT_W * sizeof(float), cudaMemcpyHostToDevice, stream));
    
    // Execute inference with the new API (no need to pass bindings)
    bool status = context.enqueueV3(stream);
    if (!status) {
        std::cerr << "Error during inference execution" << std::endl;
    }
    
    // Copy output back to host
    CHECK(cudaMemcpyAsync(output, outputBuffer, batchSize * sizeof(float), cudaMemcpyDeviceToHost, stream));
    CHECK(cudaMemcpyAsync(output_index, outputIndexBuffer, batchSize * sizeof(int32_t), cudaMemcpyDeviceToHost, stream));
    
    // // After inference, copy softmax outputs
    // float softmaxOutput[OUTPUT_SIZE];
    // CHECK(cudaMemcpyAsync(softmaxOutput, softmaxBuffer, 
    //                      batchSize * OUTPUT_SIZE * sizeof(float), 
    //                      cudaMemcpyDeviceToHost, stream));
    
    cudaStreamSynchronize(stream);

    // std::cout << "=== Output Values ===" << std::endl;
    // std::cout << "Probability: " << output[0] << std::endl;
    // std::cout << "Class Index: " << output_index[0] << std::endl;
    
    // // Print all class probabilities
    // std::cout << "Class probabilities:" << std::endl;
    // for (int i = 0; i < OUTPUT_SIZE; i++) {
    //     std::cout << "Class " << i << ": " << softmaxOutput[i] << std::endl;
    // }
    
    // Release stream and buffers
    cudaStreamDestroy(stream);
    CHECK(cudaFree(inputBuffer));
    CHECK(cudaFree(outputBuffer));
    CHECK(cudaFree(outputIndexBuffer));
    // CHECK(cudaFree(softmaxBuffer));
}

void readImageInfo(const std::string& filePath, std::vector<std::string>& paths, std::vector<int>& labels) {
    std::ifstream inputFile(filePath);
    std::string line;
    while (std::getline(inputFile, line)) {
        std::istringstream iss(line);
        std::string path;
        std::string labelStr;

        // Assuming the format is "path,label"
        if (std::getline(iss, path, ',') && std::getline(iss, labelStr, ',')) {
            paths.push_back(path);
            labels.push_back(std::stoi(labelStr));
        }
    }

    inputFile.close();
}
std::map<int, int> loadErrors(const std::string file, int lineidx)
{
    std::cout << "Loading errors from line " << lineidx << " in file: " << file << std::endl;
    std::map<int, int> error_count;
    std::ifstream infile(file);

    if (infile.is_open()) {
        std::string line;
        int current_line = 0;
        while (std::getline(infile, line)) {
            ++current_line;
            if (current_line == lineidx) {
                std::istringstream iss(line);
                int error, count;
                char colon;
                while (iss >> error >> colon >> count) {
                    error_count[error] = count;
                }
                break;  // Stop reading after processing the specified line
            }
        }
        infile.close();
    } else {
        std::cerr << "Error: Unable to open error model file" << std::endl;
        return error_count;
    }
    return error_count;
}


int main(int argc, char** argv)
{
    auto start = std::chrono::system_clock::now();
    std::string engine_name = "";
    std::string labels_dir;
    std::string images_cfg;
    int bitflip;
    int bitidx;
    int device;
    int lineidx;
    int time;
    int bias;
    if (argc == 11 && std::string(argv[1]) == "-d") {
        engine_name = std::string(argv[2]);
        images_cfg = std::string(argv[3]);
        labels_dir = std::string(argv[4]);
        bitflip = std::stoi(argv[5]);
        bitidx = std::stoi(argv[6]);
        device = std::stoi(argv[7]);
        lineidx = std::stoi(argv[8]);
        time = std::stoi(argv[9]);
        bias = std::stoi(argv[10]);
        // cudaSetDevice(device);
    } else {
        std::cerr << "arguments not right!" << std::endl;
        std::cerr << "./resnet50_inference_error -d [.engine] [images_cfg.txt] [labels_dir.txt] [bitflip] [bitidx] [device] [lineidx] [time] [bias]// deserialize plan file and run inference" << std::endl;
        return -1;
    }

    std::string logFileName = "block_" + engine_name + "_" +std::to_string(bitflip)+"_" + std::to_string(bitidx) +  "_" + std::to_string(bias) + "_" + std::to_string(time) + ".txt";
    std::ofstream logfile(logFileName);
    if (!logfile.is_open()) {
        std::cerr << "Failed to open log file" << std::endl;
        // Return an empty vector since we can't log anything
        return {};
    }

    // change your path
    std::string cfg = "/data/workplace/xln/radiation-error-emulator/libREMU/configs/LPDDR4-config.cfg";
    std::string mapping = "/data/workplace/xln/radiation-error-emulator/libREMU/mappings/LPDDR4_row_interleaving_16.map";
    std::string error_file = "/data/workplace/xln/radiation-error-emulator/example/error_counts_"+std::to_string(bitflip) + ".txt";
    std::cout << "mapping: " << mapping << std::endl;

    char *trtModelStream{nullptr};
    size_t size{0};
    std::ifstream file(engine_name, std::ios::binary);
    if (file.good()) {
        file.seekg(0, file.end);
        size = file.tellg();
        file.seekg(0, file.beg);
        trtModelStream = new char[size];
        assert(trtModelStream);
        file.read(trtModelStream, size);
        file.close();
    }

    IRuntime* runtime = createInferRuntime(gLogger);
    assert(runtime != nullptr);


    uintptr_t Vaddr = reinterpret_cast<uintptr_t>(trtModelStream);
    std::cout << "vaddr: "  << std::hex << Vaddr <<"-"<<std::dec <<size << std::endl;
    // if lineidx == 0, do DNN inference without any error.
    if(lineidx){
        std::map<int, int> errorMap = loadErrors(error_file, lineidx); 
        MemUtils memUtils;
        Pmem block =  memUtils.get_block_in_pmems(Vaddr, size, bias);
        std::cout << "block_vaddr: " << std::hex << block.s_Vaddr << "-" << std::dec << block.size << std::endl;
        memUtils.get_error_Va(block.s_Vaddr, block.size, logfile, bitflip, bitidx, cfg, mapping, errorMap);
    }
    

    ICudaEngine* engine = runtime->deserializeCudaEngine(trtModelStream, size);
    assert(engine != nullptr);

    std::cout<<"Engine deserialized\n";
    IExecutionContext* context = engine->createExecutionContext();
    assert(context != nullptr);

    std::cout<<"Context created\n";

    delete[] trtModelStream;


    std::vector<std::string> image_files;
    std::vector<int> image_targets;
    readImageInfo(images_cfg, image_files, image_targets);
    static float data[3 * INPUT_H * INPUT_W];
    static float prob[1];
    static int idx[1];

    auto classes = read_classes(labels_dir);
    
    int correct = 0;


    for (size_t j = 0; j < image_files.size(); j++){
        if(j==100) break; 
        // std::cout << j << " " << image_files[j] << std::endl;
        cv::Mat img = cv::imread(image_files[j]);
        if (img.empty()) continue;

        cv::Mat pr_img = preprocess_img(img);
        for (int i = 0; i < INPUT_H * INPUT_W; i++) {
            data[i] = pr_img.at<cv::Vec3f>(i)[2];
            data[i + INPUT_H * INPUT_W] = pr_img.at<cv::Vec3f>(i)[1];
            data[i + 2 * INPUT_H * INPUT_W] = pr_img.at<cv::Vec3f>(i)[0];
        }

        // Print some values from the preprocessed image
        // std::cout << "Preprocessed values: " 
        //           << data[0] << ", " << data[INPUT_H * INPUT_W] << ", " << data[2 * INPUT_H * INPUT_W] 
        //           << std::endl;

        doInference(*context, data, prob, idx, 1);
        // std::cout <<" "<<image_targets[j]<<"-"<<idx[0]<< " " << classes[idx[0]] << " " << prob[0] << std::endl;
        if (image_targets[j] == idx[0]) correct++;

    }
    
    std::cout<<"Inference done\n";
    // auto end = std::chrono::system_clock::now();
    // std::cout << "total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
    double accuracy = static_cast<double>(correct) / 100;
    logfile << "Bitflip: " << std::dec << bitflip << ". Accuracy: " << accuracy << std::endl;
    std::cout << "Bitflip: " << std::dec << bitflip << ". Accuracy: " << accuracy << std::endl;
    // Destroy the engine
    delete context;
    delete engine;
    delete runtime;
    return 0;
}
