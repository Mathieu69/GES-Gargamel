/* GStreamer Editing Services
 * Copyright (C) 2010 Edward Hervey <bilboed@bilboed.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <ges/ges.h>
#include <gst/pbutils/encoding-profile.h>
#include <regex.h>

/* GLOBAL VARIABLE */
static guint repeat = 0;
static GESTimelinePipeline *pipeline = NULL;
static gboolean seenerrors = FALSE;

void load_project (gchar * uri);

static gchar *
ensure_uri (gchar * location)
{
  gchar *res;
  gchar *path;

  if (gst_uri_is_valid (location))
    return g_strdup (location);

  if (!g_path_is_absolute (location)) {
    gchar *cur_dir;
    cur_dir = g_get_current_dir ();
    path = g_build_filename (cur_dir, location, NULL);
    g_free (cur_dir);
  } else
    path = g_strdup (location);

  res = g_filename_to_uri (path, NULL, NULL);
  g_free (path);

  return res;
}

static gboolean
thumbnail_cb (gpointer pipeline)
{
  static int i = 0;
  GESTimelinePipeline *p = (GESTimelinePipeline *) pipeline;
  gchar *filename;
  gboolean res;

  filename = g_strdup_printf ("thumbnail%d.jpg", i++);

  res = ges_timeline_pipeline_save_thumbnail (p, -1, -1,
      (gchar *) "image/jpeg", filename);

  g_free (filename);

  return res;
}

static gboolean
check_path (char *path)
{
  FILE *fp = fopen (path, "r");
  if (fp) {
    fclose (fp);
    return TRUE;
  }

  return FALSE;
}

static gboolean
check_time (char *time)
{
  static regex_t re;
  static gboolean compiled = FALSE;

  if (!compiled) {
    compiled = TRUE;
    if (regcomp (&re, "^[0-9]+(.[0-9]+)?$", REG_EXTENDED | REG_NOSUB))
      return FALSE;
  }

  if (!regexec (&re, time, (size_t) 0, NULL, 0))
    return TRUE;
  return FALSE;
}

static guint64
str_to_time (char *time)
{
  if (check_time (time)) {
    return (guint64) (atof (time) * GST_SECOND);
  }
  GST_ERROR ("%s not a valid time", time);
  return 0;
}

static GstEncodingProfile *
make_encoding_profile (gchar * audio, gchar * video, gchar * video_restriction,
    gchar * audio_preset, gchar * video_preset, gchar * container)
{
  GstEncodingContainerProfile *profile;
  GstEncodingProfile *stream;
  GstCaps *caps;

  caps = gst_caps_from_string (container);
  profile =
      gst_encoding_container_profile_new ((gchar *) "ges-test4", NULL, caps,
      NULL);
  gst_caps_unref (caps);

  caps = gst_caps_from_string (audio);
  stream = (GstEncodingProfile *)
      gst_encoding_audio_profile_new (caps, audio_preset, NULL, 0);
  gst_encoding_container_profile_add_profile (profile, stream);
  gst_caps_unref (caps);

  caps = gst_caps_from_string (video);
  stream = (GstEncodingProfile *)
      gst_encoding_video_profile_new (caps, video_preset, NULL, 0);
  if (video_restriction)
    gst_encoding_profile_set_restriction (stream,
        gst_caps_from_string (video_restriction));
  gst_encoding_container_profile_add_profile (profile, stream);
  gst_caps_unref (caps);

  return (GstEncodingProfile *) profile;
}

