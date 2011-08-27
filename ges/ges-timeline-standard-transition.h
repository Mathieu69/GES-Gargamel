/* GStreamer Editing Services
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
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

#ifndef _GES_TIMELINE_STANDARD_TRANSITION
#define _GES_TIMELINE_STANDARD_TRANSITION

#include <glib-object.h>
#include <ges/ges-types.h>
#include <ges/ges-timeline-transition.h>

G_BEGIN_DECLS

#define GES_TYPE_TIMELINE_STANDARD_TRANSITION ges_timeline_standard_transition_get_type()

#define GES_TIMELINE_STANDARD_TRANSITION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_TIMELINE_STANDARD_TRANSITION, GESTimelineStandardTransition))

#define GES_TIMELINE_STANDARD_TRANSITION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_TIMELINE_STANDARD_TRANSITION, GESTimelineStandardTransitionClass))

#define GES_IS_TIMELINE_STANDARD_TRANSITION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_TIMELINE_STANDARD_TRANSITION))

#define GES_IS_TIMELINE_STANDARD_TRANSITION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_TIMELINE_STANDARD_TRANSITION))

#define GES_TIMELINE_STANDARD_TRANSITION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_TIMELINE_STANDARD_TRANSITION, GESTimelineStandardTransitionClass))

typedef struct _GESTimelineStandardTransitionPrivate GESTimelineStandardTransitionPrivate;

/**
 * GESTimelineStandardTransition:
 * @vtype: a #GESVideoStandardTransitionType indicating the type of video transition
 * to apply.
 */
struct _GESTimelineStandardTransition {
  /*< private >*/
  GESTimelineTransition parent;

  /*< public >*/
  GESVideoStandardTransitionType vtype;

  /*< private >*/
  GESTimelineStandardTransitionPrivate *priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

/**
 * GESTimelineStandardTransitionClass:
 *
 */

struct _GESTimelineStandardTransitionClass {
  /*< private >*/
  GESTimelineTransitionClass parent_class;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

GType ges_timeline_standard_transition_get_type (void);

GESTimelineStandardTransition *ges_timeline_standard_transition_new (GESVideoStandardTransitionType vtype);
GESTimelineStandardTransition *ges_timeline_standard_transition_new_for_nick (char *nick);
void ges_timeline_standard_transition_set_video_only (GESTimelineObject * self, gboolean video_only);
void ges_timeline_standard_transition_set_audio_only (GESTimelineObject * self, gboolean audio_only);

G_END_DECLS

#endif /* _GES_TIMELINE_STANDARD_TRANSITION */
