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

/**
 * SECTION:ges-timeline-text-overlay
 * @short_description: Render text onto another stream in a #GESTimelineLayer
 *
 * Renders text onto the next lower priority stream using textrender.
 */

#include "ges-internal.h"
#include "ges-timeline-text-overlay.h"
#include "ges-track-object.h"
#include "ges-track-text-overlay.h"
#include <string.h>

G_DEFINE_TYPE (GESTimelineTextOverlay, ges_timeline_text_overlay,
    GES_TYPE_TIMELINE_OVERLAY);

#define DEFAULT_PROP_TEXT ""
#define DEFAULT_PROP_FONT_DESC "Serif 36"
#define DEFAULT_PROP_VALIGNMENT GES_TEXT_VALIGN_BASELINE
#define DEFAULT_PROP_HALIGNMENT GES_TEXT_HALIGN_CENTER
#

struct _GESTimelineTextOverlayPrivate
{
  gchar *text;
  gchar *font_desc;
  GESTextHAlign halign;
  GESTextVAlign valign;
};

enum
{
  PROP_0,
  PROP_TEXT,
  PROP_FONT_DESC,
  PROP_HALIGNMENT,
  PROP_VALIGNMENT,
};

static GESTrackObject
    * ges_timeline_text_overlay_create_track_object (GESTimelineObject * obj,
    GESTrack * track);