static GESTimeline *
create_timeline (int nbargs, gchar ** argv)
{
  GESTimelineLayer *layer;
  GESTrack *tracka, *trackv;
  GESTimeline *timeline;
  guint i;

  timeline = ges_timeline_new ();

  tracka = ges_track_audio_raw_new ();
  trackv = ges_track_video_raw_new ();

  /* We are only going to be doing one layer of timeline objects */
  layer = (GESTimelineLayer *) ges_simple_timeline_layer_new ();

  /* Add the tracks and the layer to the timeline */
  if (!ges_timeline_add_layer (timeline, layer) ||
      !ges_timeline_add_track (timeline, tracka) ||
      !ges_timeline_add_track (timeline, trackv))
    goto build_failure;

  /* Here we've finished initializing our timeline, we're 
   * ready to start using it... by solely working with the layer !*/

  for (i = 0; i < nbargs / 3; i++) {
    GESTimelineObject *obj;

    char *source = argv[i * 3];
    char *arg0 = argv[(i * 3) + 1];
    guint64 duration = str_to_time (argv[(i * 3) + 2]);

    if (!g_strcmp0 ("+pattern", source)) {
      obj = GES_TIMELINE_OBJECT (ges_timeline_test_source_new_for_nick (arg0));
      if (!obj) {
        g_error ("%s is an invalid pattern name!\n", arg0);
        goto build_failure;
      }

      g_object_set (G_OBJECT (obj), "duration", duration, NULL);

      g_printf ("Adding <pattern:%s> duration %" GST_TIME_FORMAT "\n", arg0,
          GST_TIME_ARGS (duration));
    }

    else if (!g_strcmp0 ("+transition", source)) {
      if (duration <= 0) {
        g_error ("durations must be greater than 0");
        goto build_failure;
      }

      obj =
          GES_TIMELINE_OBJECT (ges_timeline_standard_transition_new_for_nick
          (arg0));

      if (!obj) {
        g_error ("invalid transition type\n");
        goto build_failure;
      }

      g_object_set (G_OBJECT (obj), "duration", duration, NULL);

      g_printf ("Adding <transition:%s> duration %" GST_TIME_FORMAT "\n", arg0,
          GST_TIME_ARGS (duration));

    }

    else if (!g_strcmp0 ("+title", source)) {
      obj = GES_TIMELINE_OBJECT (ges_timeline_title_source_new ());

      g_object_set (obj, "duration", duration, "text", arg0, NULL);

      g_printf ("Adding <title:%s> duration %" GST_TIME_FORMAT "\n", arg0,
          GST_TIME_ARGS (duration));
    }

    else {
      gchar *uri;
      guint64 inpoint;

      if (!check_path (source)) {
        g_error ("'%s': could not open path!", source);
        goto build_failure;
      }

      if (!(uri = ensure_uri (source))) {
        GST_ERROR ("couldn't create uri for '%s'", source);
        goto build_failure;
      }

      inpoint = str_to_time (argv[i * 3 + 1]);
      obj = GES_TIMELINE_OBJECT (ges_timeline_filesource_new (uri));
      g_object_set (obj,
          "in-point", (guint64) inpoint, "duration", (guint64) duration, NULL);

      g_printf ("Adding clip %s inpoint:%" GST_TIME_FORMAT " duration:%"
          GST_TIME_FORMAT "\n", uri, GST_TIME_ARGS (inpoint),
          GST_TIME_ARGS (duration));

      g_free (uri);
    }

    /* Since we're using a GESSimpleTimelineLayer, objects will be automatically
     * appended to the end of the layer */
    ges_timeline_layer_add_object (layer, obj);
  }

  return timeline;

build_failure:
  {
    g_object_unref (timeline);
    return NULL;
  }
}

static GESTimelinePipeline *
create_pipeline (gchar * load_path, gchar * save_path, int argc, char **argv)
{
  GESTimelinePipeline *pipeline = NULL;
  GESTimeline *timeline = NULL;

  /* Timeline creation */
  if (load_path) {
    gchar *uri;

    g_printf ("Loading project from : %s\n", load_path);

    if (!(uri = ensure_uri (load_path))) {
      g_error ("couldn't create uri for '%s'", load_path);
      goto failure;
    }
    g_printf ("reading from '%s' (arguments ignored)\n", load_path);
    if (!(timeline = ges_timeline_new_from_uri (uri))) {
      g_error ("failed to create timeline from file '%s'", load_path);
      goto failure;
    }
    g_printf ("loaded project successfully\n");
    g_free (uri);
  } else
    /* Normal timeline creation */
  if (!(timeline = create_timeline (argc, argv)))
    goto failure;

  /* save project if path is given. we do this now in case GES crashes or
   * hangs during playback. */
  if (save_path) {
    gchar *uri;
    if (!(uri = ensure_uri (save_path))) {
      g_error ("couldn't create uri for '%s", save_path);
      goto failure;
    }
    ges_timeline_save_to_uri (timeline, uri);
    g_free (uri);
  }

  /* In order to view our timeline, let's grab a convenience pipeline to put
   * our timeline in. */
  pipeline = ges_timeline_pipeline_new ();

  /* Add the timeline to that pipeline */
  if (!ges_timeline_pipeline_add_timeline (pipeline, timeline))
    goto failure;

  return pipeline;

failure:
  {
    if (timeline)
      g_object_unref (timeline);
    if (pipeline)
      g_object_unref (pipeline);
    return NULL;
  }
}

