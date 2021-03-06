/* GStreamer Editing Services
 * Copyright (C) 2010 Brandon Lewis <brandon.lewis@collabora.co.uk>
 *               2010 Nokia Corporation
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

/**
 * SECTION:ges-track-audio-transition
 * @short_description: implements audio crossfade transition
 */

#include "ges-internal.h"
#include "ges-track-object.h"
#include "ges-track-audio-transition.h"

G_DEFINE_TYPE (GESTrackAudioTransition, ges_track_audio_transition,
    GES_TYPE_TRACK_TRANSITION);

struct _GESTrackAudioTransitionPrivate
{
  /* these enable volume interpolation. Unlike video, both inputs are adjusted
   * simultaneously */
  GstController *a_controller;
  GstInterpolationControlSource *a_control_source;

  GstController *b_controller;
  GstInterpolationControlSource *b_control_source;

};

enum
{
  PROP_0,
};


#define fast_element_link(a,b) gst_element_link_pads_full((a),"src",(b),"sink",GST_PAD_LINK_CHECK_NOTHING)

static void
ges_track_audio_transition_duration_changed (GESTrackObject * self, guint64);

static GstElement *ges_track_audio_transition_create_element (GESTrackObject
    * self);

static void ges_track_audio_transition_dispose (GObject * object);

static void ges_track_audio_transition_finalize (GObject * object);

static void ges_track_audio_transition_get_property (GObject * object, guint
    property_id, GValue * value, GParamSpec * pspec);

static void ges_track_audio_transition_set_property (GObject * object, guint
    property_id, const GValue * value, GParamSpec * pspec);

static void
ges_track_audio_transition_class_init (GESTrackAudioTransitionClass * klass)
{
  GObjectClass *object_class;
  GESTrackObjectClass *toclass;

  g_type_class_add_private (klass, sizeof (GESTrackAudioTransitionPrivate));

  object_class = G_OBJECT_CLASS (klass);
  toclass = GES_TRACK_OBJECT_CLASS (klass);

  object_class->get_property = ges_track_audio_transition_get_property;
  object_class->set_property = ges_track_audio_transition_set_property;
  object_class->dispose = ges_track_audio_transition_dispose;
  object_class->finalize = ges_track_audio_transition_finalize;

  toclass->duration_changed = ges_track_audio_transition_duration_changed;

  toclass->create_element = ges_track_audio_transition_create_element;

}

static void
ges_track_audio_transition_init (GESTrackAudioTransition * self)
{

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TRACK_AUDIO_TRANSITION, GESTrackAudioTransitionPrivate);

  self->priv->a_controller = NULL;
  self->priv->a_control_source = NULL;

  self->priv->b_controller = NULL;
  self->priv->b_control_source = NULL;
}

static void
ges_track_audio_transition_dispose (GObject * object)
{
  GESTrackAudioTransition *self;

  self = GES_TRACK_AUDIO_TRANSITION (object);

  if (self->priv->a_controller) {
    g_object_unref (self->priv->a_controller);
    self->priv->a_controller = NULL;
    if (self->priv->a_control_source)
      gst_object_unref (self->priv->a_control_source);
    self->priv->a_control_source = NULL;
  }

  if (self->priv->b_controller) {
    g_object_unref (self->priv->b_controller);
    self->priv->b_controller = NULL;
    if (self->priv->b_control_source)
      gst_object_unref (self->priv->b_control_source);
    self->priv->b_control_source = NULL;
  }

  G_OBJECT_CLASS (ges_track_audio_transition_parent_class)->dispose (object);
}

static void
ges_track_audio_transition_finalize (GObject * object)
{
  G_OBJECT_CLASS (ges_track_audio_transition_parent_class)->finalize (object);
}

static void
ges_track_audio_transition_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_track_audio_transition_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static GObject *
link_element_to_mixer_with_volume (GstBin * bin, GstElement * element,
    GstElement * mixer)
{
  GstElement *volume = gst_element_factory_make ("volume", NULL);
  gst_bin_add (bin, volume);

  if (!fast_element_link (element, volume) ||
      !gst_element_link_pads_full (volume, "src", mixer, "sink%d",
          GST_PAD_LINK_CHECK_NOTHING))
    GST_ERROR_OBJECT (bin, "Error linking volume to mixer");

  return G_OBJECT (volume);
}

