/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <bilboed@bilboed.com>
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

#include <ges/ges.h>
#include <gst/pbutils/encoding-profile.h>

GstEncodingProfile *make_ogg_vorbis_profile (void);

/* This example will take a series of files and create a audio-only timeline
 * containing the first second of each file and render it to the output uri 
 * using ogg/vorbis */

/* make_ogg_vorbis_profile:
 * simple method creating a ogg/vorbis encoding profile. This is here in
 * order not to clutter the main function. */
GstEncodingProfile *
make_ogg_vorbis_profile (void)
{
  GstEncodingContainerProfile *profile;

  profile = gst_encoding_container_profile_new ((gchar *) "ges-test4", NULL,
      gst_caps_new_simple ("application/ogg", NULL), NULL);
  gst_encoding_container_profile_add_profile (profile, (GstEncodingProfile *)
      gst_encoding_audio_profile_new (gst_caps_new_simple ("audio/x-vorbis",
              NULL), NULL, NULL, 1));
  return (GstEncodingProfile *) profile;
}

int
main (int argc, gchar ** argv)
{
  GESTimelinePipeline *pipeline;
  GESTimeline *timeline;
  GESTrack *tracka;
  GESTimelineLayer *layer;
  GList *sources = NULL;
  GMainLoop *mainloop;
  GstEncodingProfile *profile;
  gchar *output_uri;
  guint i;

  if (argc < 3) {
    g_print ("Usage: %s <output uri> <list of audio files>\n", argv[0]);
    return -1;
  }

  /* Initialize GStreamer (this will parse environment variables and commandline
   * arguments. */
  gst_init (&argc, &argv);

  /* Initialize the GStreamer Editing Services */
  ges_init ();

  /* Setup of an audio timeline */

  /* This is our main GESTimeline */
  timeline = ges_timeline_new ();

  tracka = ges_track_audio_raw_new ();

  /* We are only going to be doing one layer of timeline objects */
  layer = (GESTimelineLayer *) ges_simple_timeline_layer_new ();

  /* Add the tracks and the layer to the timeline */
  if (!ges_timeline_add_layer (timeline, layer))
    return -1;
  if (!ges_timeline_add_track (timeline, tracka))
    return -1;

  /* Here we've finished initializing our timeline, we're 
   * ready to start using it... by solely working with the layer !*/

  for (i = 2; i < argc; i++) {
    gchar *uri = g_strdup_printf ("file://%s", argv[i]);
    GESTimelineFileSource *src = ges_timeline_filesource_new (uri);

    g_assert (src);
    g_free (uri);

    g_object_set (src, "duration", GST_SECOND, NULL);
    /* Since we're using a GESSimpleTimelineLayer, objects will be automatically
     * appended to the end of the layer */
    ges_timeline_layer_add_object (layer, (GESTimelineObject *) src);

    sources = g_list_append (sources, src);
  }

  /* In order to view our timeline, let's grab a convenience pipeline to put
   * our timeline in. */
  pipeline = ges_timeline_pipeline_new ();

  /* Add the timeline to that pipeline */
  if (!ges_timeline_pipeline_add_timeline (pipeline, timeline))
    return -1;


  /* RENDER SETTINGS ! */
  /* We set our output URI and rendering setting on the pipeline */
  output_uri = argv[1];
  profile = make_ogg_vorbis_profile ();
  if (!ges_timeline_pipeline_set_render_settings (pipeline, output_uri,
          profile))
    return -1;

  /* We want the pipeline to render (without any preview) */
  if (!ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_RENDER))
    return -1;


  /* The following is standard usage of a GStreamer pipeline (note how you haven't
   * had to care about GStreamer so far ?).
   *
   * We set the pipeline to playing ... */
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  /* .. and we start a GMainLoop. GES **REQUIRES** a GMainLoop to be running in
   * order to function properly ! */
  mainloop = g_main_loop_new (NULL, FALSE);

  /* Simple code to have the mainloop shutdown after 4s */
  /* FIXME : We should wait for EOS ! */
  g_timeout_add_seconds (argc, (GSourceFunc) g_main_loop_quit, mainloop);
  g_main_loop_run (mainloop);

  return 0;
}
