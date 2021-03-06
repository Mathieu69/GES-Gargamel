/* GStreamer Editing Services
 * Copyright (C) 2010 Brandon Lewis <brandon@alum.berkeley.edu>
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

#include <stdlib.h>
#include <ges/ges.h>
#include <stdlib.h>

typedef struct
{
  int type;
  char *name;
} transition_type;

GESTimelineObject *make_source (char *path, guint64 start, guint64 duration,
    gint priority);

gboolean print_transition_data (GESTimelineObject * tr);

GESTimelinePipeline *make_timeline (char *nick, double tdur, char *patha,
    float adur, char *pathb, float bdur);

GESTimelineObject *
make_source (char *path, guint64 start, guint64 duration, gint priority)
{
  char *uri = g_strdup_printf ("file://%s", path);

  GESTimelineObject *ret =
      GES_TIMELINE_OBJECT (ges_timeline_filesource_new (uri));

  g_object_set (ret,
      "start", (guint64) start,
      "duration", (guint64) duration,
      "priority", (guint32) priority, "in-point", (guint64) 0, NULL);

  g_free (uri);

  return ret;
}

gboolean
print_transition_data (GESTimelineObject * tr)
{
  GESTrackObject *trackobj;
  GstElement *gnlobj;
  guint64 start, duration;
  gint priority;
  char *name;
  GList *trackobjects, *tmp;

  if (!tr)
    return FALSE;

  trackobjects = ges_timeline_object_get_track_objects (tr);

  trackobj = GES_TRACK_OBJECT (trackobjects->data);
  gnlobj = ges_track_object_get_gnlobject (trackobj);

  g_object_get (gnlobj, "start", &start, "duration", &duration,
      "priority", &priority, "name", &name, NULL);
  g_print ("gnlobject for %s: %f %f %d\n", name,
      ((gfloat) start) / GST_SECOND,
      ((gfloat) duration) / GST_SECOND, priority);

  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    g_object_unref (GES_TRACK_OBJECT (tmp->data));
  }

  g_list_free (trackobjects);

  return FALSE;
}

GESTimelinePipeline *
make_timeline (char *nick, double tdur, char *patha, float adur,
    char *pathb, float bdur)
{
  GESTimeline *timeline;
  GESTrack *trackv, *tracka;
  GESTimelineLayer *layer1;
  GESTimelineObject *srca, *srcb;
  GESTimelinePipeline *pipeline;
  guint64 aduration, bduration, tduration, tstart;
  GESTimelineStandardTransition *tr = NULL;

  pipeline = ges_timeline_pipeline_new ();

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);

  timeline = ges_timeline_new ();
  ges_timeline_pipeline_add_timeline (pipeline, timeline);

  trackv = ges_track_video_raw_new ();
  ges_timeline_add_track (timeline, trackv);

  tracka = ges_track_audio_raw_new ();
  ges_timeline_add_track (timeline, tracka);

  layer1 = GES_TIMELINE_LAYER (ges_timeline_layer_new ());
  g_object_set (layer1, "priority", (gint32) 0, NULL);

  if (!ges_timeline_add_layer (timeline, layer1))
    exit (-1);

  aduration = (guint64) (adur * GST_SECOND);
  bduration = (guint64) (bdur * GST_SECOND);
  tduration = (guint64) (tdur * GST_SECOND);
  tstart = aduration - tduration;
  srca = make_source (patha, 0, aduration, 1);
  srcb = make_source (pathb, tstart, bduration, 2);
  ges_timeline_layer_add_object (layer1, srca);
  ges_timeline_layer_add_object (layer1, srcb);
  g_timeout_add_seconds (1, (GSourceFunc) print_transition_data, srca);
  g_timeout_add_seconds (1, (GSourceFunc) print_transition_data, srcb);

  if (tduration != 0) {
    g_print ("creating transition at %" GST_TIME_FORMAT " of %f duration (%"
        GST_TIME_FORMAT ")\n", GST_TIME_ARGS (tstart), tdur,
        GST_TIME_ARGS (tduration));
    if (!(tr = ges_timeline_standard_transition_new_for_nick (nick)))
      g_error ("invalid transition type %s\n", nick);

    g_object_set (tr,
        "start", (guint64) tstart,
        "duration", (guint64) tduration, "in-point", (guint64) 0, NULL);
    ges_timeline_layer_add_object (layer1, GES_TIMELINE_OBJECT (tr));
    g_timeout_add_seconds (1, (GSourceFunc) print_transition_data, tr);
  }

  return pipeline;
}

int
main (int argc, char **argv)
{
  GError *err = NULL;
  GOptionContext *ctx;
  GESTimelinePipeline *pipeline;
  GMainLoop *mainloop;
  gchar *type = (gchar *) "crossfade";
  gdouble adur, bdur, tdur;

  GOptionEntry options[] = {
    {"type", 't', 0, G_OPTION_ARG_STRING, &type,
        "type of transition to create", "<smpte-transition>"},
    {"duration", 'd', 0, G_OPTION_ARG_DOUBLE, &tdur,
        "duration of transition", "seconds"},
    {NULL}
  };

  if (!g_thread_supported ())
    g_thread_init (NULL);

  ctx = g_option_context_new ("- transition between two media files");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing %s\n", err->message);
    exit (1);
  }

  if (argc < 4) {
    g_print ("%s", g_option_context_get_help (ctx, TRUE, NULL));
    exit (0);
  }

  g_option_context_free (ctx);

  ges_init ();


  adur = (gdouble) atof (argv[2]);
  bdur = (gdouble) atof (argv[4]);

  pipeline = make_timeline (type, tdur, argv[1], adur, argv[3], bdur);

  mainloop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_seconds ((adur + bdur) + 1, (GSourceFunc) g_main_loop_quit,
      mainloop);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  g_main_loop_run (mainloop);

  return 0;
}
