/**
 * Copyright (C) 2021 Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * - enixma_analytic -
 *
 * This application loads a larod model which takes an image as input and
 * outputs values corresponding to the class, score and location of detected
 * objects in the image.
 *
 * The application expects eight arguments on the command line in the
 * following order: MODEL WIDTH HEIGHT QUALITY RAW_WIDTH RAW_HEIGHT
 * THRESHOLD LABELSFILE.
 *
 * First argument, MODEL, is a string describing path to the model.
 *
 * Second argument, WIDTH, is an integer for the input width.
 *
 * Third argument, HEIGHT, is an integer for the input height.
 *
 * Fourth argument, QUALITY, is an integer for the desired jpeg quality.
 *
 * Fifth argument, RAW_WIDTH is an integer for camera width resolution.
 *
 * Sixth argument, RAW_HEIGHT is an integer for camera height resolution.
 *
 * Seventh argument, THRESHOLD is an integer ranging from 0 to 100 to select good detections.
 *
 * Eighth argument, LABELSFILE, is a string describing path to the label txt.
 *
 */

#pragma once

#include "argparse.h"
#include "imgprovider.h"
#include "imgutils.h"
#include "larod.h"
#include "vdo-frame.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "vdo-types.h"
#pragma GCC diagnostic pop

typedef struct {
    const char* labelsFile;
    int inputWidth;
    int inputHeight;
    int threshold;
    int quality;
} ProgramArgs;

typedef struct {
    ImgProvider_t* sdImageProvider;
    ImgProvider_t* hdImageProvider;
} ImageProviders;

typedef struct {
    larodError* error;
    larodConnection* conn;
    larodTensor** inputTensors;
    size_t numInputs;
    larodTensor** outputTensors;
    size_t numOutputs;
    larodJobRequest* ppReq;
    larodJobRequest* ppReqHD;
    larodJobRequest* infReq;
    void* larodOutput1Addr;
    void* larodOutput2Addr;
    void* larodOutput3Addr;
    void* larodOutput4Addr;

    int larodOutput1Fd;
    int larodOutput2Fd;
    int larodOutput3Fd;
    int larodOutput4Fd;
} LarodResources;

typedef struct {
    void* ppInputAddr;
    void* ppInputAddrHD;
    void* ppOutputAddrHD;
} MemoryAddresses;

typedef struct {
    unsigned int widthFrameHD;
    unsigned int heightFrameHD;
} FrameSize;

typedef struct {
    size_t yuyvBufferSize;
} BufferProperties;

typedef struct {
    char** labels;
    char* labelFileData;
} LabelsData;

typedef struct {
    ProgramArgs args;
    ImageProviders providers;
    LarodResources larod;
    MemoryAddresses addresses;
    FrameSize resolution;
    BufferProperties buffer;
    LabelsData label;
} FrameContext;

extern FrameContext context;

/**
 * @brief Free up resources held by an array of labels.
 *
 * @param labels An array of label string pointers.
 * @param labelFileBuffer Heap buffer containing the actual string data.
 */
void freeLabels(char** labelsArray, char* labelFileBuffer);

/**
 * @brief Reads a file of labels into an array.
 *
 * An array filled by this function should be freed using freeLabels.
 *
 * @param labelsPtr Pointer to a string array.
 * @param labelFileBuffer Pointer to the labels file contents.
 * @param labelsPath String containing the path to the labels file to be read.
 * @param numLabelsPtr Pointer to number which will store number of labels read.
 * @return False if any errors occur, otherwise true.
 */
bool parseLabels(char*** labelsPtr,
                        char** labelFileBuffer,
                        const char* labelsPath,
                        size_t* numLabelsPtr);

/**
 * @brief Creates a temporary fd and truncated to correct size and mapped.
 *
 * This convenience function creates temp files to be used for input and output.
 *
 * @param fileName Pattern for how the temp file will be named in file system.
 * @param fileSize How much space needed to be allocated (truncated) in fd.
 * @param mappedAddr Pointer to the address of the fd mapped for this process.
 * @param Pointer to the generated fd.
 * @return Positive errno style return code (zero means success).
 */
bool createAndMapTmpFile(char* fileName, size_t fileSize, void** mappedAddr, int* convFd);

/**
 * @brief Sets up and configures a connection to larod, and loads a model.
 *
 * Opens a connection to larod, which is tied to larodConn. After opening a
 * larod connection the chip specified by chipString is set for the
 * connection. Then the model file specified by larodModelFd is loaded to the
 * chip, and a corresponding larodModel object is tied to model.
 *
 * @param chipString Speficier for which larod chip to use.
 * @param larodModelFd Fd for a model file to load.
 * @param larodConn Pointer to a larod connection to be opened.
 * @param model Pointer to a larodModel to be obtained.
 * @return false if error has occurred, otherwise true.
 */
bool setupLarod(const char* chipString,
                       const int larodModelFd,
                       larodConnection** larodConn,
                       larodModel** model);