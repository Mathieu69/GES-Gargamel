/* GStreamer Editing Services
 * Copyright (C) 2009 Brandon Lewis <brandon.lewis@collabora.co.uk>
 *               2009 Nokia Corporation
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

#ifndef _GES_TL_TESTSOURCE
#define _GES_TL_TESTSOURCE

#include <glib-object.h>
#include <ges/ges-enums.h>
#include <ges/ges-types.h>
#include <ges/ges-timeline-source.h>
#include <ges/ges-track.h>

G_BEGIN_DECLS

#define GES_TYPE_TIMELINE_TEST_SOURCE ges_timeline_test_source_get_type()

#define GES_TIMELINE_TEST_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_TIMELINE_TEST_SOURCE, GESTimelineTestSource))

#define GES_TIMELINE_TEST_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_TIMELINE_TEST_SOURCE, GESTimelineTestSourceClass))

#define GES_IS_TIMELINE_TEST_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_TIMELINE_TEST_SOURCE))

#define GES_IS_TIMELINE_TEST_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_TIMELINE_TEST_SOURCE))

#define GES_TIMELINE_TEST_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_TIMELINE_TEST_SOURCE, GESTimelineTestSourceClass))

typedef struct _GESTimelineTestSourcePrivate GESTimelineTestSourcePrivate;

/**
 * GESTimelineTestSource:
 * 
 */

struct _GESTimelineTestSource {

  GESTimelineSource parent;

  /*< private >*/
  GESTimelineTestSourcePrivate *priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

/**
 * GESTimelineTestSourceClass:
 */

struct _GESTimelineTestSourceClass {
  /*< private >*/
  GESTimelineSourceClass parent_class;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

GType ges_timeline_test_source_get_type (void);

void
ges_timeline_test_source_set_mute (GESTimelineTestSource * self, gboolean mute);

void
ges_timeline_test_source_set_vpattern (GESTimelineTestSource * self,
    GESVideoTestPattern vpattern);

void
ges_timeline_test_source_set_frequency (GESTimelineTestSource * self, gdouble freq);

void
ges_timeline_test_source_set_volume (GESTimelineTestSource * self,
    gdouble volume);

GESVideoTestPattern
ges_timeline_test_source_get_vpattern (GESTimelineTestSource * self);

gboolean ges_timeline_test_source_is_muted (GESTimelineTestSource * self);
gdouble ges_timeline_test_source_get_frequency (GESTimelineTestSource * self);
gdouble ges_timeline_test_source_get_volume (GESTimelineTestSource * self);

GESTimelineTestSource* ges_timeline_test_source_new (void);
GESTimelineTestSource* ges_timeline_test_source_new_for_nick(gchar * nick);

G_END_DECLS

#endif /* _GES_TL_TESTSOURCE */

