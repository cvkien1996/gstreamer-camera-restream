/**
  **************************************************************************************************
  * @file   main.cpp
  * @author Chung Vinh Kien
  **************************************************************************************************
  *
  * Copyright (C) <?> Chung Vinh Kien cvkien1996@gmail.com. All rights reserved.
  *
  **************************************************************************************************
  */

//@NOTES

//compile: g++ -std=c++11 -Wall main.cpp -o main -lgstapp-1.0 -lgio-2.0 `pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 opencv`

/* rtsp example location:
   rtsp://root:root@127.0.0.1:8555/test 
   rtsps://root:root@127.0.0.1:8555/test
   rtsp://127.0.0.1:8555/test
**/

/* view stream
  JPEGenc: gst-launch-1.0 -v rtspsrc location=rtsp://127.0.0.1:8555/test latency=10 ! rtpjpegdepay ! jpegparse ! jpegdec ! videoconvert ! ximagesink
   H264enc: gst-launch-1.0 -v rtspsrc location=rtsp://127.0.0.1:8555/test latency=10 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! ximagesink
**/

/* Includes --------------------------------------------------------------------------------------*/

/* Standard lib */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mutex>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

/* Glib */
#include <gio/gio.h>

/* GStreamer */
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtsp-server/rtsp-server.h>

/* OpenCV */
#include "opencv2/opencv.hpp"

/* Namespace -------------------------------------------------------------------------------------*/

using namespace std;
using namespace cv;

/* Private macros --------------------------------------------------------------------------------*/

/* Choose source */
//#define _RTSPSRC_

/* Choose Encoder */
//#define _JPEGENC_ 
#define _x264ENC_
//#define _OMXH264ENC_

/* define username and password */
#define MYGST_USERNAME        "root"
#define MYGST_PASSWORD        "root"

/* define opened port */
#define MYGST_LOCALPORT       "9555"
/* define stream path */
#define MYGST_STREAMPATH      "/test"

/* Private data types ----------------------------------------------------------------------------*/

/* Private variables -----------------------------------------------------------------------------*/

static const int g_frame_w      = 640;  /* frame width */
static const int g_frame_h      = 480;  /* frame height */
static const int g_frame_rate   = 30;   /* fps */

/* Frame stored in appsink and get in appsrc */
static Mat g_frameStored;     

/* Mutex, guard resource g_frameStored */
static std::mutex g_mtx;

/* Private function prototypes -------------------------------------------------------------------*/

static gchar *
get_local_ipv4addr (void);

static GstFlowReturn
new_preroll (GstAppSink * appsink, gpointer data);

static GstFlowReturn
new_sample (GstAppSink * appsink, gpointer data);

static void 
need_data (GstElement * appsrc, guint unused, GstClockTime * timestamp);

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
                gpointer user_data);

static gboolean
my_bus_callback(GstBus * bus, GstMessage * message, gpointer data);

/* Public function bodies ------------------------------------------------------------------------*/

int 
main (int argc, char * argv[])
{
    char rtsp_src[50] = {0};
    GMainLoop           * loop;
    GstRTSPServer       * server;
    GstRTSPMountPoints  * mounts;
    GstRTSPMediaFactory * factory;

    GError * error = NULL;

    /* Init loop */
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

#ifdef _RTSPSRC_
    /* Get input source */
    g_print("Input rtsp source address: ");
    fgets(rtsp_src, 50, stdin);
#endif

    gchar * descr = g_strdup_printf(
#ifdef _RTSPSRC_
                        "rtspsrc location=%s latency=10 ! rtph264depay ! avdec_h264 ! "
#else
                        "videotestsrc ! "
#endif
                        "videoconvert ! "
                        "capsfilter caps=video/x-raw,format=RGB,width=%d,height=%d ! "
                        "appsink name=sink sync=true"
#ifdef _RTSPSRC_
                        , rtsp_src
#endif
                        , g_frame_w, g_frame_h);

    GstElement * pipeline = gst_parse_launch(descr, &error);

    g_free(descr);

    if (NULL != error)
    {
        g_print("Could not construct pipeline: %s\n", error->message);
        g_error_free(error);
        exit(-1);
    }

    /* Get appsink by name */
    GstElement * sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    /* Config appsink */
    gst_app_sink_set_emit_signals((GstAppSink *)sink, true);
    gst_app_sink_set_drop((GstAppSink *)sink, true);
    gst_app_sink_set_max_buffers((GstAppSink *)sink, 1);
    /* Install callback function */
    GstAppSinkCallbacks callbacks = {NULL, new_preroll, new_sample};
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, NULL, NULL);

    /* Add bus */
    GstBus * bus          = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    /* Install callback function */
    guint    bus_watch_id = gst_bus_add_watch(bus, my_bus_callback, NULL); 
    gst_object_unref(bus);

    /* Set pipeline state */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

    /* Create rtsp server */
    server  = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, MYGST_LOCALPORT);

    mounts  = gst_rtsp_server_get_mount_points(server);
    factory = gst_rtsp_media_factory_new();

    gchar * launch = NULL;