static void
ges_timeline_text_overlay_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESTimelineTextOverlayPrivate *priv =
      GES_TIMELINE_TEXT_OVERLAY (object)->priv;

  switch (property_id) {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_FONT_DESC:
      g_value_set_string (value, priv->font_desc);
      break;
    case PROP_HALIGNMENT:
      g_value_set_enum (value, priv->halign);
      break;
    case PROP_VALIGNMENT:
      g_value_set_enum (value, priv->valign);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_timeline_text_overlay_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESTimelineTextOverlay *tfs = GES_TIMELINE_TEXT_OVERLAY (object);

  switch (property_id) {
    case PROP_TEXT:
      ges_timeline_text_overlay_set_text (tfs, g_value_get_string (value));
      break;
    case PROP_FONT_DESC:
      ges_timeline_text_overlay_set_font_desc (tfs, g_value_get_string (value));
      break;
    case PROP_HALIGNMENT:
      ges_timeline_text_overlay_set_halign (tfs, g_value_get_enum (value));
      break;
    case PROP_VALIGNMENT:
      ges_timeline_text_overlay_set_valign (tfs, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_timeline_text_overlay_dispose (GObject * object)
{
  GESTimelineTextOverlayPrivate *priv =
      GES_TIMELINE_TEXT_OVERLAY (object)->priv;

  if (priv->text)
    g_free (priv->text);
  if (priv->font_desc)
    g_free (priv->font_desc);

  G_OBJECT_CLASS (ges_timeline_text_overlay_parent_class)->dispose (object);
}

static void
ges_timeline_text_overlay_class_init (GESTimelineTextOverlayClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GESTimelineObjectClass *timobj_class = GES_TIMELINE_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESTimelineTextOverlayPrivate));

  object_class->get_property = ges_timeline_text_overlay_get_property;
  object_class->set_property = ges_timeline_text_overlay_set_property;
  object_class->dispose = ges_timeline_text_overlay_dispose;

  /**
   * GESTimelineTextOverlay:text
   *
   * The text to diplay
   */

  g_object_class_install_property (object_class, PROP_TEXT,
      g_param_spec_string ("text", "Text", "The text to display",
          DEFAULT_PROP_TEXT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GESTimelineTextOverlay:font-desc
   *
   * Pango font description string
   */

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FONT_DESC,
      g_param_spec_string ("font-desc", "font description",
          "Pango font description of font to be used for rendering. "
          "See documentation of pango_font_description_from_string "
          "for syntax.", DEFAULT_PROP_FONT_DESC,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GESTimelineTextOverlay:valignment
   *
   * Vertical alignent of the text
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_VALIGNMENT,
      g_param_spec_enum ("valignment", "vertical alignment",
          "Vertical alignment of the text", GES_TEXT_VALIGN_TYPE,
          DEFAULT_PROP_VALIGNMENT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
          G_PARAM_STATIC_STRINGS));
  /**
   * GESTimelineTextOverlay:halignment
   *
   * Horizontal alignment of the text
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HALIGNMENT,
      g_param_spec_enum ("halignment", "horizontal alignment",
          "Horizontal alignment of the text",
          GES_TEXT_HALIGN_TYPE, DEFAULT_PROP_HALIGNMENT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  timobj_class->create_track_object =
      ges_timeline_text_overlay_create_track_object;
  timobj_class->need_fill_track = FALSE;
}

static void
ges_timeline_text_overlay_init (GESTimelineTextOverlay * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TIMELINE_TEXT_OVERLAY, GESTimelineTextOverlayPrivate);

  GES_TIMELINE_OBJECT (self)->duration = 0;
  /* Not 100% needed since gobject contents are memzero'd when created */
  self->priv->text = NULL;
  self->priv->font_desc = NULL;
  self->priv->halign = DEFAULT_PROP_HALIGNMENT;
  self->priv->valign = DEFAULT_PROP_VALIGNMENT;
}

/**
 * ges_timeline_text_overlay_set_text:
 * @self: the #GESTimelineTextOverlay* to set text on
 * @text: the text to render. an internal copy of this text will be
 * made.
 *
 * Sets the text this timeline object will render.
 *
 */
void
ges_timeline_text_overlay_set_text (GESTimelineTextOverlay * self,
    const gchar * text)
{
  GList *tmp, *trackobjects;
  GESTimelineObject *object = (GESTimelineObject *) self;

  GST_DEBUG ("self:%p, text:%s", self, text);

  if (self->priv->text)
    g_free (self->priv->text);

  self->priv->text = g_strdup (text);

  trackobjects = ges_timeline_object_get_track_objects (object);
  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    GESTrackObject *trackobject = (GESTrackObject *) tmp->data;

    if (ges_track_object_get_track (trackobject)->type == GES_TRACK_TYPE_VIDEO)
      ges_track_text_overlay_set_text (GES_TRACK_TEXT_OVERLAY
          (trackobject), self->priv->text);

    g_object_unref (GES_TRACK_OBJECT (tmp->data));
  }
  g_list_free (trackobjects);
}

/**
 * ges_timeline_text_overlay_set_font_desc:
 * @self: the #GESTimelineTextOverlay*
 * @font_desc: the pango font description
 *
 * Sets the pango font description of the text
 *
 */
void
ges_timeline_text_overlay_set_font_desc (GESTimelineTextOverlay * self,
    const gchar * font_desc)
{
  GList *tmp, *trackobjects;
  GESTimelineObject *object = (GESTimelineObject *) self;

  GST_DEBUG ("self:%p, font_desc:%s", self, font_desc);

  if (self->priv->font_desc)
    g_free (self->priv->font_desc);

  self->priv->font_desc = g_strdup (font_desc);

  trackobjects = ges_timeline_object_get_track_objects (object);
  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    GESTrackObject *trackobject = (GESTrackObject *) tmp->data;

    if (ges_track_object_get_track (trackobject)->type == GES_TRACK_TYPE_VIDEO)
      ges_track_text_overlay_set_font_desc (GES_TRACK_TEXT_OVERLAY
          (trackobject), self->priv->font_desc);

    g_object_unref (GES_TRACK_OBJECT (tmp->data));
  }
  g_list_free (trackobjects);

}

/**
 * ges_timeline_text_overlay_set_halign:
 * @self: the #GESTimelineTextOverlay* to set horizontal alignement of text on
 * @halign: #GESTextHAlign
 *
 * Sets the horizontal aligment of the text.
 *
 */
void
ges_timeline_text_overlay_set_halign (GESTimelineTextOverlay * self,
    GESTextHAlign halign)
{
  GList *tmp, *trackobjects;
  GESTimelineObject *object = (GESTimelineObject *) self;

  GST_DEBUG ("self:%p, halign:%d", self, halign);

  self->priv->halign = halign;

  trackobjects = ges_timeline_object_get_track_objects (object);
  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    GESTrackObject *trackobject = (GESTrackObject *) tmp->data;

    if (ges_track_object_get_track (trackobject)->type == GES_TRACK_TYPE_VIDEO)
      ges_track_text_overlay_set_halignment (GES_TRACK_TEXT_OVERLAY
          (trackobject), self->priv->halign);

    g_object_unref (GES_TRACK_OBJECT (tmp->data));
  }
  g_list_free (trackobjects);

}