static void
bus_message_cb (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      gst_message_parse_error (message, &err, &dbg_info);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (message->src), err->message);
      g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      seenerrors = TRUE;
      g_main_loop_quit (mainloop);
      break;
    }
    case GST_MESSAGE_EOS:
      if (repeat > 0) {
        g_printerr ("Looping again\n");
        if (!gst_element_seek_simple (GST_ELEMENT (pipeline), GST_FORMAT_TIME,
                GST_SEEK_FLAG_FLUSH, 0))
          g_printerr ("seeking failed\n");
        else
          g_printerr ("seeking succeeded\n");
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
        g_printerr ("Looping set\n");
        repeat -= 1;
      } else {
        g_printerr ("Done\n");
        g_main_loop_quit (mainloop);
      }
      break;
    default:
      break;
  }
}

static void
print_enum (GType enum_type)
{
  GEnumClass *enum_class = G_ENUM_CLASS (g_type_class_ref (enum_type));
  guint i;

  GST_ERROR ("%d", enum_class->n_values);

  for (i = 0; i < enum_class->n_values; i++) {
    g_printf ("%s\n", enum_class->values[i].value_nick);
  }

  g_type_class_unref (enum_class);
}

static void
print_transition_list (void)
{
  print_enum (GES_VIDEO_STANDARD_TRANSITION_TYPE_TYPE);
}

static void
print_pattern_list (void)
{
  print_enum (GES_VIDEO_TEST_PATTERN_TYPE);
}

void
load_project (gchar * uri)
{
  GESFormatter *formatter;
  GESTimeline *timeline;
  GMainLoop *mainloop;
  GESTimelinePipeline *pipeline;
  GstBus *bus;
  gchar *container = (gchar *) "video/x-msvideo";
  gchar *audio = (gchar *) "audio/mpeg,mpegversion=4";
  gchar *video = (gchar *) "video/x-msmpeg,msmpegversion=43";
  gchar *video_restriction = (gchar *) "ANY";
  gchar *outputuri = (gchar *) "file:///media/320/pute.avi";

  formatter = GES_FORMATTER (ges_pitivi_formatter_new ());
  timeline = ges_timeline_new ();
  pipeline = ges_timeline_pipeline_new ();
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  mainloop = g_main_loop_new (NULL, FALSE);

  ges_timeline_pipeline_add_timeline (pipeline, timeline);
  ges_formatter_load_from_uri (formatter, timeline, uri);
  //ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);
  printf ("guh ??\n");
  ges_timeline_pipeline_set_render_settings (pipeline, outputuri,
      make_encoding_profile (audio, video, video_restriction, container));
  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_SMART_RENDER);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  printf ("playing ...\n");
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);
  g_main_loop_run (mainloop);
}