#ifdef _JPEGENC_
    launch = g_strdup_printf(
                            "( appsrc name=src ! "
                            "videoconvert ! "
                            "capsfilter caps=video/x-raw,"
                                        "format=I420,"
                                        "width=%d,height=%d,"
                                        "framerate=%d/1,"
                                        "pixel-aspect-ratio=1/1 ! "
                            "jpegenc ! "
                            "jpegparse ! "
                            "rtpjpegpay name=pay0 pt=96 )"
                            , g_frame_w, g_frame_h, g_frame_rate);
#endif

#ifdef _x264ENC_
    launch = g_strdup_printf(
                            "( appsrc name=src ! "
                            "videoconvert ! "
                            "capsfilter caps=video/x-raw,"
                                        "format=I420,"
                                        "width=%d,height=%d,"
                                        "framerate=%d/1,"
                                        "pixel-aspect-ratio=1/1 ! "
                            "x264enc noise-reduction=10000 "
                                    "tune=zerolatency "
                                    "byte-stream=true "
                                    "threads=32 ! " 
                            "h264parse ! "
                            "rtph264pay name=pay0 pt=96 )"
                            , g_frame_w, g_frame_h, g_frame_rate);
#endif

#ifdef _OMXH264ENC_
    launch = g_strdup_printf(
                            "( appsrc name=src ! "
                            "videoconvert ! "
                            "capsfilter caps=video/x-raw,"
                                        "format=I420,"
                                        "width=%d,height=%d,"
                                        "framerate=%d/1,"
                                        "pixel-aspect-ratio=1/1 ! "
                            "omxh264enc ! "
                            "capsfilter caps=video/x-h264,"
                                "stream-format=byte-stream,"
                                "profile=high,"
                                "control-rate=2,"
                                "target-bitrate=3000000 ! "
                            "h264parse ! " 
                            "rtph264pay name=pay0 pt=96 )"
                            , g_frame_w, g_frame_h, g_frame_rate);
#endif

    /* Set factory */
    gst_rtsp_media_factory_set_launch (factory, launch);
    gst_rtsp_media_factory_set_shared(factory, TRUE);

    g_free(launch);

    /* Install callback function */
    g_signal_connect(factory, "media-configure", (GCallback)media_configure, NULL);
    
    /* Attach the test factory to the /test url */
    gst_rtsp_mount_points_add_factory(mounts, MYGST_STREAMPATH, factory);

    /* Don't need the ref to the mounts anymore */
    g_object_unref(mounts);

    /* Attach the server to the default maincontext */
    gst_rtsp_server_attach(server, NULL);

    /* Start serving */
    gchar * login_username = NULL;
    gchar * login_password = NULL;
    gchar * protocol       = "rtsp";
    gchar * local_ipaddr   = get_local_ipv4addr();

    // g_print ("Stream ready at %s://%s%s%s:%s%s\n", 
    //         protocol, login_username, login_password, local_ipaddr, MYGST_LOCALPORT, MYGST_STREAMPATH);
    g_print("Stream ready at rtsp://127.0.0.1:%s%s\n", MYGST_LOCALPORT, MYGST_STREAMPATH);
    g_free(login_username);
    g_free(login_password);

    /* Main loop */
    g_main_loop_run(loop);

    /* Don't need the ref anymore */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));

    return 0;
}

/* Private function bodies -----------------------------------------------------------------------*/

