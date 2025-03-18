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

// stuff we know about the network and the input/output blobs
static const int INPUT_H = 224;
static const int INPUT_W = 224;
static const int OUTPUT_SIZE = 45;
static const float ALPHA = 0;

const char* INPUT_BLOB_NAME = "data";
const char* OUTPUT_BLOB_NAME = "prob";
const char* OUTPUT_BLOB_NAME_INDEX = "index";

const std::array<float, 3> IMAGENET_DEFAULT_MEAN = {0.485f, 0.456f, 0.406f};
const std::array<float, 3> IMAGENET_DEFAULT_STD = {0.229f, 0.224f, 0.225f};

using namespace nvinfer1;
static Logger gLogger;  

// Load weights from files shared with TensorRT samples.
// TensorRT weight files have a simple space delimited format:
// [type] [size] <data x size in hex>
std::map<std::string, Weights> loadWeights(const std::string file)
{
    std::cout << "Loading weights: " << file << std::endl;
    std::map<std::string, Weights> weightMap;

    // Open weights file
    std::ifstream input(file);
    assert(input.is_open() && "Unable to load weight file.");

    // Read number of weight blobs
    int32_t count;
    input >> count;
    assert(count > 0 && "Invalid weight map file.");

    while (count--)
    {
        Weights wt{DataType::kFLOAT, nullptr, 0};
        uint32_t size;

        // Read name and type of blob
        std::string name;
        input >> name >> std::dec >> size;
        wt.type = DataType::kFLOAT;

        // Load blob
        uint32_t* val = reinterpret_cast<uint32_t*>(malloc(sizeof(val) * size));
        for (uint32_t x = 0, y = size; x < y; ++x)
        {
            input >> std::hex >> val[x];
        }
        wt.values = val;
        
        wt.count = size;
        weightMap[name] = wt;
    }

    return weightMap;
}

IScaleLayer* addBatchNorm2d(INetworkDefinition *network, std::map<std::string, Weights>& weightMap, ITensor& input, std::string lname, float eps) {
    float *gamma = (float*)weightMap[lname + ".weight"].values;
    float *beta = (float*)weightMap[lname + ".bias"].values;
    float *mean = (float*)weightMap[lname + ".running_mean"].values;
    float *var = (float*)weightMap[lname + ".running_var"].values;
    int len = weightMap[lname + ".running_var"].count;
    std::cout << "len " << len << std::endl;

    float *scval = reinterpret_cast<float*>(malloc(sizeof(float) * len));
    for (int i = 0; i < len; i++) {
        scval[i] = gamma[i] / sqrt(var[i] + eps);
    }
    Weights scale{DataType::kFLOAT, scval, len};
    
    float *shval = reinterpret_cast<float*>(malloc(sizeof(float) * len));
    for (int i = 0; i < len; i++) {
        shval[i] = beta[i] - mean[i] * gamma[i] / sqrt(var[i] + eps);
    }
    Weights shift{DataType::kFLOAT, shval, len};

    float *pval = reinterpret_cast<float*>(malloc(sizeof(float) * len));
    for (int i = 0; i < len; i++) {
        pval[i] = 1.0;
    }
    Weights power{DataType::kFLOAT, pval, len};

    weightMap[lname + ".scale"] = scale;
    weightMap[lname + ".shift"] = shift;
    weightMap[lname + ".power"] = power;
    IScaleLayer* scale_1 = network->addScale(input, ScaleMode::kCHANNEL, shift, scale, power);
    assert(scale_1);
    return scale_1;
}

