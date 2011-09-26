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

#ifndef __GES_H__
#define __GES_H__
#include <glib.h>
#include <gst/gst.h>

#include <ges/ges-types.h>
#include <ges/ges-enums.h>

#include <ges/ges-timeline.h>
#include <ges/ges-timeline-layer.h>
#include <ges/ges-simple-timeline-layer.h>
#include <ges/ges-timeline-object.h>
#include <ges/ges-timeline-pipeline.h>
#include <ges/ges-timeline-source.h>
#include <ges/ges-timeline-test-source.h>
#include <ges/ges-timeline-title-source.h>
#include <ges/ges-timeline-operation.h>
#include <ges/ges-timeline-effect.h>
#include <ges/ges-timeline-overlay.h>
#include <ges/ges-timeline-text-overlay.h>
#include <ges/ges-timeline-transition.h>
#include <ges/ges-timeline-standard-transition.h>
#include <ges/ges-timeline-parse-launch-effect.h>
#include <ges/ges-custom-timeline-source.h>
#include <ges/ges-timeline-effect.h>
#include <ges/ges-timeline-file-source.h>
#include <ges/ges-screenshot.h>

#include <ges/ges-track.h>
#include <ges/ges-track-object.h>
#include <ges/ges-track-source.h>
#include <ges/ges-track-operation.h>

#include <ges/ges-track-filesource.h>
#include <ges/ges-track-image-source.h>
#include <ges/ges-track-video-test-source.h>
#include <ges/ges-track-audio-test-source.h>
#include <ges/ges-track-title-source.h>
#include <ges/ges-track-text-overlay.h>
#include <ges/ges-track-transition.h>
#include <ges/ges-track-video-transition.h>
#include <ges/ges-track-audio-transition.h>
#include <ges/ges-track-effect.h>
#include <ges/ges-track-parse-launch-effect.h>
#include <ges/ges-formatter.h>
#include <ges/ges-keyfile-formatter.h>
#include <ges/ges-pitivi-formatter.h>
#include <ges/ges-utils.h>

G_BEGIN_DECLS

gboolean ges_init (void);

G_END_DECLS

#endif /* __GES_H__ */
