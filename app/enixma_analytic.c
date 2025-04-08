#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include <glib.h>

#include "overlay.h"
#include "detection.h"
#include "deepsort.h"
#include "roi.h"
#include "counting.h"
#include "fastcgi.h"
#include "imwrite.h"
#include "event.h"

static GMainLoop *main_loop = NULL;
static gint animation_timer = -1;
static gint overlay_id = -1;
static gint overlay_id_text = -1;

gdouble start_value  = 0.0;

/**
 * @brief Callback function which is called when animation timer has elapsed.
 *
 * This function is called when the animation timer has elapsed, which will
 * update the counter, colors and also trigger a redraw of the overlay.
 *
 * @param user_data Optional callback user data.
 */
static gboolean process_frame(gpointer user_data)
{
    /* Silence compiler warnings for unused @parameters/arguments */
    (void)user_data;
    FrameContext *context = (FrameContext *)user_data;

    GError *overlayError = NULL;

    struct timeval startTs, endTs;
    // unsigned int elapsedMs = 0;

    // Hardcode to use three image "color" channels (eg. RGB).
    const unsigned int CHANNELS = 3;
    // Hardcode to set output bytes of four tensors from MobileNet V2 model.
    const unsigned int FLOATSIZE = 4;
    const unsigned int TENSOR1SIZE = 80 * FLOATSIZE;
    const unsigned int TENSOR2SIZE = 20 * FLOATSIZE;
    const unsigned int TENSOR3SIZE = 20 * FLOATSIZE;
    const unsigned int TENSOR4SIZE = 1 * FLOATSIZE;

    ImgProvider_t *sdImageProvider = context->providers.sdImageProvider;
    ImgProvider_t *hdImageProvider = context->providers.hdImageProvider;
    larodError *error = context->larod.error;
    larodConnection *conn = context->larod.conn;
    larodTensor **inputTensors = context->larod.inputTensors;
    size_t numInputs = context->larod.numInputs;
    larodTensor **outputTensors = context->larod.outputTensors;
    size_t numOutputs = context->larod.numOutputs;
    larodJobRequest *ppReq = context->larod.ppReq;
    larodJobRequest *ppReqHD = context->larod.ppReqHD;
    larodJobRequest *infReq = context->larod.infReq;
    void *ppInputAddr = context->addresses.ppInputAddr;
    void *ppInputAddrHD = context->addresses.ppInputAddrHD;
    void *ppOutputAddrHD = context->addresses.ppOutputAddrHD;

    void *larodOutput1Addr = context->larod.larodOutput1Addr;
    void *larodOutput2Addr = context->larod.larodOutput2Addr;
    void *larodOutput3Addr = context->larod.larodOutput3Addr;
    void *larodOutput4Addr = context->larod.larodOutput4Addr;

    int larodOutput1Fd = context->larod.larodOutput1Fd;
    int larodOutput2Fd = context->larod.larodOutput2Fd;
    int larodOutput3Fd = context->larod.larodOutput3Fd;
    int larodOutput4Fd = context->larod.larodOutput4Fd;

    char **labels = context->label.labels;
    char *labelFileData = context->label.labelFileData;

    unsigned int widthFrameHD = context->resolution.widthFrameHD;
    unsigned int heightFrameHD = context->resolution.heightFrameHD;
    size_t yuyvBufferSize = context->buffer.yuyvBufferSize;

    const char *labelsFile = context->args.labelsFile;
    const int inputWidth = context->args.inputWidth;
    const int inputHeight = context->args.inputWidth;
    const int threshold = confidence;
    // const int threshold          = context->args.threshold;
    // const int quality            = context->args.quality;

    // Get latest frame from image pipeline.
    VdoBuffer *buf = getLastFrameBlocking(sdImageProvider);
    if (!buf)
    {
        syslog(LOG_ERR, "buf empty in provider");
        goto end;
    }

    VdoBuffer *buf_hq = getLastFrameBlocking(hdImageProvider);
    if (!buf_hq)
    {
        syslog(LOG_ERR, "buf empty in provider high resolution");
        goto end;
    }

    // Get data from latest frame.
    uint8_t *nv12Data = (uint8_t *)vdo_buffer_get_data(buf);
    uint8_t *nv12Data_hq = (uint8_t *)vdo_buffer_get_data(buf_hq);

    // Covert image data from NV12 format to interleaved uint8_t RGB format.
    gettimeofday(&startTs, NULL);

    memcpy(ppInputAddr, nv12Data, yuyvBufferSize);
    if (!larodRunJob(conn, ppReq, &error))
    {
        syslog(LOG_ERR,
               "Unable to run job to preprocess model: %s (%d)",
               error->msg,
               error->code);
        goto end;
    }
    memcpy(ppInputAddrHD, nv12Data_hq, widthFrameHD * heightFrameHD * CHANNELS / 2);
    if (!larodRunJob(conn, ppReqHD, &error))
    {
        syslog(LOG_ERR,
               "Unable to run job to preprocess model: %s (%d)",
               error->msg,
               error->code);
        goto end;
    }

    gettimeofday(&endTs, NULL);

    // elapsedMs = (unsigned int)(((endTs.tv_sec - startTs.tv_sec) * 1000) +
    //                            ((endTs.tv_usec - startTs.tv_usec) / 1000));
    // syslog(LOG_INFO, "Converted image in %u ms", elapsedMs);

    // Since larodOutputAddr points to the beginning of the fd we should
    // rewind the file position before each job.
    if (lseek(larodOutput1Fd, 0, SEEK_SET) == -1)
    {
        syslog(LOG_ERR, "Unable to rewind output file position: %s", strerror(errno));
        goto end;
    }

    if (lseek(larodOutput2Fd, 0, SEEK_SET) == -1)
    {
        syslog(LOG_ERR, "Unable to rewind output file position: %s", strerror(errno));
        goto end;
    }

    if (lseek(larodOutput3Fd, 0, SEEK_SET) == -1)
    {
        syslog(LOG_ERR, "Unable to rewind output file position: %s", strerror(errno));
        goto end;
    }

    if (lseek(larodOutput4Fd, 0, SEEK_SET) == -1)
    {
        syslog(LOG_ERR, "Unable to rewind output file position: %s", strerror(errno));
        goto end;
    }

    gettimeofday(&startTs, NULL);
    if (!larodRunJob(conn, infReq, &error))
    {
        syslog(LOG_ERR,
               "Unable to run inference on model %s: %s (%d)",
               labelsFile,
               error->msg,
               error->code);
        goto end;
    }
    gettimeofday(&endTs, NULL);

    // elapsedMs = (unsigned int)(((endTs.tv_sec - startTs.tv_sec) * 1000) +
    //                            ((endTs.tv_usec - startTs.tv_usec) / 1000));
    // syslog(LOG_INFO, "Ran inference for %u ms", elapsedMs);

    float *locations = (float *)larodOutput1Addr;
    float *classes = (float *)larodOutput2Addr;
    float *scores = (float *)larodOutput3Addr;
    float *numberOfDetections = (float *)larodOutput4Addr;

    // if ((int)numberOfDetections[0] == 0) {
    //     syslog(LOG_INFO, "No object is detected");
    //     continue;
    // }

    for (int i = 0; i < numberOfDetections[0]; i++)
    {
        // float top    = locations[4 * i];
        // float left   = locations[4 * i + 1];
        // float bottom = locations[4 * i + 2];
        // float right  = locations[4 * i + 3];

        // if (scores[i] >= threshold / 100.0) {
        //     syslog(LOG_INFO,
        //             "Object %d: Classes: %s - Scores: %f - Locations: [%f,%f,%f,%f]",
        //             i,
        //             labels[(int)classes[i]],
        //             scores[i],
        //             top,
        //             left,
        //             bottom,
        //             right);
        // }

        // Update tracker with new detections
        update_tracker(tracker,
                       locations,
                       classes,
                       scores,
                       numberOfDetections[0],
                       threshold,
                       labels);
    }

    // Check for periodic backup (every 5 minutes)
    check_periodic_backup(counting_system);

    // Check for midnight reset (already part of your system)
    check_midnight_reset(counting_system);

    // Release frame reference to provider.
    returnFrame(sdImageProvider, buf);
    returnFrame(hdImageProvider, buf_hq);

    // Request a redraw of the overlay
    axoverlay_redraw(&overlayError);
    if (overlayError != NULL)
    {
        /*
         * If redraw fails then it is likely due to that overlayd has
         * crashed. Don't exit instead wait for overlayd to restart and
         * for axoverlay to restore the connection.
         */
        syslog(LOG_ERR, "Failed to redraw overlay (%d): %s", overlayError->code, overlayError->message);
        g_error_free(overlayError);
    }

    return G_SOURCE_CONTINUE;

end:
    if (sdImageProvider)
    {
        destroyImgProvider(sdImageProvider);
    }
    if (hdImageProvider)
    {
        destroyImgProvider(hdImageProvider);
    }
    // Only the model handle is released here. We count on larod service to
    // release the privately loaded model when the session is disconnected in
    // larodDisconnect().
    // larodDestroyMap(&ppMap);
    // larodDestroyMap(&cropMap);
    // larodDestroyMap(&ppMapHD);
    // larodDestroyModel(&ppModel);
    // larodDestroyModel(&ppModelHD);
    // larodDestroyModel(&model);
    if (conn)
    {
        larodDisconnect(&conn, NULL);
    }
    // if (larodModelFd >= 0) {
    //     close(larodModelFd);
    // }
    // if (larodInputAddr != MAP_FAILED) {
    //     munmap(larodInputAddr, inputWidth * inputHeight * CHANNELS);
    // }
    // if (larodInputFd >= 0) {
    //     close(larodInputFd);
    // }
    if (ppInputAddr != MAP_FAILED)
    {
        munmap(ppInputAddr, inputWidth * inputHeight * CHANNELS);
    }
    // if (ppInputFd >= 0) {
    //     close(ppInputFd);
    // }
    if (ppInputAddrHD != MAP_FAILED)
    {
        munmap(ppInputAddrHD, widthFrameHD * heightFrameHD * CHANNELS / 2);
    }
    // if (ppInputFdHD >= 0) {
    //     close(ppInputFdHD);
    // }
    if (ppOutputAddrHD != MAP_FAILED)
    {
        munmap(ppOutputAddrHD, widthFrameHD * heightFrameHD * CHANNELS);
    }
    // if (ppOutputFdHD >= 0) {
    //     close(ppOutputFdHD);
    // }
    // if (cropAddr != MAP_FAILED) {
    //     munmap(cropAddr, widthFrameHD * heightFrameHD * CHANNELS);
    // }
    if (larodOutput1Addr != MAP_FAILED)
    {
        munmap(larodOutput1Addr, TENSOR1SIZE);
    }

    if (larodOutput2Addr != MAP_FAILED)
    {
        munmap(larodOutput2Addr, TENSOR2SIZE);
    }

    if (larodOutput3Addr != MAP_FAILED)
    {
        munmap(larodOutput3Addr, TENSOR3SIZE);
    }

    if (larodOutput4Addr != MAP_FAILED)
    {
        munmap(larodOutput4Addr, TENSOR4SIZE);
    }

    if (larodOutput1Fd >= 0)
    {
        close(larodOutput1Fd);
    }

    if (larodOutput2Fd >= 0)
    {
        close(larodOutput2Fd);
    }

    if (larodOutput3Fd >= 0)
    {
        close(larodOutput3Fd);
    }

    if (larodOutput4Fd >= 0)
    {
        close(larodOutput4Fd);
    }
    larodDestroyJobRequest(&ppReq);
    larodDestroyJobRequest(&ppReqHD);
    larodDestroyJobRequest(&infReq);
    larodDestroyTensors(conn, &inputTensors, numInputs, &error);
    larodDestroyTensors(conn, &outputTensors, numOutputs, &error);
    larodClearError(&error);

    if (labels)
    {
        freeLabels(labels, labelFileData);
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief A callback function called when an overlay needs to be drawn.
 *
 * This function is called whenever the system redraws an overlay. This can
 * happen in two cases, axoverlay_redraw() is called or a new stream is
 * started.
 *
 * @param rendering_context A pointer to the rendering context.
 * @param id Overlay id.
 * @param stream Information about the rendered stream.
 * @param postype The position type.
 * @param overlay_x The x coordinate of the overlay.
 * @param overlay_y The y coordinate of the overlay.
 * @param overlay_width Overlay width.
 * @param overlay_height Overlay height.
 * @param user_data Optional user data associated with this overlay.
 */
static void render_overlay_cb(gpointer rendering_context,
                              gint id,
                              struct axoverlay_stream_data *stream,
                              enum axoverlay_position_type postype,
                              gfloat overlay_x,
                              gfloat overlay_y,
                              gint overlay_width,
                              gint overlay_height,
                              gpointer user_data)
{
    /* Silence compiler warnings for unused @parameters/arguments */
    (void)postype;
    (void)user_data;
    (void)overlay_x;
    (void)overlay_y;
    (void)overlay_width;
    (void)overlay_height;

    FrameContext *context = (FrameContext *)user_data;
    (void)context;

    gdouble val = FALSE;

    // syslog(LOG_INFO, "Render callback for camera: %i", stream->camera);
    // syslog(LOG_INFO, "Render callback for overlay: %i x %i", overlay_width, overlay_height);
    // syslog(LOG_INFO, "Render callback for stream: %i x %i", stream->width, stream->height);

    if (!roi1 || !roi2)
    {
        // syslog(LOG_INFO, "ROI is NULL");
        return;
    }

    if (id == overlay_id)
    {
        //  Clear background by drawing a "filled" rectangle
        val = index2cairo(0);
        cairo_set_source_rgba(rendering_context, val, val, val, val);
        cairo_set_operator(rendering_context, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle(rendering_context, 0, 0, stream->width, stream->height);
        cairo_fill(rendering_context);
    }
    else if (id == overlay_id_text)
    {
        //  Draw transparent
        draw_transparent(rendering_context, 0, 0, stream->width, stream->height);
        //  Draw polygons
        draw_roi_polygon(rendering_context, stream->width, stream->height, 3.0);
        //  Show text in black
        draw_label(rendering_context, stream->width, stream->height);
        //  Draw crosslines
        draw_counting_line(rendering_context, stream->width, stream->height, 3.0, counting_system);
        // draw_text(rendering_context, stream->width / 2, stream->height / 2);
        draw_count(rendering_context, stream->width, stream->height, 3.0);
    }
    // else
    // {
    //     syslog(LOG_INFO, "Unknown overlay id!");
    // }
}

/***** Signal handler functions **********************************************/

/**
 * @brief Handles the signals.
 *
 * This function handles the signals when it is time to
 * quit the main loop.
 *
 * @param signal_num Signal number.
 */
static void signal_handler(gint signal_num)
{
    switch (signal_num)
    {
    case SIGTERM:
    case SIGABRT:
    case SIGINT:
        // syslog(LOG_INFO, "Received signal %d, cleaning up...", signal_num);
        fcgi_running = 0;
        g_main_loop_quit(main_loop);
        break;
    default:
        break;
    }
}

/**
 * @brief Initialize the signal handler.
 *
 * This function handles the initialization of signals.
 *
 * return result as boolean.
 */
static gboolean signal_handler_init(void)
{
    struct sigaction sa = {0};

    if (sigemptyset(&sa.sa_mask) == -1)
    {
        syslog(LOG_ERR, "Failed to initialize signal handler: %s", strerror(errno));
        return FALSE;
    }

    sa.sa_handler = signal_handler;

    if ((sigaction(SIGTERM, &sa, NULL) < 0) || (sigaction(SIGABRT, &sa, NULL) < 0) ||
        (sigaction(SIGINT, &sa, NULL) < 0))
    {
        syslog(LOG_ERR, "Failed to install signal handler: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Main function that starts a stream with different options.
 */
int main(int argc, char **argv)
{
    /* Silence compiler warnings for unused parameters/arguments */
    (void)argc;
    (void)argv;

    GError *overlay_error = NULL;
    GError *overlay_error_text = NULL;
    gint camera_height = 0;
    gint camera_width = 0;

    // Hardcode to use three image "color" channels (eg. RGB).
    const unsigned int CHANNELS = 3;
    // Hardcode to set output bytes of four tensors from MobileNet V2 model.
    const unsigned int FLOATSIZE = 4;
    const unsigned int TENSOR1SIZE = 80 * FLOATSIZE;
    const unsigned int TENSOR2SIZE = 20 * FLOATSIZE;
    const unsigned int TENSOR3SIZE = 20 * FLOATSIZE;
    const unsigned int TENSOR4SIZE = 1 * FLOATSIZE;

    // Name patterns for the temp file we will create.

    // Pre-processing of the High resolution frame input and output
    char PP_HD_INPUT_FILE_PATTERN[] = "/tmp/larod.pp.hd.test-XXXXXX";
    char PP_HD_OUTPUT_FILE_PATTERN[] = "/tmp/larod.pp.hd.out.test-XXXXXX";

    // Pre-processing of the Low resolution frame input and output
    // The output of the pre-processing correspond with the input of the object detector
    char PP_SD_INPUT_FILE_PATTERN[] = "/tmp/larod.pp.test-XXXXXX";
    char OBJECT_DETECTOR_INPUT_FILE_PATTERN[] = "/tmp/larod.in.test-XXXXXX";

    char OBJECT_DETECTOR_OUT1_FILE_PATTERN[] = "/tmp/larod.out1.test-XXXXXX";
    char OBJECT_DETECTOR_OUT2_FILE_PATTERN[] = "/tmp/larod.out2.test-XXXXXX";
    char OBJECT_DETECTOR_OUT3_FILE_PATTERN[] = "/tmp/larod.out3.test-XXXXXX";
    char OBJECT_DETECTOR_OUT4_FILE_PATTERN[] = "/tmp/larod.out4.test-XXXXXX";

    bool ret = false;
    context.providers.sdImageProvider = NULL;
    context.providers.hdImageProvider = NULL;
    context.larod.error = NULL;
    context.larod.conn = NULL;
    larodMap *ppMap = NULL;
    larodMap *cropMap = NULL;
    larodMap *ppMapHD = NULL;
    larodModel *ppModel = NULL;
    larodModel *ppModelHD = NULL;
    larodModel *model = NULL;
    larodTensor **ppInputTensors = NULL;
    size_t ppNumInputs = 0;
    larodTensor **ppOutputTensors = NULL;
    size_t ppNumOutputs = 0;
    larodTensor **ppInputTensorsHD = NULL;
    size_t ppNumInputsHD = 0;
    larodTensor **ppOutputTensorsHD = NULL;
    size_t ppNumOutputsHD = 0;
    context.larod.inputTensors = NULL;
    context.larod.numInputs = 0;
    context.larod.outputTensors = NULL;
    context.larod.numOutputs = 0;
    context.larod.ppReq = NULL;
    context.larod.ppReqHD = NULL;
    context.larod.infReq = NULL;
    void *cropAddr = NULL;
    context.addresses.ppInputAddr = MAP_FAILED;
    context.addresses.ppInputAddrHD = MAP_FAILED;
    context.addresses.ppOutputAddrHD = MAP_FAILED;
    void *larodInputAddr = MAP_FAILED; // this address is both used for the output of the
                                       // preprocessing and input for the inference
    context.larod.larodOutput1Addr = MAP_FAILED;
    context.larod.larodOutput2Addr = MAP_FAILED;
    context.larod.larodOutput3Addr = MAP_FAILED;
    context.larod.larodOutput4Addr = MAP_FAILED;
    int larodModelFd = -1;
    int ppInputFd = -1;
    int ppInputFdHD = -1;
    int ppOutputFdHD = -1;
    int larodInputFd = -1; // This file descriptor is used for both as output for the pre
                           // processing and input for the inference
    context.larod.larodOutput1Fd = -1;
    context.larod.larodOutput2Fd = -1;
    context.larod.larodOutput3Fd = -1;
    context.larod.larodOutput4Fd = -1;
    context.label.labels = NULL;        // This is the array of label strings. The label
                                        // entries points into the large labelFileData buffer.
    size_t numLabels = 0;               // Number of entries in the labels array.
    context.label.labelFileData = NULL; // Buffer holding the complete collection of label strings.

    // Open the syslog to report messages for "enixma_analytic"
    openlog("enixma_analytic", LOG_PID | LOG_CONS, LOG_USER);

    if (!signal_handler_init())
    {
        syslog(LOG_ERR, "Could not set up signal handler");
        return 1;
    }

    //  Create a glib main loop
    main_loop = g_main_loop_new(NULL, FALSE);

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // Start FastCGI thread
    if (pthread_create(&fcgi_thread, NULL, fcgi_thread_func, NULL) != 0)
    {
        syslog(LOG_ERR, "Failed to create FastCGI thread");
        goto end;
    }

    args_t args;
    if (!parseArgs(argc, argv, &args))
    {
        syslog(LOG_ERR, "%s: Could not parse arguments", __func__);
        goto earlyend;
    }

    srand(time(0));

    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);

    // syslog(LOG_INFO, "Starting %s at time %i o'clock", argv[0], local_time->tm_hour);

    const char *chipString = args.chip;
    const char *modelFile = (local_time->tm_hour >= 6 && local_time->tm_hour < 18) ? args.day_modelFile : args.night_modelFile;
    context.args.labelsFile = args.labelsFile;
    context.args.inputWidth = args.width;
    context.args.inputHeight = args.height;
    const int desiredHDImgWidth = args.raw_width;
    const int desiredHDImgHeight = args.raw_height;
    context.args.threshold = args.threshold;
    context.args.quality = args.quality;

    syslog(LOG_INFO, "Finding best resolution to use as model input");
    unsigned int streamWidth = 0;
    unsigned int streamHeight = 0;
    if (!chooseStreamResolution(context.args.inputWidth, context.args.inputHeight, &streamWidth, &streamHeight))
    {
        syslog(LOG_ERR, "%s: Failed choosing stream resolution", __func__);
        goto end;
    }

    syslog(LOG_INFO,
           "Creating VDO image provider and creating stream %d x %d",
           streamWidth,
           streamHeight);
    context.providers.sdImageProvider = createImgProvider(streamWidth, streamHeight, 2, VDO_FORMAT_YUV);
    if (!context.providers.sdImageProvider)
    {
        syslog(LOG_ERR, "%s: Could not create image provider", __func__);
        goto end;
    }
    syslog(LOG_INFO, "Find the best resolution to save the high resolution image");
    // unsigned int widthFrameHD;
    // unsigned int heightFrameHD;
    if (!chooseStreamResolution(desiredHDImgWidth,
                                desiredHDImgHeight,
                                &context.resolution.widthFrameHD,
                                &context.resolution.heightFrameHD))
    {
        syslog(LOG_ERR, "%s: Failed choosing HD resolution", __func__);
        goto end;
    }
    syslog(LOG_INFO,
           "Creating VDO High resolution image provider and stream %d x %d",
           context.resolution.widthFrameHD,
           context.resolution.heightFrameHD);
    context.providers.hdImageProvider = createImgProvider(context.resolution.widthFrameHD, context.resolution.heightFrameHD, 2, VDO_FORMAT_YUV);
    if (!context.providers.hdImageProvider)
    {
        syslog(LOG_ERR, "%s: Could not create high resolution image provider", __func__);
    }

    // Calculate crop image
    // 1. The crop area shall fill the input image either horizontally or
    //    vertically.
    // 2. The crop area shall have the same aspect ratio as the output image.
    syslog(LOG_INFO, "Calculate crop image");
    float destWHratio = (float)context.args.inputWidth / (float)context.args.inputHeight;
    float cropW = (float)streamWidth;
    float cropH = cropW / destWHratio;
    if (cropH > (float)streamHeight)
    {
        cropH = (float)streamHeight;
        cropW = cropH * destWHratio;
    }
    unsigned int clipW = (unsigned int)cropW;
    unsigned int clipH = (unsigned int)cropH;
    unsigned int clipX = (streamWidth - clipW) / 2;
    unsigned int clipY = (streamHeight - clipH) / 2;
    syslog(LOG_INFO, "Crop VDO image X=%d Y=%d (%d x %d)", clipX, clipY, clipW, clipH);

    // Create preprocessing maps
    syslog(LOG_INFO, "Create preprocessing maps");
    ppMap = larodCreateMap(&context.larod.error);
    if (!ppMap)
    {
        syslog(LOG_ERR, "Could not create preprocessing larodMap %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetStr(ppMap, "image.input.format", "nv12", &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetIntArr2(ppMap, "image.input.size", streamWidth, streamHeight, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetStr(ppMap, "image.output.format", "rgb-interleaved", &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }

    if (!larodMapSetIntArr2(ppMap, "image.output.size", context.args.inputWidth, context.args.inputHeight, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    ppMapHD = larodCreateMap(&context.larod.error);
    if (!ppMapHD)
    {
        syslog(LOG_ERR, "Could not create preprocessing high resolution larodMap %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetStr(ppMapHD, "image.input.format", "nv12", &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetIntArr2(ppMapHD, "image.input.size", context.resolution.widthFrameHD, context.resolution.heightFrameHD, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetStr(ppMapHD, "image.output.format", "rgb-interleaved", &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetIntArr2(ppMapHD, "image.output.size", context.resolution.widthFrameHD, context.resolution.heightFrameHD, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }

    cropMap = larodCreateMap(&context.larod.error);
    if (!cropMap)
    {
        syslog(LOG_ERR, "Could not create preprocessing crop larodMap %s", context.larod.error->msg);
        goto end;
    }
    if (!larodMapSetIntArr4(cropMap, "image.input.crop", clipX, clipY, clipW, clipH, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting preprocessing parameters: %s", context.larod.error->msg);
        goto end;
    }

    // Create larod models
    syslog(LOG_INFO, "Create larod models");
    larodModelFd = open(modelFile, O_RDONLY);
    if (larodModelFd < 0)
    {
        syslog(LOG_ERR, "Unable to open model file %s: %s", modelFile, strerror(errno));
        goto end;
    }

    syslog(LOG_INFO,
           "Setting up larod connection with chip %s, model %s and label file %s",
           chipString,
           modelFile,
           context.args.labelsFile);
    if (!setupLarod(chipString, larodModelFd, &context.larod.conn, &model))
    {
        goto end;
    }

    // Use libyuv as image preprocessing backend
    const char *larodLibyuvPP = "cpu-proc";
    const larodDevice *dev_pp;
    dev_pp = larodGetDevice(context.larod.conn, larodLibyuvPP, 0, &context.larod.error);
    ppModel = larodLoadModel(context.larod.conn, -1, dev_pp, LAROD_ACCESS_PRIVATE, "", ppMap, &context.larod.error);
    if (!ppModel)
    {
        syslog(LOG_ERR,
               "Unable to load preprocessing model with chip %s: %s",
               larodLibyuvPP,
               context.larod.error->msg);
        goto end;
    }
    else
    {
        syslog(LOG_INFO, "Loading preprocessing model with chip %s", larodLibyuvPP);
    }

    // run image processing also on the high resolution frame

    const larodDevice *dev_pp_hd;
    dev_pp_hd = larodGetDevice(context.larod.conn, larodLibyuvPP, 0, &context.larod.error);
    ppModelHD = larodLoadModel(context.larod.conn, -1, dev_pp_hd, LAROD_ACCESS_PRIVATE, "", ppMapHD, &context.larod.error);
    if (!ppModelHD)
    {
        syslog(LOG_ERR,
               "Unable to load preprocessing model with chip %s: %s",
               larodLibyuvPP,
               context.larod.error->msg);
        goto end;
    }
    else
    {
        syslog(LOG_INFO, "Loading preprocessing model with chip %s", larodLibyuvPP);
    }

    // Create input/output tensors
    syslog(LOG_INFO, "Create input/output tensors");
    ppInputTensors = larodCreateModelInputs(ppModel, &ppNumInputs, &context.larod.error);
    if (!ppInputTensors)
    {
        syslog(LOG_ERR, "Failed retrieving input tensors: %s", context.larod.error->msg);
        goto end;
    }
    ppOutputTensors = larodCreateModelOutputs(ppModel, &ppNumOutputs, &context.larod.error);
    if (!ppOutputTensors)
    {
        syslog(LOG_ERR, "Failed retrieving output tensors: %s", context.larod.error->msg);
        goto end;
    }

    ppInputTensorsHD = larodCreateModelInputs(ppModelHD, &ppNumInputsHD, &context.larod.error);
    if (!ppInputTensorsHD)
    {
        syslog(LOG_ERR, "Failed retrieving input tensors: %s", context.larod.error->msg);
        goto end;
    }
    ppOutputTensorsHD = larodCreateModelOutputs(ppModelHD, &ppNumOutputsHD, &context.larod.error);
    if (!ppOutputTensorsHD)
    {
        syslog(LOG_ERR, "Failed retrieving output tensors: %s", context.larod.error->msg);
        goto end;
    }

    context.larod.inputTensors = larodCreateModelInputs(model, &context.larod.numInputs, &context.larod.error);
    if (!context.larod.inputTensors)
    {
        syslog(LOG_ERR, "Failed retrieving input tensors: %s", context.larod.error->msg);
        goto end;
    }

    context.larod.outputTensors = larodCreateModelOutputs(model, &context.larod.numOutputs, &context.larod.error);
    if (!context.larod.outputTensors)
    {
        syslog(LOG_ERR, "Failed retrieving output tensors: %s", context.larod.error->msg);
        goto end;
    }

    // Determine tensor buffer sizes
    syslog(LOG_INFO, "Determine tensor buffer sizes");
    const larodTensorPitches *ppInputPitches = larodGetTensorPitches(ppInputTensors[0], &context.larod.error);
    if (!ppInputPitches)
    {
        syslog(LOG_ERR, "Could not get pitches of tensor: %s", context.larod.error->msg);
        goto end;
    }
    context.buffer.yuyvBufferSize = ppInputPitches->pitches[0];
    const larodTensorPitches *ppOutputPitches = larodGetTensorPitches(ppOutputTensors[0], &context.larod.error);
    if (!ppOutputPitches)
    {
        syslog(LOG_ERR, "Could not get pitches of tensor: %s", context.larod.error->msg);
        goto end;
    }
    size_t rgbBufferSize = ppOutputPitches->pitches[0];
    size_t expectedSize = context.args.inputWidth * context.args.inputHeight * CHANNELS;
    if (expectedSize != rgbBufferSize)
    {
        syslog(LOG_ERR, "Expected video output size %zu, actual %zu", expectedSize, rgbBufferSize);
        goto end;
    }
    const larodTensorPitches *outputPitches = larodGetTensorPitches(context.larod.outputTensors[0], &context.larod.error);
    if (!outputPitches)
    {
        syslog(LOG_ERR, "Could not get pitches of tensor: %s", context.larod.error->msg);
        goto end;
    }

    // Allocate space for input tensor
    syslog(LOG_INFO, "Allocate memory for input/output buffers");
    if (!createAndMapTmpFile(PP_SD_INPUT_FILE_PATTERN, context.buffer.yuyvBufferSize, &context.addresses.ppInputAddr, &ppInputFd))
    {
        goto end;
    }
    if (!createAndMapTmpFile(OBJECT_DETECTOR_INPUT_FILE_PATTERN,
                             context.args.inputWidth * context.args.inputHeight * CHANNELS,
                             &larodInputAddr,
                             &larodInputFd))
    {
        goto end;
    }
    if (!createAndMapTmpFile(PP_HD_INPUT_FILE_PATTERN,
                             context.resolution.widthFrameHD * context.resolution.heightFrameHD * CHANNELS / 2,
                             &context.addresses.ppInputAddrHD,
                             &ppInputFdHD))
    {
        goto end;
    }
    if (!createAndMapTmpFile(PP_HD_OUTPUT_FILE_PATTERN,
                             context.resolution.widthFrameHD * context.resolution.heightFrameHD * CHANNELS,
                             &context.addresses.ppOutputAddrHD,
                             &ppOutputFdHD))
    {
        goto end;
    }

    if (!createAndMapTmpFile(OBJECT_DETECTOR_OUT1_FILE_PATTERN,
                             TENSOR1SIZE,
                             &context.larod.larodOutput1Addr,
                             &context.larod.larodOutput1Fd))
    {
        goto end;
    }
    if (!createAndMapTmpFile(OBJECT_DETECTOR_OUT2_FILE_PATTERN,
                             TENSOR2SIZE,
                             &context.larod.larodOutput2Addr,
                             &context.larod.larodOutput2Fd))
    {
        goto end;
    }

    if (!createAndMapTmpFile(OBJECT_DETECTOR_OUT3_FILE_PATTERN,
                             TENSOR3SIZE,
                             &context.larod.larodOutput3Addr,
                             &context.larod.larodOutput3Fd))
    {
        goto end;
    }

    if (!createAndMapTmpFile(OBJECT_DETECTOR_OUT4_FILE_PATTERN,
                             TENSOR4SIZE,
                             &context.larod.larodOutput4Addr,
                             &context.larod.larodOutput4Fd))
    {
        goto end;
    }

    // Connect tensors to file descriptors
    syslog(LOG_INFO, "Connect tensors to file descriptors");
    syslog(LOG_INFO, "Set pp input tensors");
    if (!larodSetTensorFd(ppInputTensors[0], ppInputFd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting input tensor fd: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodSetTensorFd(ppOutputTensors[0], larodInputFd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting input tensor fd: %s", context.larod.error->msg);
        goto end;
    }
    syslog(LOG_INFO, "Set pp input tensors for high resolution frame");
    if (!larodSetTensorFd(ppInputTensorsHD[0], ppInputFdHD, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting input tensor fd: %s", context.larod.error->msg);
        goto end;
    }
    if (!larodSetTensorFd(ppOutputTensorsHD[0], ppOutputFdHD, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting input tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    syslog(LOG_INFO, "Set input tensors");
    if (!larodSetTensorFd(context.larod.inputTensors[0], larodInputFd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting input tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    syslog(LOG_INFO, "Set output tensors");
    if (!larodSetTensorFd(context.larod.outputTensors[0], context.larod.larodOutput1Fd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting output tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    if (!larodSetTensorFd(context.larod.outputTensors[1], context.larod.larodOutput2Fd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting output tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    if (!larodSetTensorFd(context.larod.outputTensors[2], context.larod.larodOutput3Fd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting output tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    if (!larodSetTensorFd(context.larod.outputTensors[3], context.larod.larodOutput4Fd, &context.larod.error))
    {
        syslog(LOG_ERR, "Failed setting output tensor fd: %s", context.larod.error->msg);
        goto end;
    }

    // Create job requests
    syslog(LOG_INFO, "Create job requests");
    context.larod.ppReq = larodCreateJobRequest(ppModel,
                                                ppInputTensors,
                                                ppNumInputs,
                                                ppOutputTensors,
                                                ppNumOutputs,
                                                NULL,
                                                &context.larod.error);
    if (!context.larod.ppReq)
    {
        syslog(LOG_ERR, "Failed creating preprocessing job request: %s", context.larod.error->msg);
        goto end;
    }
    context.larod.ppReqHD = larodCreateJobRequest(ppModelHD,
                                                  ppInputTensorsHD,
                                                  ppNumInputsHD,
                                                  ppOutputTensorsHD,
                                                  ppNumOutputsHD,
                                                  NULL,
                                                  &context.larod.error);
    if (!context.larod.ppReqHD)
    {
        syslog(LOG_ERR,
               "Failed creating high resolution preprocessing job request: %s",
               context.larod.error->msg);
        goto end;
    }

    // App supports only one input/output tensor.
    context.larod.infReq = larodCreateJobRequest(model,
                                                 context.larod.inputTensors,
                                                 context.larod.numInputs,
                                                 context.larod.outputTensors,
                                                 context.larod.numOutputs,
                                                 NULL,
                                                 &context.larod.error);
    if (!context.larod.infReq)
    {
        syslog(LOG_ERR, "Failed creating inference request: %s", context.larod.error->msg);
        goto end;
    }

    if (context.args.labelsFile)
    {
        if (!parseLabels(&context.label.labels, &context.label.labelFileData, context.args.labelsFile, &numLabels))
        {
            syslog(LOG_ERR, "Failed creating parsing labels file");
            goto end;
        }
    }

    syslog(LOG_INFO, "Found %zu input tensors and %zu output tensors", context.larod.numInputs, context.larod.numOutputs);
    syslog(LOG_INFO, "Start fetching video frames from VDO");
    if (!startFrameFetch(context.providers.sdImageProvider))
    {
        syslog(LOG_ERR, "Stuck in provider");
        goto end;
    }

    if (!startFrameFetch(context.providers.hdImageProvider))
    {
        syslog(LOG_ERR, "Stuck in provider high resolution");
        goto end;
    }

    if (!axoverlay_is_backend_supported(AXOVERLAY_CAIRO_IMAGE_BACKEND))
    {
        syslog(LOG_ERR, "AXOVERLAY_CAIRO_IMAGE_BACKEND is not supported");
        return 1;
    }

    //  Initialize the library
    struct axoverlay_settings settings;
    axoverlay_init_axoverlay_settings(&settings);
    settings.render_callback = render_overlay_cb;
    settings.adjustment_callback = adjustment_cb;
    settings.select_callback = NULL;
    settings.backend = AXOVERLAY_CAIRO_IMAGE_BACKEND;
    axoverlay_init(&settings, &overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to initialize axoverlay: %s", overlay_error->message);
        g_error_free(overlay_error);
        return 1;
    }

    //  Setup colors
    if (!setup_palette_color(0, 0, 0, 0, 0) || !setup_palette_color(1, 255, 0, 0, 255) ||
        !setup_palette_color(2, 0, 255, 0, 255) || !setup_palette_color(3, 0, 0, 255, 255))
    {
        syslog(LOG_ERR, "Failed to setup palette colors");
        return 1;
    }

    // Get max resolution for width and height
    camera_width = axoverlay_get_max_resolution_width(1, &overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to get max resolution width: %s", overlay_error->message);
        g_error_free(overlay_error);
        overlay_error = NULL;
    }

    camera_height = axoverlay_get_max_resolution_height(1, &overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to get max resolution height: %s", overlay_error->message);
        g_error_free(overlay_error);
        overlay_error = NULL;
    }

    syslog(LOG_INFO, "Max resolution (width x height): %i x %i", camera_width, camera_height);

    // Create a large overlay using Palette color space
    struct axoverlay_overlay_data data;
    setup_axoverlay_data(&data);
    data.width = camera_width;
    data.height = camera_height;
    data.colorspace = AXOVERLAY_COLORSPACE_4BIT_PALETTE;
    overlay_id = axoverlay_create_overlay(&data, NULL, &overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to create first overlay: %s", overlay_error->message);
        g_error_free(overlay_error);
        return 1;
    }

    // Create an text overlay using ARGB32 color space
    struct axoverlay_overlay_data data_text;
    setup_axoverlay_data(&data_text);
    data_text.width = camera_width;
    data_text.height = camera_height;
    data_text.colorspace = AXOVERLAY_COLORSPACE_ARGB32;
    overlay_id_text = axoverlay_create_overlay(&data_text, NULL, &overlay_error_text);
    if (overlay_error_text != NULL)
    {
        syslog(LOG_ERR, "Failed to create second overlay: %s", overlay_error_text->message);
        g_error_free(overlay_error_text);
        return 1;
    }

    // Draw overlays
    axoverlay_redraw(&overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to draw overlays: %s", overlay_error->message);
        axoverlay_destroy_overlay(overlay_id, &overlay_error);
        axoverlay_destroy_overlay(overlay_id_text, &overlay_error_text);
        axoverlay_cleanup();
        g_error_free(overlay_error);
        g_error_free(overlay_error_text);
        return 1;
    }

    // Initialize global ROI
    roi1 = init_polygon(MAX_POLYGON_POINTS);
    roi2 = init_polygon(MAX_POLYGON_POINTS);

    if (!roi1 || !roi2)
    {
        syslog(LOG_INFO, "Failed to initialize ROIs");
        return 1;
    }

    // Initialize tracker on first call
    if (tracker == NULL)
    {
        tracker = init_tracker(100, 0.3f, 30, 3);
    }

    // Load icons at program start
    if (!load_vehicle_icons())
    {
        syslog(LOG_ERR, "Failed to initialize vehicle icons");
        return 1;
    }

    // Initialize incident detection system
    init_incident();

    // Initialize system with LINE_1 having 2 lanes
    LinePoint line1_points[2] = {
        {0.0, 0.0},
        {0.0, 0.0}};
    counting_system = init_counting_system(7, 1, line1_points);
    
    if (counting_system) {
        load_counting_data(counting_system, "/usr/local/packages/enixma_analytic/localdata/counts_backup.json");
        load_chart_data("/usr/local/packages/enixma_analytic/localdata/daily_vehicle_count.json", daily_vehicle_count, 24);
        load_chart_data("/usr/local/packages/enixma_analytic/localdata/weekly_vehicle_count.json", weekly_vehicle_count, 7);

        load_chart_data_double("/usr/local/packages/enixma_analytic/localdata/daily_vehicle_pcu.json", daily_vehicle_pcu, 24);
        load_chart_data_double("/usr/local/packages/enixma_analytic/localdata/weekly_vehicle_pcu.json", weekly_vehicle_pcu, 7);

        load_chart_data_double("/usr/local/packages/enixma_analytic/localdata/daily_average_speed.json", daily_average_speed, 24);
        load_chart_data_double("/usr/local/packages/enixma_analytic/localdata/weekly_average_speed.json", weekly_average_speed, 7);

        load_image_name("/usr/local/packages/enixma_analytic/localdata/incidentImages.json", incident_images, 10);
        cleanup_incident_images_directory();
    }

    get_parameters();

    // Initialize StopLine event handler
    app_data_stopline = calloc(1, sizeof(AppData_StopLine));
    app_data_stopline->base.event_handler = ax_event_handler_new();
    app_data_stopline->base.event_id = setup_stopline_declaration(app_data_stopline->base.event_handler);

    // Initialize Counting event handler
    app_data_counting = calloc(1, sizeof(AppData_Counting));
    app_data_counting->base.event_handler = ax_event_handler_new();
    app_data_counting->base.event_id = setup_counting_declaration(app_data_counting->base.event_handler);

    // Initialize Incidents event handler
    app_data_incidents = calloc(1, sizeof(AppData_Incidents));
    app_data_incidents->base.event_handler = ax_event_handler_new();
    app_data_incidents->base.event_id = setup_incidents_declaration(app_data_incidents->base.event_handler);

    // Start animation timer
    animation_timer = g_timeout_add(1, process_frame, &context);

    // Enter main loop
    g_main_loop_run(main_loop);

    // Destroy the overlay
    axoverlay_destroy_overlay(overlay_id, &overlay_error);
    if (overlay_error != NULL)
    {
        syslog(LOG_ERR, "Failed to destroy first overlay: %s", overlay_error->message);
        g_error_free(overlay_error);
        return 1;
    }
    axoverlay_destroy_overlay(overlay_id_text, &overlay_error_text);
    if (overlay_error_text != NULL)
    {
        syslog(LOG_ERR, "Failed to destroy second overlay: %s", overlay_error_text->message);
        g_error_free(overlay_error_text);
        return 1;
    }

    // Release library resources
    axoverlay_cleanup();

    // Release the animation timer
    g_source_remove(animation_timer);

    // Cleanup event handler
    free_app_data(app_data_stopline, 1);
    free_app_data(app_data_counting, 2);
    free_app_data(app_data_incidents, 3);

    // Release main loop
    g_main_loop_unref(main_loop);

    // Cleanup
    free_polygon(roi1);
    free_polygon(roi2);
    free_tracker(tracker);
    free_counting_system(counting_system);
    cleanup_vehicle_icons();

    syslog(LOG_INFO, "Stop streaming video from VDO");
    if (!stopFrameFetch(context.providers.sdImageProvider))
    {
        goto end;
    }

    fcgi_running = 0;                // Signal FastCGI thread to stop
    pthread_join(fcgi_thread, NULL); // Wait for FastCGI thread to finish

    ret = true;

end:
    if (context.providers.sdImageProvider)
    {
        destroyImgProvider(context.providers.sdImageProvider);
    }
    if (context.providers.hdImageProvider)
    {
        destroyImgProvider(context.providers.hdImageProvider);
    }
    // Only the model handle is released here. We count on larod service to
    // release the privately loaded model when the session is disconnected in
    // larodDisconnect().
    larodDestroyMap(&ppMap);
    larodDestroyMap(&cropMap);
    larodDestroyMap(&ppMapHD);
    larodDestroyModel(&ppModel);
    larodDestroyModel(&ppModelHD);
    larodDestroyModel(&model);
    if (context.larod.conn)
    {
        larodDisconnect(&context.larod.conn, NULL);
    }
    if (larodModelFd >= 0)
    {
        close(larodModelFd);
    }
    if (larodInputAddr != MAP_FAILED)
    {
        munmap(larodInputAddr, context.args.inputWidth * context.args.inputHeight * CHANNELS);
    }
    if (larodInputFd >= 0)
    {
        close(larodInputFd);
    }
    if (context.addresses.ppInputAddr != MAP_FAILED)
    {
        munmap(context.addresses.ppInputAddr, context.args.inputWidth * context.args.inputHeight * CHANNELS);
    }
    if (ppInputFd >= 0)
    {
        close(ppInputFd);
    }
    if (context.addresses.ppInputAddrHD != MAP_FAILED)
    {
        munmap(context.addresses.ppInputAddrHD, context.resolution.widthFrameHD * context.resolution.heightFrameHD * CHANNELS / 2);
    }
    if (ppInputFdHD >= 0)
    {
        close(ppInputFdHD);
    }
    if (context.addresses.ppOutputAddrHD != MAP_FAILED)
    {
        munmap(context.addresses.ppOutputAddrHD, context.resolution.widthFrameHD * context.resolution.heightFrameHD * CHANNELS);
    }
    if (ppOutputFdHD >= 0)
    {
        close(ppOutputFdHD);
    }
    if (cropAddr != MAP_FAILED)
    {
        munmap(cropAddr, context.resolution.widthFrameHD * context.resolution.heightFrameHD * CHANNELS);
    }
    if (context.larod.larodOutput1Addr != MAP_FAILED)
    {
        munmap(context.larod.larodOutput1Addr, TENSOR1SIZE);
    }

    if (context.larod.larodOutput2Addr != MAP_FAILED)
    {
        munmap(context.larod.larodOutput2Addr, TENSOR2SIZE);
    }

    if (context.larod.larodOutput3Addr != MAP_FAILED)
    {
        munmap(context.larod.larodOutput3Addr, TENSOR3SIZE);
    }

    if (context.larod.larodOutput4Addr != MAP_FAILED)
    {
        munmap(context.larod.larodOutput4Addr, TENSOR4SIZE);
    }

    if (context.larod.larodOutput1Fd >= 0)
    {
        close(context.larod.larodOutput1Fd);
    }

    if (context.larod.larodOutput2Fd >= 0)
    {
        close(context.larod.larodOutput2Fd);
    }

    if (context.larod.larodOutput3Fd >= 0)
    {
        close(context.larod.larodOutput3Fd);
    }

    if (context.larod.larodOutput4Fd >= 0)
    {
        close(context.larod.larodOutput4Fd);
    }
    larodDestroyJobRequest(&context.larod.ppReq);
    larodDestroyJobRequest(&context.larod.ppReqHD);
    larodDestroyJobRequest(&context.larod.infReq);
    larodDestroyTensors(context.larod.conn, &context.larod.inputTensors, context.larod.numInputs, &context.larod.error);
    larodDestroyTensors(context.larod.conn, &context.larod.outputTensors, context.larod.numOutputs, &context.larod.error);
    larodClearError(&context.larod.error);

    if (context.label.labels)
    {
        freeLabels(context.label.labels, context.label.labelFileData);
    }

earlyend:
    syslog(LOG_INFO, "Exit %s", argv[0]);
    closelog(); // Add this line to properly close the syslog connection
    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}