ILayer* bottleneck(INetworkDefinition *network, std::map<std::string, Weights>& weightMap, ITensor& input, int inch, int outch, int stride, std::string lname) {
    Weights emptywts{DataType::kFLOAT, nullptr, 0};

    IConvolutionLayer* conv1 = network->addConvolutionNd(input, outch, DimsHW{1, 1}, weightMap[lname + "conv1.weight"], emptywts);
    assert(conv1);

    IScaleLayer* bn1 = addBatchNorm2d(network, weightMap, *conv1->getOutput(0), lname + "bn1", 1e-5);

    IActivationLayer* relu1 = network->addActivation(*bn1->getOutput(0), ActivationType::kRELU);
    assert(relu1);

    IConvolutionLayer* conv2 = network->addConvolutionNd(*relu1->getOutput(0), outch, DimsHW{3, 3}, weightMap[lname + "conv2.weight"], emptywts);
    assert(conv2);
    conv2->setStrideNd(DimsHW{stride, stride});
    conv2->setPaddingNd(DimsHW{1, 1});

    IScaleLayer* bn2 = addBatchNorm2d(network, weightMap, *conv2->getOutput(0), lname + "bn2", 1e-5);

    IActivationLayer* relu2 = network->addActivation(*bn2->getOutput(0), ActivationType::kRELU);
    assert(relu2);

    IConvolutionLayer* conv3 = network->addConvolutionNd(*relu2->getOutput(0), outch * 4, DimsHW{1, 1}, weightMap[lname + "conv3.weight"], emptywts);
    assert(conv3);

    IScaleLayer* bn3 = addBatchNorm2d(network, weightMap, *conv3->getOutput(0), lname + "bn3", 1e-5);

    IElementWiseLayer* ew1;
    if (stride != 1 || inch != outch * 4) {
        IConvolutionLayer* conv4 = network->addConvolutionNd(input, outch * 4, DimsHW{1, 1}, weightMap[lname + "downsample.0.weight"], emptywts);
        assert(conv4);
        conv4->setStrideNd(DimsHW{stride, stride});

        IScaleLayer* bn4 = addBatchNorm2d(network, weightMap, *conv4->getOutput(0), lname + "downsample.1", 1e-5);
        ew1 = network->addElementWise(*bn4->getOutput(0), *bn3->getOutput(0), ElementWiseOperation::kSUM);
    } else {
        ew1 = network->addElementWise(input, *bn3->getOutput(0), ElementWiseOperation::kSUM);
    }
    IActivationLayer* relu3 = network->addActivation(*ew1->getOutput(0), ActivationType::kRELU);
    assert(relu3);
    return relu3;
}

//!
//! \brief  Write network tensor names to a file.
//!
void writeNetworkTensorNames(INetworkDefinition *network)
{
    std::cout << "Sample requires to run with per-tensor dynamic range." << std::endl;
    std::cout << "In order to run Int8 inference without calibration, user will need to provide dynamic range for all "
                "the network tensors."
             << std::endl;

    std::ofstream tensorsFile{"tensor_name.txt"};

    // Iterate through network inputs to write names of input tensors.
    for (int i = 0; i < network->getNbInputs(); ++i)
    {
        std::cout << "inputs" << std::endl;
        std::string tName = network->getInput(i)->getName();
        tensorsFile << "TensorName: " << tName << std::endl;
        std::cout << "TensorName: " << tName << std::endl;
    }

    // Iterate through network layers.
    for (int i = 0; i < network->getNbLayers(); ++i)
    {
        // Write output tensors of a layer to the file.
        for (int j = 0; j < network->getLayer(i)->getNbOutputs(); ++j)
        {
            std::cout << "layers" << network->getLayer(i)->getName() << std::endl;
            std::string tName = network->getLayer(i)->getOutput(j)->getName();
            tensorsFile << "TensorName: " << tName << std::endl;
            std::cout << "TensorName: " << tName << std::endl;
        }
    }
    tensorsFile.close();
    std::cout << "Successfully generated network tensor names." << std::endl;
    std::cout
        << "Use the generated tensor names file to create dynamic range file for Int8 inference. Follow README.md "
           "for instructions to generate dynamic_ranges.txt file."
        << std::endl;
}

//!
//! \brief Populate per-tensor dyanamic range values
//!
bool readPerTensorDynamicRangeValues(std::unordered_map<std::string, float>& mPerTensorDynamicRangeMap)
{
    std::ifstream iDynamicRangeStream("dynamic_range.txt");
    if (!iDynamicRangeStream)
    {
        std::cout << "Could not find per-tensor scales file: " << "dynamic_range.txt" << std::endl;
        return false;
    }

    std::string line;
    char delim = ':';
    while (std::getline(iDynamicRangeStream, line))
    {
        std::istringstream iline(line);
        std::string token;
        std::getline(iline, token, delim);
        std::string tensorName = token;
        std::getline(iline, token, delim);
        float dynamicRange = std::stof(token);
        mPerTensorDynamicRangeMap[tensorName] = dynamicRange;
    }
    return true;
}