/* Get local ip */
static gchar *
get_local_ipv4addr (void)
{
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* IP address attached to "eth0" */
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

/* Appsink new_preroll callback function */
static GstFlowReturn
new_preroll (GstAppSink * appsink, gpointer data)
{
    g_print("Got preroll!\n");
    return GST_FLOW_OK;
}

/* Appsink new_sample callback function */
static GstFlowReturn
new_sample (GstAppSink * appsink, gpointer data)
{
    static uint8_t b_showCaps = 1;

    /* Get frame */
    GstSample * sample = gst_app_sink_pull_sample(appsink);
    GstCaps   * caps   = gst_sample_get_caps(sample);
    GstBuffer * buffer = gst_sample_get_buffer(sample);

    /* Read frame and convert to opencv format */
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_READ);

    /* Convert gstreamer data to OpenCV Mat
       could resolve height / width from capsfilter... */
    Mat frame(Size(g_frame_w, g_frame_h), CV_8UC3, (char *)map.data, Mat::AUTO_STEP);

    /* Get frame size */
    int frameSize = map.size;

    g_mtx.lock();
        /* Process image processing here, before copy to g_frameStored */
        frame.copyTo(g_frameStored);
    g_mtx.unlock();
    
    /* Show caps on first frame */
    if (1 == b_showCaps)
    {
        g_print("Caps on first frame: %s\n", gst_caps_to_string(caps));
        /* Example: 
            video/x-raw, 
            width=(int)640, height=(int)480, 
            framerate=(fraction)30/1, 
            pixel-aspect-ratio=(fraction)1/1,
            interlace-mode=(string)progressive, (progressive, interleaved)
            format=(string)RGB 
        */
        b_showCaps = 0;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    frame.release();

    return GST_FLOW_OK;
}

/* Appsrc need_data callback function */
static void 
need_data (GstElement * appsrc, guint unused, GstClockTime * timestamp)
{
    GstBuffer *     buffer;
    guint           buffersize;
    GstFlowReturn   ret;
    GstMapInfo      info;

    Mat frameimage;

    g_mtx.lock();
        g_frameStored.copyTo(frameimage);
    g_mtx.unlock();

    /* Get frame info */
    buffersize = frameimage.cols * frameimage.rows * frameimage.channels();

    /* Convert frame to GstBuffer type */
    buffer = gst_buffer_new_and_alloc(buffersize);

    uchar * IMG_data = frameimage.data;

    if (gst_buffer_map(buffer, &info, (GstMapFlags)GST_MAP_WRITE))
    {
        memcpy(info.data, IMG_data, buffersize);
        gst_buffer_unmap(buffer, &info);
    }
    else
    {
        g_print("Error getting data in appsrc.\n");
    }

    /* Set buffer timestamp */
    GST_BUFFER_PTS(buffer) = (*timestamp);
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, g_frame_rate);
    (*timestamp) += GST_BUFFER_DURATION(buffer);

    /* Appsrc emit push-buffer signal */
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);

    if (ret != GST_FLOW_OK)
    {
        g_print("something wrong in cb_need_data\n");
    }

    gst_buffer_unref(buffer);

    frameimage.release();

}

/* Signal media_configure callback function */
static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
                gpointer user_data)
{
    GstElement * element, * appsrc;
    GstClockTime * timestamp;

    /* Get the element used for providing the streams of the media */
    element = gst_rtsp_media_get_element(media);

    /* Get appsrc by name */
    appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "src");

    /* Config appsrc */
    gst_util_set_object_arg(G_OBJECT(appsrc), "format", "time");
    gst_util_set_object_arg(G_OBJECT(appsrc), "stream-type", "stream");
    gst_util_set_object_arg(G_OBJECT(appsrc), "is-live", "TRUE");

    g_object_set(G_OBJECT(appsrc), "caps",
                gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "RGB",
                                    "width", G_TYPE_INT, g_frame_w,
                                    "height", G_TYPE_INT, g_frame_h,
                                    "framerate", GST_TYPE_FRACTION, g_frame_rate, 1,
                                    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL),
                NULL);

    /* New timestamp */
    timestamp    = g_new0(GstClockTime, 1);
    (*timestamp) = 0;
    /* Make sure the data is freed when the media is gone */
    g_object_set_data_full(G_OBJECT(media), "my-extra-data", timestamp,
                            (GDestroyNotify)g_free);

    /* Install the callback that will be called when a buffer is needed */
    g_signal_connect(appsrc, "need-data", (GCallback)need_data, timestamp);

    gst_object_unref(appsrc);
    gst_object_unref(element);
}

/* Bus callback function */
static gboolean
my_bus_callback(GstBus * bus, GstMessage * message, gpointer data)
{
    g_print("Got %s message\n", GST_MESSAGE_TYPE_NAME(message));

    switch (GST_MESSAGE_TYPE(message))
    {
        case GST_MESSAGE_ERROR:

            GError *err;
            gchar *debug;

            gst_message_parse_error(message, &err, &debug);
            g_print("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);

        break;

        case GST_MESSAGE_EOS:
            /* end-of-stream */
        break;

        default:
            /* unhandled message */
        break;
    }
    /* we want to be notified again the next time there is a message
    * on the bus, so returning TRUE (FALSE means we want to stop watching
    * for messages on the bus and our callback should not be called again)
    */
    return TRUE;
}

/* END OF FILE ************************************************************************************/