int
main (int argc, gchar ** argv)
{
  GError *err = NULL;
  gchar *outputuri = NULL;
  gchar *container = (gchar *) "application/ogg";
  gchar *audio = (gchar *) "audio/x-vorbis";
  gchar *video = (gchar *) "video/x-theora";
  gchar *video_restriction = (gchar *) "ANY";
  gchar *audio_preset = NULL;
  gchar *video_preset = NULL;
  static gboolean render = FALSE;
  static gboolean smartrender = FALSE;
  static gboolean list_transitions = FALSE;
  static gboolean list_patterns = FALSE;
  static gdouble thumbinterval = 0;
  gchar *save_path = NULL;
  gchar *load_path = NULL;
  gchar *project_path = NULL;
  GOptionEntry options[] = {
    {"thumbnail", 'm', 0.0, G_OPTION_ARG_DOUBLE, &thumbinterval,
        "Take thumbnails every n seconds (saved in current directory)", "N"},
    {"render", 'r', 0, G_OPTION_ARG_NONE, &render,
        "Render to outputuri", NULL},
    {"smartrender", 's', 0, G_OPTION_ARG_NONE, &smartrender,
        "Render to outputuri, and avoid decoding/reencoding", NULL},
    {"outputuri", 'o', 0, G_OPTION_ARG_STRING, &outputuri,
        "URI to encode to", "URI (<protocol>://<location>)"},
    {"format", 'f', 0, G_OPTION_ARG_STRING, &container,
        "Container format", "<GstCaps>"},
    {"vformat", 'v', 0, G_OPTION_ARG_STRING, &video,
        "Video format", "<GstCaps>"},
    {"aformat", 'a', 0, G_OPTION_ARG_STRING, &audio,
        "Audio format", "<GstCaps>"},
    {"vrestriction", 'x', 0, G_OPTION_ARG_STRING, &video_restriction,
        "Video restriction", "<GstCaps>"},
    {"apreset", 0, 0, G_OPTION_ARG_STRING, &audio_preset,
        "Encoding audio profile preset", "<GstPresetName>"},
    {"vpreset", 0, 0, G_OPTION_ARG_STRING, &video_preset,
        "Encoding video profile preset", "<GstPresetName>"},
    {"repeat", 'l', 0, G_OPTION_ARG_INT, &repeat,
        "Number of time to repeat timeline", NULL},
    {"list-transitions", 't', 0, G_OPTION_ARG_NONE, &list_transitions,
        "List valid transition types and exit", NULL},
    {"list-patterns", 'p', 0, G_OPTION_ARG_NONE, &list_patterns,
        "List patterns and exit", NULL},
    {"save", 'z', 0, G_OPTION_ARG_STRING, &save_path,
        "Save project to file before rendering", "<path>"},
    {"load", 'q', 0, G_OPTION_ARG_STRING, &load_path,
        "Load project from file before rendering", "<path>"},
    {"load-xptv", 'y', 0, G_OPTION_ARG_STRING, &project_path,
        "Load xptv project from file for previewing", "<path>"},
    {NULL}
  };
  GOptionContext *ctx;
  GMainLoop *mainloop;
  GstBus *bus;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx = g_option_context_new ("- plays or renders a timeline.");
  g_option_context_set_summary (ctx,
      "ges-launch renders a timeline, which can be specified on the commandline,\n"
      "or loaded from a file using the -q option.\n\n"
      "A timeline is a list of files, patterns, and transitions to be rendered\n"
      "one after the other. Files and Patterns provide video and audio as the\n"
      "primary input, and transitions animate between the end of one file/pattern\n"
      "and the beginning of a new one. Hence, transitions can only be listed\n"
      "in between patterns or files.\n\n"
      "A file is a triplet of filename, inpoint (in seconds) and\n"
      "duration (in seconds). If the duration is 0, the full file length is used.\n\n"
      "Patterns and transitions are triplets that begin with either \"+pattern\"\n"
      "or \"+transition\", followed by a <type> and duration (in seconds, must be\n"
      "greater than 0)\n\n"
      "Durations in all cases can be fractions of a second.\n\n"
      "Example:\n"
      "ges-launch file1.avi 0 45 +transition crossfade 3.5 file2.avi 0 0");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_printerr ("Error initializing: %s\n", err->message);
    g_option_context_free (ctx);
    exit (1);
  }

  /* Initialize the GStreamer Editing Services */
  if (!ges_init ()) {
    g_printerr ("Error initializing GES\n");
    exit (1);
  }

  if (list_transitions) {
    print_transition_list ();
    exit (0);
  }

  if (list_patterns) {
    print_pattern_list ();
    exit (0);
  }

  if (project_path) {
    load_project (project_path);
    exit (0);
  }

  if (((!load_path && (argc < 4))) || (outputuri && (!render && !smartrender))) {
    g_printf ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    g_option_context_free (ctx);
    exit (1);
  }

  g_option_context_free (ctx);

  /* Create the pipeline */
  pipeline = create_pipeline (load_path, save_path, argc - 1, argv + 1);
  if (!pipeline)
    exit (1);

  /* Setup profile/encoding if needed */
  if (render || smartrender) {
    GstEncodingProfile *prof;

    prof = make_encoding_profile (audio, video, video_restriction, audio_preset,
        video_preset, container);

    if (!prof ||
        !ges_timeline_pipeline_set_render_settings (pipeline, outputuri, prof)
        || !ges_timeline_pipeline_set_mode (pipeline,
            smartrender ? TIMELINE_MODE_SMART_RENDER : TIMELINE_MODE_RENDER))
      exit (1);

    g_free (outputuri);
    gst_encoding_profile_unref (prof);
  } else {
    ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW);
  }

  /* Play the pipeline */
  mainloop = g_main_loop_new (NULL, FALSE);

  if (thumbinterval != 0.0) {
    g_printf ("thumbnailing every %f seconds\n", thumbinterval);
    g_timeout_add (1000 * thumbinterval, thumbnail_cb, pipeline);
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message_cb), mainloop);

  if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    g_error ("Failed to start the encoding\n");
    return 1;
  }
  g_main_loop_run (mainloop);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);

  gst_object_unref (pipeline);

  return (int) seenerrors;
}