//!
//! \brief  Sets custom dynamic range for network tensors
//!
bool setDynamicRange(INetworkDefinition* network)
{
    std::cout << "Setting Per Tensor Dynamic Range" << std::endl;
    std::cout << "If dynamic range for a tensor is missing, TensorRT will run inference assuming dynamic range for "
                "the tensor as optional."
                << std::endl;
    std::cout << "If dynamic range for a tensor is required then inference will fail. Follow README.md to generate "
                "missing per-tensor dynamic range."
                << std::endl;
    // set dynamic range for network input tensors
    for (int i = 0; i < network->getNbInputs(); ++i)
    {
        // std::string tName = network->getInput(i)->getName();
        if (!network->getInput(i)->setDynamicRange(
                -8.0f, 8.0f))
            return false;
    }

    // set dynamic range for layer output tensors
    for (int i = 0; i < network->getNbLayers(); ++i)
    {
        auto lyr = network->getLayer(i);
        for (int j = 0, e = lyr->getNbOutputs(); j < e; ++j)
        {
            if (lyr->getType()!= LayerType::kCONSTANT){
                if (!lyr->getOutput(j)->setDynamicRange(
                        -8.0f, 8.0f))
                    return false;
            }
            else {
                // std::cout <<"kCONSTANT: "<<lyr->getName()<<std::endl;
            }
        }
    } 
    return true;
}
//!
//! \brief  Sets computation precision for network layers
//!
void setLayerPrecision(INetworkDefinition* network)
{
    for (int i = 0; i < network->getNbLayers(); ++i)
    {
        auto layer = network->getLayer(i);
        std::string layerName = layer->getName();
        if (layer->getType() != LayerType::kCONSTANT &&
            layer->getType() != LayerType::kCONCATENATION && 
            layer->getType() != LayerType::kSHAPE)
        {
            layer->setPrecision(nvinfer1::DataType::kINT8);
        }
        if (layer->getType() == LayerType::kCONVOLUTION){
            IConvolutionLayer * cLyr = static_cast<IConvolutionLayer*>(layer);
        }
        for (int j = 0; j < layer->getNbOutputs(); ++j)
        {
            std::string tensorName = layer->getOutput(j)->getName();
            if (layer->getOutput(j)->isExecutionTensor() && layer->getType() != LayerType::kCONSTANT)  // Don't set INT8 output type for constants
            {
                layer->setOutputType(j, nvinfer1::DataType::kINT8);
            }
        }
    }
}

