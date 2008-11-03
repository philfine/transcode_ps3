#!/bin/bash

FONTSIZE=20

function usage {
  echo "Usage: ${0} <display/gen> <ps3/iphone> <movie_file> <output_file> [subtitles_file] [font_size]"
  exit 1;
}

if [ $# -le 3 ]; then
  usage
fi

case "$1" in
  display)
    DISPLAY=1
    ;;
  gen)
    DISPLAY=0
    ;;
  *)
    usage
    ;;
esac

if [ $# -ge 5 ]; then
  if [ -f $5 ]; then
    ADD_SUBTITLES=1
    SUBTITLES_FILE=$5
  else
    echo "Error: $5 file doesn't exist."
  fi
fi
if [ $# -ge 6 ]; then
  if [[ $6 -ge 1 && $6 -le 40 ]]; then
    FONTSIZE=$6
  else
    echo "Error: font_size should be a number between 5 and 40."
    exit 1
  fi
fi

FORMAT=$2
INPUT_FILE=$3
OUTPUT_FILE=$4


#Scalling part

SCALE="queue"
case "$2" in
  iphone)
    echo "Please enter the number of extra pixels to needed encode video"
    read EXTRA_PIXELS

    if [ ${EXTRA_PIXELS} -ge 0 ]; then
      HALF=`echo "${EXTRA_PIXELS}/2 | bc`
      BOTTOM=`echo "${HALF} + ((${EXTRA_PIXELS}/2)%2) | bc`

      echo "HALF = ${HALF} BOTTOM = ${BOTTOM}"

      SCALE="ffmpegcolorspace ! videoscale ! capsfilter caps=video/x-raw-yuv,width=480 ! videobox top=-${HALF} bottom=-${BOTTOM} ! mixer.sink_0 ${BACKGROUND}"
    else
      echo "Error: Not a valid number".
      exit 1
    fi
    ;;
esac

#Read movie part - also includes scalling

READ_VIDEO="filesrc location=${INPUT_FILE} ! decodebin name=dec_input ! ${SCALE} ! ffmpegcolorspace name=video_input"

READ_SUBTITLES="filesrc location=${SUBTITLES_FILE} ! queue ! subparse ! textoverlay name=overlay font-desc=${FONTSIZE} ! queue name=video_output"
WITH_SUBTITLES="video_input. ! overlay."
WITHOUT_SUBTITLES="video_input. ! queue name=video_output"

# Encoding part

case "$2" in 
  ps3)
    VIDEO_ENCODE="video_output. ! queue ! x264enc bitrate=2000 cabac=true trellis=true pass=0 ! queue ! muxer."
    AUDIO_ENCODE="dec_input. ! audioconvert ! faac profile=1 ! muxer."
    MUXER="ffmux_mp4 name=muxer ! progressreport update-freq=1 ! filesink location=${OUTPUT_FILE}"
    ;;
  iphone)
    VIDEO_ENCODE="video_output. ! x264enc ! muxer."
    AUDIO_ENCODE="dec_input. ! audioconvert ! faac ! queue ! muxer."
    MUXER="qtmux name=muxer ! progressreport update-freq=1 ! filesink location=${OUTPUT_FILE}"
    ;;
  *)
    usage
    ;;
esac

DISPLAY_VIDEO_SINK="video_output. ! ffmpegcolorspace ! queue ! autovideosink"
DISPLAY_AUDIO_SINK="dec_input. ! queue ! autoaudiosink"

cmd="gst-launch-0.10 -v"
cmd="$cmd ${READ_VIDEO}"
if [ ${ADD_SUBTITLES} -eq 1 ]; then
  cmd="$cmd ${READ_SUBTITLES}"
  cmd="$cmd ${WITH_SUBTITLES}"
else
  cmd="$cmd ${WITHOUT_SUBTITLES}"
fi

if [ ${DISPLAY} -eq 1 ]; then
  cmd="$cmd ${DISPLAY_AUDIO_SINK}"
  cmd="$cmd ${DISPLAY_VIDEO_SINK}"
else
  cmd="$cmd ${VIDEO_ENCODE}"
  cmd="$cmd ${AUDIO_ENCODE}"
  cmd="$cmd ${MUXER}"
fi

echo "$cmd"
$cmd

#gst-launch-0.10 -v \
#      filesrc location="$1" ! decodebin name="decode" \
#      filesrc location="$2" ! queue ! subparse ! textoverlay name=overlay font-desc="${FONTSIZE}" ! video_enc. \
#      decode. ! ffmpegcolorspace ! overlay. \
#      ffenc_mpeg4 name=video_enc bitrate=999999 gop-size=24 qmin=2 qmax=31 flags=0x00000010 ! mux. \
#      decode. ! audioconvert ! lame name=enc vbr=0 ! mux. \
#      avimux name=mux ! filesink location="$3" 



#gst-launch-0.10 filesrc location="movie.avi" ! decodebin name="decode" decode. ! queue ! ffmpegcolorspace ! x264enc ! ffmux_mp4 name=mux ! filesink location="output.avi" decode. ! queue ! audioconvert ! faac name=enc ! queue ! mux.

#gst-launch-0.10 -v \
#    filesrc location="$1" ! decodebin name="dec" \
#    dec. ! queue ! ffmpegcolorspace ! overlay. \
#    dec. ! queue ! audioconvert ! audio/x-raw-int,channels=5 ! audioconvert ! faac name=enc ! queue ! mux. \
#    filesrc location="$2" ! queue ! subparse ! textoverlay name=overlay \
#    overlay. ! queue ! x264enc ! mux. \
#    ffmux_mp4 name=mux ! progressreport ! filesink location="$3"
##  qtmux name=mux ! progressreport ! filesink location="$3"