/**
 * ges_timeline_text_overlay_set_valign:
 * @self: the #GESTimelineTextOverlay* to set vertical alignement of text on
 * @valign: #GESTextVAlign
 *
 * Sets the vertical aligment of the text.
 *
 */
void
ges_timeline_text_overlay_set_valign (GESTimelineTextOverlay * self,
    GESTextVAlign valign)
{
  GList *tmp, *trackobjects;
  GESTimelineObject *object = (GESTimelineObject *) self;

  GST_DEBUG ("self:%p, valign:%d", self, valign);

  self->priv->valign = valign;

  trackobjects = ges_timeline_object_get_track_objects (object);
  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    GESTrackObject *trackobject = (GESTrackObject *) tmp->data;

    if (ges_track_object_get_track (trackobject)->type == GES_TRACK_TYPE_VIDEO)
      ges_track_text_overlay_set_valignment (GES_TRACK_TEXT_OVERLAY
          (trackobject), self->priv->valign);

    g_object_unref (GES_TRACK_OBJECT (tmp->data));
  }
  g_list_free (trackobjects);

}

/**
 * ges_timeline_text_overlay_get_text:
 * @self: a #GESTimelineTextOverlay
 *
 * Get the text currently set on @self.
 *
 * Returns: The text currently set on @self.
 *
 */
const gchar *
ges_timeline_text_overlay_get_text (GESTimelineTextOverlay * self)
{
  return self->priv->text;
}

/**
 * ges_timeline_text_overlay_get_font_desc:
 * @self: a #GESTimelineTextOverlay
 *
 * Get the pango font description used by @self.
 *
 * Returns: The pango font description used by @self.
 */
const char *
ges_timeline_text_overlay_get_font_desc (GESTimelineTextOverlay * self)
{
  return self->priv->font_desc;
}

/**
 * ges_timeline_text_overlay_get_halignment:
 * @self: a #GESTimelineTextOverlay
 *
 * Get the horizontal aligment used by @self.
 *
 * Returns: The horizontal aligment used by @self.
 */
GESTextHAlign
ges_timeline_text_overlay_get_halignment (GESTimelineTextOverlay * self)
{
  return self->priv->halign;
}

/**
 * ges_timeline_text_overlay_get_valignment:
 * @self: a #GESTimelineTextOverlay
 *
 * Get the vertical aligment used by @self.
 *
 * Returns: The vertical aligment used by @self.
 */
GESTextVAlign
ges_timeline_text_overlay_get_valignment (GESTimelineTextOverlay * self)
{
  return self->priv->valign;
}

static GESTrackObject *
ges_timeline_text_overlay_create_track_object (GESTimelineObject * obj,
    GESTrack * track)
{

  GESTimelineTextOverlayPrivate *priv = GES_TIMELINE_TEXT_OVERLAY (obj)->priv;
  GESTrackObject *res = NULL;

  GST_DEBUG ("Creating a GESTrackOverlay");

  if (track->type == GES_TRACK_TYPE_VIDEO) {
    res = (GESTrackObject *) ges_track_text_overlay_new ();
    GST_DEBUG ("Setting text property");
    ges_track_text_overlay_set_text ((GESTrackTextOverlay *) res, priv->text);
    ges_track_text_overlay_set_font_desc ((GESTrackTextOverlay *) res,
        priv->font_desc);
    ges_track_text_overlay_set_halignment ((GESTrackTextOverlay *) res,
        priv->halign);
    ges_track_text_overlay_set_valignment ((GESTrackTextOverlay *) res,
        priv->valign);
  }

  return res;
}

/**
 * ges_timeline_text_overlay_new:
 *
 * Creates a new #GESTimelineTextOverlay
 *
 * Returns: The newly created #GESTimelineTextOverlay, or NULL if there was an
 * error.
 */
GESTimelineTextOverlay *
ges_timeline_text_overlay_new (void)
{
  /* FIXME : Check for validity/existence of URI */
  return g_object_new (GES_TYPE_TIMELINE_TEXT_OVERLAY, NULL);
}
