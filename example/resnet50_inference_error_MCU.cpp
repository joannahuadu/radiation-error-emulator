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

    // Pointers to input and output device buffers to pass to engine.
    // Engine requires exactly IEngine::getNbBindings() number of buffers.
    assert(engine.getNbBindings() == 3);
    void* buffers[3];

    // In order to bind the buffers, we need to know the names of the input and output tensors.
    // Note that indices are guaranteed to be less than IEngine::getNbBindings()
    const int inputIndex = engine.getBindingIndex(INPUT_BLOB_NAME);
    const int outputIndex = engine.getBindingIndex(OUTPUT_BLOB_NAME);
    const int indexIndex = engine.getBindingIndex(OUTPUT_BLOB_NAME_INDEX);

    // Create GPU buffers on device
    CHECK(cudaMalloc(&buffers[inputIndex], batchSize * 3 * INPUT_H * INPUT_W * sizeof(float)));
    CHECK(cudaMalloc(&buffers[outputIndex], batchSize * sizeof(float)));
    CHECK(cudaMalloc(&buffers[indexIndex], batchSize * sizeof(int32_t)));

    // Create stream
    cudaStream_t stream;
    CHECK(cudaStreamCreate(&stream));
    
    // cudaEvent_t start, stop;
    // cudaEventCreate(&start);
    // cudaEventCreate(&stop);
    // cudaEventRecord(start); 
    CHECK(cudaMemcpyAsync(buffers[inputIndex], input, batchSize * 3 * INPUT_H * INPUT_W * sizeof(float), cudaMemcpyHostToDevice, stream));
    context.enqueue(batchSize, buffers, stream, nullptr);
    CHECK(cudaMemcpyAsync(output, buffers[outputIndex], batchSize * sizeof(float), cudaMemcpyDeviceToHost, stream));
    CHECK(cudaMemcpyAsync(output_index, buffers[indexIndex], batchSize * sizeof(int32_t), cudaMemcpyDeviceToHost, stream));
    // cudaEventRecord(stop);
    // cudaEventSynchronize(stop);
    // float milliseconds = 0;
    // cudaEventElapsedTime(&milliseconds, start, stop);
    // std::cout << milliseconds<< "ms" << std::endl;
    cudaStreamSynchronize(stream);
    // print the three output
    // std::cout<<"why"<<std::endl;
    // for(int i=0;i<batchSize;i++){
    //     std::cout << "Output: " << output[i] << " Index: " << output_index[i] << std::endl;
    // }

    // Release stream and buffers
    cudaStreamDestroy(stream);
    CHECK(cudaFree(buffers[inputIndex]));
    CHECK(cudaFree(buffers[outputIndex]));
    CHECK(cudaFree(buffers[indexIndex]));
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
        std::cerr << "Error: Unable to open file" << std::endl;
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
    double dq;
    if (argc == 11 && std::string(argv[1]) == "-d") {
        engine_name = std::string(argv[2]);
        images_cfg = std::string(argv[3]);
        labels_dir = std::string(argv[4]);
        bitflip = std::stoi(argv[5]);
        bitidx = std::stoi(argv[6]);
        device = std::stoi(argv[7]);
        lineidx = std::stoi(argv[8]);
        time = std::stoi(argv[9]);
        dq = std::stod(argv[10]);
        // cudaSetDevice(device);
    } else {
        std::cerr << "arguments not right!" << std::endl;
        std::cerr << "sudo ./resnet50_inference_error_MCU -d [.engine] [images_cfg.txt] [labels_dir.txt] [bitflip] [bitidx] [device] [lineidx] [time] [dq]// deserialize plan file and run inference" << std::endl;
        return -1;
    }

    std::string logFileName = engine_name + "_" +std::to_string(bitflip)+"_" +  std::to_string(bitidx) + "_" + std::to_string(dq) + "_" + std::to_string(time) + ".txt";
    std::ofstream logfile(logFileName);
    if (!logfile.is_open()) {
        std::cerr << "Failed to open log file" << std::endl;
        // Return an empty vector since we can't log anything
        return {};
    }


    // change your path
    std::string tree_mapping = "../../libREMU/configs/lpddr4_jetson_xavier_nx.yaml";
    std::string error_file = "../../example/MCU_counts_"+std::to_string(bitflip) + ".txt";
    std::cout << "mapping: " << tree_mapping << std::endl;

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
        std::map<int, int> errorMap = loadErrors(error_file, 1); 
        size_t dram_capacity_gb=16;
        MemUtils memUtils(dram_capacity_gb);
        // MemUtils::get_error_Va_tree(&memUtils, Vaddr+300, size-300, logfile, bitflip, bitidx, tree_mapping, errorMap);
        MemUtils::get_error_Va_tree(&memUtils, Vaddr+300, size-300, logfile, bitflip, bitidx, tree_mapping, errorMap, dq);
    }

    ICudaEngine* engine = runtime->deserializeCudaEngine(trtModelStream, size, nullptr);
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
        cv::Mat img = cv::imread(image_files[j]);
        if (img.empty()) continue;

        cv::Mat pr_img = preprocess_img(img);
        for (int i = 0; i < INPUT_H * INPUT_W; i++) {
            data[i] = pr_img.at<cv::Vec3f>(i)[2];
            data[i + INPUT_H * INPUT_W] = pr_img.at<cv::Vec3f>(i)[1];
            data[i + 2 * INPUT_H * INPUT_W] = pr_img.at<cv::Vec3f>(i)[0];
        }

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
    context->destroy();
    engine->destroy();
    runtime->destroy();
    return 0;
}