// Creat the engine using only the API and not any parser.
ICudaEngine* createEngine(unsigned int maxBatchSize, IBuilder* builder, IBuilderConfig* config, DataType dt, std::string& wts_name)
{
    INetworkDefinition* network = builder->createNetworkV2(0U);

    // Create input tensor of shape { 3, INPUT_H, INPUT_W } with name INPUT_BLOB_NAME
    ITensor* data = network->addInput(INPUT_BLOB_NAME, dt, Dims3{3, INPUT_H, INPUT_W});
    assert(data);

    std::map<std::string, Weights> weightMap = loadWeights(wts_name);
    Weights emptywts{DataType::kFLOAT, nullptr, 0};

    IConvolutionLayer* conv1 = network->addConvolutionNd(*data, 64, DimsHW{7, 7}, weightMap["conv1.weight"], emptywts);
    assert(conv1);
    conv1->setStrideNd(DimsHW{2, 2});
    conv1->setPaddingNd(DimsHW{3, 3});

    IScaleLayer* bn1 = addBatchNorm2d(network, weightMap, *conv1->getOutput(0), "bn1", 1e-5);

    // Add activation layer using the ReLU algorithm.
    IActivationLayer* relu1 = network->addActivation(*bn1->getOutput(0), ActivationType::kRELU);
    assert(relu1);
    

    // Add max pooling layer with stride of 2x2 and kernel size of 2x2.
    IPoolingLayer* pool1 = network->addPoolingNd(*relu1->getOutput(0), PoolingType::kMAX, DimsHW{3, 3});
    assert(pool1);
    pool1->setStrideNd(DimsHW{2, 2});
    pool1->setPaddingNd(DimsHW{1, 1});

    ILayer* x = bottleneck(network, weightMap, *pool1->getOutput(0), 64, 64, 1, "layer1.0.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 256, 64, 1, "layer1.1.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 256, 64, 1, "layer1.2.");

    x = bottleneck(network, weightMap, *x->getOutput(0), 256, 128, 2, "layer2.0.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 512, 128, 1, "layer2.1.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 512, 128, 1, "layer2.2.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 512, 128, 1, "layer2.3.");

    x = bottleneck(network, weightMap, *x->getOutput(0), 512, 256, 2, "layer3.0.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 256, 1, "layer3.1.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 256, 1, "layer3.2.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 256, 1, "layer3.3.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 256, 1, "layer3.4.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 256, 1, "layer3.5.");

    x = bottleneck(network, weightMap, *x->getOutput(0), 1024, 512, 2, "layer4.0.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 2048, 512, 1, "layer4.1.");
    x = bottleneck(network, weightMap, *x->getOutput(0), 2048, 512, 1, "layer4.2.");

    IPoolingLayer* pool2 = network->addPoolingNd(*x->getOutput(0), PoolingType::kAVERAGE, DimsHW{7, 7});
    assert(pool2);
    pool2->setStrideNd(DimsHW{1, 1});
    
    IFullyConnectedLayer* fc1 = network->addFullyConnected(*pool2->getOutput(0), OUTPUT_SIZE, weightMap["fc.weight"], weightMap["fc.bias"]);
    assert(fc1);
    
    // fc1->getOutput(0)->setName(OUTPUT_BLOB_NAME);
    // std::cout << "set name out" << std::endl;
    // network->markOutput(*fc1->getOutput(0));

    ISoftMaxLayer* softmax_layer=network->addSoftMax(*fc1->getOutput(0));
    assert(softmax_layer);
    
    ITopKLayer* prob_ans = network->addTopK(*softmax_layer->getOutput(0), TopKOperation::kMAX,1,1);//?
    assert(prob_ans);

    prob_ans->getOutput(0)->setName(OUTPUT_BLOB_NAME);
    prob_ans->getOutput(1)->setName(OUTPUT_BLOB_NAME_INDEX);
    network->markOutput(*prob_ans->getOutput(0)); 
    network->markOutput(*prob_ans->getOutput(1));  
    std::cout << "set main subengine name out " << ((prob_ans->getOutput(0))->getName())<<" "<<((prob_ans->getOutput(1))->getName()) <<std::endl;

    if (false)
    {
        writeNetworkTensorNames(network);
        std::cout << "Write network tensor names to a file." << std::endl;
        return nullptr;
    }

    builder->setMaxBatchSize(maxBatchSize);
    config->setMaxWorkspaceSize(1 << 30);
    setLayerPrecision(network);
    if (!setDynamicRange(network))
    {
        std::cout << "Unable to set per-tensor dynamic range." << std::endl;
        return nullptr;
    }
    config->setFlag(BuilderFlag::kINT8);
    config->setInt8Calibrator(nullptr);
    config->setFlag(BuilderFlag::kPREFER_PRECISION_CONSTRAINTS);

    ICudaEngine* engine = builder->buildEngineWithConfig(*network, *config);
    std::cout << "build out" << std::endl;
    network->destroy();
    for (auto& mem : weightMap)
    {
        free((void*) (mem.second.values));
    }

    return engine;
}

void APIToModel(unsigned int maxBatchSize, IHostMemory** modelStream, std::string& wts_name)
{
    // Create builder
    IBuilder* builder = createInferBuilder(gLogger);
    IBuilderConfig* config = builder->createBuilderConfig();
    if (!builder->platformHasFastInt8())
        std::cout << "Platform does not support INT8 inference. sampleINT8API can only run in INT8 Mode." << std::endl;

    // Create model to populate the network, then set the outputs and create an engine
    ICudaEngine* engine = createEngine(maxBatchSize, builder, config, DataType::kFLOAT, wts_name);
    assert(engine != nullptr);

    // Serialize the engine
    (*modelStream) = engine->serialize();

    // Close everything down
    engine->destroy();
    config->destroy();
    builder->destroy();
}

int main(int argc, char** argv)
{
    std::string wts_name = "";
    std::string engine_name = "";

    if (argc == 4 && std::string(argv[1]) == "-s") {
        IHostMemory* modelStream{nullptr};
        wts_name = std::string(argv[2]);
        engine_name = std::string(argv[3]);
        APIToModel(1, &modelStream, wts_name);
        assert(modelStream != nullptr);
        std::ofstream p(engine_name, std::ios::binary);
        if (!p)
        {
            std::cerr << "could not open plan output file" << std::endl;
            return -1;
        }
        p.write(reinterpret_cast<const char*>(modelStream->data()), modelStream->size());
        modelStream->destroy();
        return 1;
    } else {
        std::cerr << "arguments not right!" << std::endl;
        std::cerr << "./resnet50 -s [.wts] [.engine]  // serialize model to plan file" << std::endl;
        return -1;
    }
}