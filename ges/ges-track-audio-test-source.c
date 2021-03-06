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
 * SECTION:ges-track-audio-test-source
 * @short_description: produce a simple test waveform or silence
 * 
 * Outputs a test audio stream using audiotestsrc. The default property values
 * output silence. Useful for testing pipelines, or to fill gaps in an audio
 * track.
 */

#include "ges-internal.h"
#include "ges-track-object.h"
#include "ges-track-audio-test-source.h"

G_DEFINE_TYPE (GESTrackAudioTestSource, ges_track_audio_test_source,
    GES_TYPE_TRACK_SOURCE);

struct _GESTrackAudioTestSourcePrivate
{
  gdouble freq;
  gdouble volume;
};

enum
{
  PROP_0,
};

static void ges_track_audio_test_source_get_property (GObject * object, guint
    property_id, GValue * value, GParamSpec * pspec);

static void ges_track_audio_test_source_set_property (GObject * object, guint
    property_id, const GValue * value, GParamSpec * pspec);

static GstElement *ges_track_audio_test_source_create_element (GESTrackObject *
    self);

static void
ges_track_audio_test_source_class_init (GESTrackAudioTestSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESTrackObjectClass *bg_class = GES_TRACK_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESTrackAudioTestSourcePrivate));

  object_class->get_property = ges_track_audio_test_source_get_property;
  object_class->set_property = ges_track_audio_test_source_set_property;

  bg_class->create_element = ges_track_audio_test_source_create_element;
}

static void
ges_track_audio_test_source_init (GESTrackAudioTestSource * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TRACK_AUDIO_TEST_SOURCE, GESTrackAudioTestSourcePrivate);

  self->priv->freq = 440;
  self->priv->volume = 0;
}

static void
ges_track_audio_test_source_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_track_audio_test_source_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static GstElement *
ges_track_audio_test_source_create_element (GESTrackObject * trksrc)
{
  GESTrackAudioTestSource *self;
  GstElement *ret;

  self = (GESTrackAudioTestSource *) trksrc;
  ret = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (ret, "volume", (gdouble) self->priv->volume, "freq", (gdouble)
      self->priv->freq, NULL);

  return ret;
}

/**
 * ges_track_audio_test_source_set_freq:
 * @self: a #GESTrackAudioTestSource
 * @freq: The frequency you want to apply on @self
 *
 * Lets you set the frequency applied on the track object
 */
void
ges_track_audio_test_source_set_freq (GESTrackAudioTestSource * self,
    gdouble freq)
{
  GstElement *element = ges_track_object_get_element (GES_TRACK_OBJECT (self));

  self->priv->freq = freq;
  if (element)
    g_object_set (element, "freq", (gdouble) freq, NULL);
}

/**
 * ges_track_audio_test_source_set_volume:
 * @self: a #GESTrackAudioTestSource
 * @volume: The volume you want to apply on @self
 *
 * Sets the volume of the test audio signal.
 */
void
ges_track_audio_test_source_set_volume (GESTrackAudioTestSource * self,
    gdouble volume)
{
  GstElement *element = ges_track_object_get_element (GES_TRACK_OBJECT (self));

  self->priv->volume = volume;
  if (element)
    g_object_set (element, "volume", (gdouble) volume, NULL);
}

/**
 * ges_track_audio_test_source_get_freq:
 * @self: a #GESTrackAudioTestSource
 *
 * Get the current frequency of @self.
 *
 * Returns: The current frequency of @self.
 */
double
ges_track_audio_test_source_get_freq (GESTrackAudioTestSource * self)
{
  return self->priv->freq;
}

/**
 * ges_track_audio_test_source_get_volume:
 * @self: a #GESTrackAudioTestSource
 * 
 * Get the current volume of @self.
 *
 * Returns: The current volume of @self
 */
double
ges_track_audio_test_source_get_volume (GESTrackAudioTestSource * self)
{
  return self->priv->volume;
}

/**
 * ges_track_audio_test_source_new:
 *
 * Creates a new #GESTrackAudioTestSource.
 *
 * Returns: The newly created #GESTrackAudioTestSource.
 */
GESTrackAudioTestSource *
ges_track_audio_test_source_new (void)
{
  return g_object_new (GES_TYPE_TRACK_AUDIO_TEST_SOURCE, NULL);
}
