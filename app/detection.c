#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "detection.h"

FrameContext context;

/**
 * @brief Free up resources held by an array of labels.
 *
 * @param labels An array of label string pointers.
 * @param labelFileBuffer Heap buffer containing the actual string data.
 */
void freeLabels(char** labelsArray, char* labelFileBuffer) {
    free(labelsArray);
    free(labelFileBuffer);
}

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
                        size_t* numLabelsPtr) {
    // We cut off every row at 60 characters.
    const size_t LINE_MAX_LEN = 60;
    bool ret                  = false;
    char* labelsData          = NULL;  // Buffer containing the label file contents.
    char** labelArray         = NULL;  // Pointers to each line in the labels text.

    struct stat fileStats = {0};
    if (stat(labelsPath, &fileStats) < 0) {
        syslog(LOG_ERR,
               "%s: Unable to get stats for label file %s: %s",
               __func__,
               labelsPath,
               strerror(errno));
        return false;
    }

    // Sanity checking on the file size - we use size_t to keep track of file
    // size and to iterate over the contents. off_t is signed and 32-bit or
    // 64-bit depending on architecture. We just check toward 10 MByte as we
    // will not encounter larger label files and both off_t and size_t should be
    // able to represent 10 megabytes on both 32-bit and 64-bit systems.
    if (fileStats.st_size > (10 * 1024 * 1024)) {
        syslog(LOG_ERR, "%s: failed sanity check on labels file size", __func__);
        return false;
    }

    int labelsFd = open(labelsPath, O_RDONLY);
    if (labelsFd < 0) {
        syslog(LOG_ERR,
               "%s: Could not open labels file %s: %s",
               __func__,
               labelsPath,
               strerror(errno));
        return false;
    }

    size_t labelsFileSize = (size_t)fileStats.st_size;
    // Allocate room for a terminating NULL char after the last line.
    labelsData = malloc(labelsFileSize + 1);
    if (labelsData == NULL) {
        syslog(LOG_ERR, "%s: Failed allocating labels text buffer: %s", __func__, strerror(errno));
        goto end;
    }

    ssize_t numBytesRead  = -1;
    size_t totalBytesRead = 0;
    char* fileReadPtr     = labelsData;
    while (totalBytesRead < labelsFileSize) {
        numBytesRead = read(labelsFd, fileReadPtr, labelsFileSize - totalBytesRead);

        if (numBytesRead < 1) {
            syslog(LOG_ERR, "%s: Failed reading from labels file: %s", __func__, strerror(errno));
            goto end;
        }
        totalBytesRead += (size_t)numBytesRead;
        fileReadPtr += numBytesRead;
    }

    // Now count number of lines in the file - check all bytes except the last
    // one in the file.
    size_t numLines = 0;
    for (size_t i = 0; i < (labelsFileSize - 1); i++) {
        if (labelsData[i] == '\n') {
            numLines++;
        }
    }

    // We assume that there is always a line at the end of the file, possibly
    // terminated by newline char. Either way add this line as well to the
    // counter.
    numLines++;

    labelArray = malloc(numLines * sizeof(char*));
    if (!labelArray) {
        syslog(LOG_ERR, "%s: Unable to allocate labels array: %s", __func__, strerror(errno));
        ret = false;
        goto end;
    }

    size_t labelIdx      = 0;
    labelArray[labelIdx] = labelsData;
    labelIdx++;
    for (size_t i = 0; i < labelsFileSize; i++) {
        if (labelsData[i] == '\n') {
            if (i < (labelsFileSize - 1)) {
                // Register the string start in the list of labels.
                labelArray[labelIdx] = labelsData + i + 1;
                labelIdx++;
            }
            // Replace the newline char with string-ending NULL char.
            labelsData[i] = '\0';
        }
    }

    // Make sure we always have a terminating NULL char after the label file
    // contents.
    labelsData[labelsFileSize] = '\0';

    // Now go through the list of strings and cap if strings too long.
    for (size_t i = 0; i < numLines; i++) {
        size_t stringLen = strnlen(labelArray[i], LINE_MAX_LEN);
        if (stringLen >= LINE_MAX_LEN) {
            // Just insert capping NULL terminator to limit the string len.
            *(labelArray[i] + LINE_MAX_LEN + 1) = '\0';
        }
    }

    *labelsPtr       = labelArray;
    *numLabelsPtr    = numLines;
    *labelFileBuffer = labelsData;

    ret = true;

end:
    if (!ret) {
        freeLabels(labelArray, labelsData);
    }
    close(labelsFd);

    return ret;
}

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
bool createAndMapTmpFile(char* fileName, size_t fileSize, void** mappedAddr, int* convFd) {
    syslog(LOG_INFO,
           "%s: Setting up a temp fd with pattern %s and size %zu",
           __func__,
           fileName,
           fileSize);

    int fd = mkstemp(fileName);
    if (fd < 0) {
        syslog(LOG_ERR, "%s: Unable to open temp file %s: %s", __func__, fileName, strerror(errno));
        goto error;
    }

    // Allocate enough space in for the fd.
    if (ftruncate(fd, (off_t)fileSize) < 0) {
        syslog(LOG_ERR,
               "%s: Unable to truncate temp file %s: %s",
               __func__,
               fileName,
               strerror(errno));
        goto error;
    }

    // Remove since we don't actually care about writing to the file system.
    if (unlink(fileName)) {
        syslog(LOG_ERR,
               "%s: Unable to unlink from temp file %s: %s",
               __func__,
               fileName,
               strerror(errno));
        goto error;
    }

    // Get an address to fd's memory for this process's memory space.
    void* data = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (data == MAP_FAILED) {
        syslog(LOG_ERR, "%s: Unable to mmap temp file %s: %s", __func__, fileName, strerror(errno));
        goto error;
    }

    *mappedAddr = data;
    *convFd     = fd;

    return true;

error:
    if (fd >= 0) {
        close(fd);
    }

    return false;
}

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
                       larodModel** model) {
    larodError* error       = NULL;
    larodConnection* conn   = NULL;
    larodModel* loadedModel = NULL;
    bool ret                = false;

    // Set up larod connection.
    if (!larodConnect(&conn, &error)) {
        syslog(LOG_ERR, "%s: Could not connect to larod: %s", __func__, error->msg);
        goto end;
    }

    // List available chip id:s
    size_t numDevices = 0;
    syslog(LOG_INFO, "Available chip IDs:");
    const larodDevice** devices;
    devices = larodListDevices(conn, &numDevices, &error);
    for (size_t i = 0; i < numDevices; ++i) {
        syslog(LOG_INFO, "%s: %s", "Chip", larodGetDeviceName(devices[i], &error));
        ;
    }
    const larodDevice* dev = larodGetDevice(conn, chipString, 0, &error);
    loadedModel            = larodLoadModel(conn,
                                 larodModelFd,
                                 dev,
                                 LAROD_ACCESS_PRIVATE,
                                 "enixma_analytic",
                                 NULL,
                                 &error);
    if (!loadedModel) {
        syslog(LOG_ERR, "%s: Unable to load model: %s", __func__, error->msg);
        goto error;
    }
    *larodConn = conn;
    *model     = loadedModel;

    ret = true;

    goto end;

error:
    if (conn) {
        larodDisconnect(&conn, NULL);
    }

end:
    if (error) {
        larodClearError(&error);
    }

    return ret;
}