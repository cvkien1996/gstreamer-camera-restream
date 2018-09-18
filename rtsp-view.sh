#!/bin/bash

# debug flag
debugOn=""
#debugOn="-v"

#--------------------------------------------------------------------------------------------

echo "--- VIEW RTSP SOURCE WITH JPEG OR H264 ENCODING ---"

echo "Input rtsp source address (default: rtsp://127.0.0.1:8555/test) : "
read address

if [ -z "$address" ]
then
    address="rtsp://127.0.0.1:8555/test"
    echo "Use default address: $address" 
fi

echo "Choose encode source : "
echo "1. H264"
echo "2. JPEG"
read choice

if [ $choice -eq 1 ]
then
    gst-launch-1.0 $debugOn rtspsrc location=$address latency=10 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! ximagesink
elif [ $choice -eq 2 ]
then
    gst-launch-1.0 $debugOn rtspsrc location=$address latency=10 ! rtpjpegdepay ! jpegparse ! jpegdec ! videoconvert ! ximagesink
else
    echo "Invalid choice"
fi