static GstElement *
ges_track_audio_transition_create_element (GESTrackObject * object)
{
  GESTrackAudioTransition *self;
  GstElement *topbin, *iconva, *iconvb, *oconv;
  GObject *atarget, *btarget = NULL;
  const gchar *propname = "volume";
  GstElement *mixer = NULL;
  GstPad *sinka_target, *sinkb_target, *src_target, *sinka, *sinkb, *src;
  GstController *acontroller, *bcontroller;
  GstInterpolationControlSource *acontrol_source, *bcontrol_source;

  self = GES_TRACK_AUDIO_TRANSITION (object);


  GST_LOG ("creating an audio bin");

  topbin = gst_bin_new ("transition-bin");
  iconva = gst_element_factory_make ("audioconvert", "tr-aconv-a");
  iconvb = gst_element_factory_make ("audioconvert", "tr-aconv-b");
  oconv = gst_element_factory_make ("audioconvert", "tr-aconv-output");

  gst_bin_add_many (GST_BIN (topbin), iconva, iconvb, oconv, NULL);

  mixer = gst_element_factory_make ("adder", NULL);
  gst_bin_add (GST_BIN (topbin), mixer);

  atarget = link_element_to_mixer_with_volume (GST_BIN (topbin), iconva, mixer);
  btarget = link_element_to_mixer_with_volume (GST_BIN (topbin), iconvb, mixer);

  g_assert (atarget && btarget);

  fast_element_link (mixer, oconv);

  sinka_target = gst_element_get_static_pad (iconva, "sink");
  sinkb_target = gst_element_get_static_pad (iconvb, "sink");
  src_target = gst_element_get_static_pad (oconv, "src");

  sinka = gst_ghost_pad_new ("sinka", sinka_target);
  sinkb = gst_ghost_pad_new ("sinkb", sinkb_target);
  src = gst_ghost_pad_new ("src", src_target);

  gst_element_add_pad (topbin, src);
  gst_element_add_pad (topbin, sinka);
  gst_element_add_pad (topbin, sinkb);

  /* set up interpolation */

  gst_object_unref (sinka_target);
  gst_object_unref (sinkb_target);
  gst_object_unref (src_target);


  //g_object_set(atarget, propname, (gdouble) 0, NULL);
  //g_object_set(btarget, propname, (gdouble) 0, NULL);

  acontroller = gst_object_control_properties (atarget, propname, NULL);
  bcontroller = gst_object_control_properties (btarget, propname, NULL);

  g_assert (acontroller && bcontroller);

  acontrol_source = gst_interpolation_control_source_new ();
  gst_controller_set_control_source (acontroller,
      propname, GST_CONTROL_SOURCE (acontrol_source));
  gst_interpolation_control_source_set_interpolation_mode (acontrol_source,
      GST_INTERPOLATE_LINEAR);

  bcontrol_source = gst_interpolation_control_source_new ();
  gst_controller_set_control_source (bcontroller,
      propname, GST_CONTROL_SOURCE (bcontrol_source));
  gst_interpolation_control_source_set_interpolation_mode (bcontrol_source,
      GST_INTERPOLATE_LINEAR);

  self->priv->a_controller = acontroller;
  self->priv->b_controller = bcontroller;
  self->priv->a_control_source = acontrol_source;
  self->priv->b_control_source = bcontrol_source;

  return topbin;
}

static void
ges_track_audio_transition_duration_changed (GESTrackObject * object,
    guint64 duration)
{
  GESTrackAudioTransition *self;
  GstElement *gnlobj = ges_track_object_get_gnlobject (object);

  GValue zero = { 0, };
  GValue one = { 0, };

  self = GES_TRACK_AUDIO_TRANSITION (object);

  GST_LOG ("updating controller: gnlobj (%p) acontroller(%p) bcontroller(%p)",
      gnlobj, self->priv->a_controller, self->priv->b_controller);

  if (G_UNLIKELY ((!gnlobj || !self->priv->a_control_source ||
              !self->priv->b_control_source)))
    return;

  GST_INFO ("duration: %" G_GUINT64_FORMAT, duration);
  g_value_init (&zero, G_TYPE_DOUBLE);
  g_value_init (&one, G_TYPE_DOUBLE);
  g_value_set_double (&zero, 0.0);
  g_value_set_double (&one, 1.0);

  GST_LOG ("setting values on controller");

  gst_interpolation_control_source_unset_all (self->priv->a_control_source);
  gst_interpolation_control_source_set (self->priv->a_control_source, 0, &one);
  gst_interpolation_control_source_set (self->priv->a_control_source,
      duration, &zero);

  gst_interpolation_control_source_unset_all (self->priv->b_control_source);
  gst_interpolation_control_source_set (self->priv->b_control_source, 0, &zero);
  gst_interpolation_control_source_set (self->priv->b_control_source, duration,
      &one);

  GST_LOG ("done updating controller");
}

/**
 * ges_track_audio_transition_new:
 *
 * Creates a new #GESTrackAudioTransition.
 *
 * Returns: The newly created #GESTrackAudioTransition.
 */
GESTrackAudioTransition *
ges_track_audio_transition_new (void)
{
  return g_object_new (GES_TYPE_TRACK_AUDIO_TRANSITION, NULL);
}
