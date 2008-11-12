#include <gst/gst.h>

#define VIDEO_ENCODER "h264"
#define AUDIO_ENCODER "faac"
#define MUXER "mpegtsmux"

struct encoding_presets {
  gchar *name;
  gchar *video_encoder;
  gchar *audio_encoder;
  gchar *muxer;
};

const struct encoding_presets {
  { "ps3", "h264", "faac", "mpegtsmux" },
  { "iphone", "h264", "faac", "qtmux" }
};


GST_DEBUG_CATEGORY_STATIC (ps3_convert_debug);
#define GST_CAT_DEFAULT ps3_convert_debug

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

GstElement *pipeline, *play_video, *play_audio;

static void
cb_newpad (GstElement *decodebin,
	   GstPad     *pad,
	   gboolean    last,
	   gpointer    data)
{
  GstCaps *caps;
  GstStructure *str;
  GstPad *videopad, *audiopad;

  /* only link once */
  videopad = gst_element_get_static_pad (play_video, "sink");
  audiopad = gst_element_get_static_pad (play_audio, "sink");
  if (GST_PAD_IS_LINKED (videopad) && GST_PAD_IS_LINKED (audiopad)) {
    g_object_unref (videopad);
    return;
  }

  /* check media type */
  caps = gst_pad_get_caps (pad);
  str = gst_caps_get_structure (caps, 0);
  if (g_strrstr (gst_structure_get_name (str), "video"))
  {
    gst_pad_link (pad, videopad);
  }
  else if (g_strrstr (gst_structure_get_name (str), "audio"))
  {
    gst_pad_link (pad, audiopad);
  }
  else
  {
    gst_caps_unref (caps);
    gst_object_unref (videopad);
    return;
  }
  gst_caps_unref (caps);
}

gint
play_content_from (GstElement *video, GstElement *audio)
{
//  GstElement *play_video, *play_audio;
  GError *error = NULL;

  /* create video output */
  play_video = gst_bin_new ("audiobin");
  GstElement *play_video_pipe = gst_parse_launch ("ffmpegcolorspace name=videosink ! osxvideosink", &error);

  if(error != NULL)
  {
    GST_LOG ("Error: parse error\n");
    exit(1);
  }

  GstPad *videoplaysinkpad = gst_element_get_static_pad (play_video_pipe, "videosink");
  gst_bin_add (GST_BIN (play_video), play_video_pipe);

  gst_element_add_pad (play_video, 
      gst_ghost_pad_new ("sink", videoplaysinkpad));
  gst_object_unref (videoplaysinkpad);

  gst_bin_add (GST_BIN (pipeline), play_video);

  /*
  play_video = gst_bin_new ("playvideobin");
  {
    GstElement *colorspace = gst_element_factory_make ("ffmpegcolorspace", "colorspace");
    GstPad *videoplaysinkpad = gst_element_get_static_pad (colorspace, "sink");

    GstElement *sink = gst_element_factory_make ("osxvideosink", "playvideosink");
    gst_bin_add_many (GST_BIN (play_video), colorspace, sink, NULL);
    gst_element_link (colorspace, sink);

    gst_element_add_pad (play_video,
        gst_ghost_pad_new ("sink", videoplaysinkpad));
    gst_object_unref (videoplaysinkpad);
    gst_bin_add (GST_BIN (pipeline), play_video);
  }
  */
  
  /* create audio output */
  {
    play_audio = gst_bin_new ("audiobin");
    GstElement *conv = gst_element_factory_make ("audioconvert", "aconv");
    GstPad *audiopad = gst_element_get_static_pad (conv, "sink");
    GstElement *sink = gst_element_factory_make ("autoaudiosink", "sink");
    gst_bin_add_many (GST_BIN (play_audio), conv, sink, NULL);
    gst_element_link (conv, sink);
    gst_element_add_pad (play_audio,
        gst_ghost_pad_new ("sink", audiopad));
    gst_object_unref (audiopad);

    gst_bin_add (GST_BIN (pipeline), play_audio);
  }
}
  /*
  "autoplug-continue" :  gboolean user_function (GstElement* object,
                                                  GstPad* arg0,
                                                  GstCaps* arg1,
                                                  gpointer user_data);
                                                  */
static gboolean 
check_input_formats (GstElement* decoder, GstPad* pad, GstCaps* caps, gpointer data)
{
  //printf("I AM HERE : %s\n", gst_caps_to_string (caps));


  GST_LOG ("caps here: %s", gst_caps_to_string (caps));

  const GstStructure *str;

  str = gst_caps_get_structure (caps, 0);

  return TRUE;
}

gint
main (gint   argc,
      gchar *argv[])
{
  GMainLoop *loop;
  GstBus *bus;

  /* init GStreamer */
  GST_DEBUG_CATEGORY_INIT (ps3_convert_debug, "ps3_convert", 0, "PS3Convert debug element");
  gst_init (&argc, &argv);


  loop = g_main_loop_new (NULL, FALSE);

  /* make sure we have input */
  if (argc != 2) {
    g_print ("Usage: %s <filename>\n", argv[0]);
    return -1;
  }

  /* setup */
  pipeline = gst_pipeline_new ("pipeline");

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* filesrc location=argv[1] ! decodebin2 */
  GstElement *src = gst_element_factory_make ("filesrc", "source");
  g_object_set (G_OBJECT (src), "location", argv[1], NULL);

  GstElement *decoder = gst_element_factory_make ("decodebin2", "decoderbin");
  g_signal_connect (decoder, "new-decoded-pad", G_CALLBACK (cb_newpad), NULL);
  g_signal_connect (decoder, "autoplug-continue", G_CALLBACK (check_input_formats), NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, decoder, NULL);
  gst_element_link (src, decoder);

  play_content_from (decoder, decoder);


  /* create video output */
  /*
  play_video = gst_bin_new ("playvideobin");
  {
    GstElement *colorspace = gst_element_factory_make ("ffmpegcolorspace", "colorspace");
    GstPad *videoplaysinkpad = gst_element_get_static_pad (colorspace, "sink");

    GstElement *sink = gst_element_factory_make ("osxvideosink", "playvideosink");
    gst_bin_add_many (GST_BIN (play_video), colorspace, sink, NULL);
    gst_element_link (colorspace, sink);

    gst_element_add_pad (play_video,
        gst_ghost_pad_new ("sink", videoplaysinkpad));
    gst_object_unref (videoplaysinkpad);
    gst_bin_add (GST_BIN (pipeline), play_video);
  }
  */
  
  /* create audio output */
  /*
  {
    play_audio = gst_bin_new ("audiobin");
    GstElement *conv = gst_element_factory_make ("audioconvert", "aconv");
    GstPad *audiopad = gst_element_get_static_pad (conv, "sink");
    GstElement *sink = gst_element_factory_make ("autoaudiosink", "sink");
    gst_bin_add_many (GST_BIN (play_audio), conv, sink, NULL);
    gst_element_link (conv, sink);
    gst_element_add_pad (play_audio,
        gst_ghost_pad_new ("sink", audiopad));
    gst_object_unref (audiopad);

    gst_bin_add (GST_BIN (pipeline), play_audio);
  }
  */

  /* run */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}
