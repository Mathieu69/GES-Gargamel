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
 * SECTION:ges-timeline-object
 * @short_description: Base Class for objects in a #GESTimelineLayer
 *
 * A #GESTimelineObject is a 'natural' object which controls one or more
 * #GESTrackObject(s) in one or more #GESTrack(s).
 *
 * Keeps a reference to the #GESTrackObject(s) it created and
 * sets/updates their properties.
 */

#include "ges-timeline-object.h"
#include "ges.h"
#include "ges-internal.h"

gboolean
ges_timeline_object_fill_track_object_func (GESTimelineObject * object,
    GESTrackObject * trackobj, GstElement * gnlobj);

gboolean
ges_timeline_object_create_track_objects_func (GESTimelineObject
    * object, GESTrack * track);

static void
track_object_start_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object);
static void
track_object_inpoint_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object);
static void
track_object_duration_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object);
static void
track_object_priority_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object);

G_DEFINE_ABSTRACT_TYPE (GESTimelineObject, ges_timeline_object,
    G_TYPE_INITIALLY_UNOWNED);

/* Mapping of relationship between a TimelineObject and the TrackObjects
 * it controls
 *
 * NOTE : how do we make this public in the future ?
 */
typedef struct
{
  GESTrackObject *object;
  gint64 start_offset;
  gint64 duration_offset;
  gint64 inpoint_offset;
  gint32 priority_offset;

  guint start_notifyid;
  guint duration_notifyid;
  guint inpoint_notifyid;
  guint priority_notifyid;

  /* track mapping ?? */
} ObjectMapping;

struct _GESTimelineObjectPrivate
{
  /*< public > */
  GESTimelineLayer *layer;

  /*< private > */
  /* A list of TrackObject controlled by this TimelineObject */
  GList *trackobjects;

  /* Set to TRUE when the timelineobject is doing updates of track object
   * properties so we don't end up in infinite property update loops
   */
  gboolean ignore_notifies;

  GList *mappings;
};

enum
{
  PROP_0,
  PROP_START,
  PROP_INPOINT,
  PROP_DURATION,
  PROP_PRIORITY,
  PROP_HEIGHT,
  PROP_LAYER,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void
ges_timeline_object_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GESTimelineObject *tobj = GES_TIMELINE_OBJECT (object);

  switch (property_id) {
    case PROP_START:
      g_value_set_uint64 (value, tobj->start);
      break;
    case PROP_INPOINT:
      g_value_set_uint64 (value, tobj->inpoint);
      break;
    case PROP_DURATION:
      g_value_set_uint64 (value, tobj->duration);
      break;
    case PROP_PRIORITY:
      g_value_set_uint (value, tobj->priority);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, tobj->height);
      break;
    case PROP_LAYER:
      g_value_set_object (value, tobj->priv->layer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_timeline_object_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GESTimelineObject *tobj = GES_TIMELINE_OBJECT (object);

  switch (property_id) {
    case PROP_START:
      ges_timeline_object_set_start (tobj, g_value_get_uint64 (value));
      break;
    case PROP_INPOINT:
      ges_timeline_object_set_inpoint (tobj, g_value_get_uint64 (value));
      break;
    case PROP_DURATION:
      ges_timeline_object_set_duration (tobj, g_value_get_uint64 (value));
      break;
    case PROP_PRIORITY:
      ges_timeline_object_set_priority (tobj, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ges_timeline_object_class_init (GESTimelineObjectClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GESTimelineObjectPrivate));

  object_class->get_property = ges_timeline_object_get_property;
  object_class->set_property = ges_timeline_object_set_property;
  klass->create_track_objects = ges_timeline_object_create_track_objects_func;

  /**
   * GESTimelineObject:start
   *
   * The position of the object in the #GESTimelineLayer (in nanoseconds).
   */
  properties[PROP_START] = g_param_spec_uint64 ("start", "Start",
      "The position in the container", 0, G_MAXUINT64, 0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_START,
      properties[PROP_START]);

  /**
   * GESTimelineObject:in-point
   *
   * The in-point at which this #GESTimelineObject will start outputting data
   * from its contents (in nanoseconds).
   *
   * Ex : an in-point of 5 seconds means that the first outputted buffer will
   * be the one located 5 seconds in the controlled resource.
   */
  properties[PROP_INPOINT] =
      g_param_spec_uint64 ("in-point", "In-point", "The in-point", 0,
      G_MAXUINT64, 0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_INPOINT,
      properties[PROP_INPOINT]);

  /**
   * GESTimelineObject:duration
   *
   * The duration (in nanoseconds) which will be used in the container #GESTrack
   * starting from 'in-point'.
   */
  properties[PROP_DURATION] =
      g_param_spec_uint64 ("duration", "Duration", "The duration to use", 0,
      G_MAXUINT64, GST_CLOCK_TIME_NONE, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DURATION,
      properties[PROP_DURATION]);

  /**
   * GESTimelineObject:priority
   *
   * The layer priority of the timeline object.
   */
  properties[PROP_PRIORITY] = g_param_spec_uint ("priority", "Priority",
      "The priority of the object", 0, G_MAXUINT, 0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_PRIORITY,
      properties[PROP_PRIORITY]);

  /**
   * GESTimelineObject:height
   *
   * The span of layer priorities which this object occupies.
   */

  g_object_class_install_property (object_class, PROP_HEIGHT,
      g_param_spec_uint ("height", "Height",
          "The span of priorities this object occupies", 0, G_MAXUINT, 1,
          G_PARAM_READABLE));

  /* GESSimpleTimelineLayer:layer
   *
   * The GESTimelineLayer where this object is being used.
   */

  g_object_class_install_property (object_class, PROP_LAYER,
      g_param_spec_object ("layer", "Layer",
          "The GESTimelineLayer where this object is being used.",
          GES_TYPE_TIMELINE_LAYER, G_PARAM_READABLE));

  klass->need_fill_track = TRUE;
}

static void
ges_timeline_object_init (GESTimelineObject * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_TIMELINE_OBJECT, GESTimelineObjectPrivate);
  self->duration = GST_SECOND;
  self->height = 1;
  self->priv->trackobjects = NULL;
  self->priv->layer = NULL;
}

/**
 * ges_timeline_object_create_track_object:
 * @object: The origin #GESTimelineObject
 * @track: The #GESTrack to create a #GESTrackObject for.
 *
 * Creates a #GESTrackObject for the provided @track. The timeline object
 * keep a reference to the newly created trackobject, you therefore need to
 * call @ges_timeline_object_release_track_object when you are done with it.
 *
 * Returns: (transfer none): A #GESTrackObject. Returns NULL if the #GESTrackObject could not
 * be created.
 */

GESTrackObject *
ges_timeline_object_create_track_object (GESTimelineObject * object,
    GESTrack * track)
{
  GESTimelineObjectClass *class;
  GESTrackObject *res;

  class = GES_TIMELINE_OBJECT_GET_CLASS (object);

  if (G_UNLIKELY (class->create_track_object == NULL)) {
    GST_ERROR ("No 'create_track_object' implementation available");
    return NULL;
  }

  res = class->create_track_object (object, track);
  ges_timeline_object_add_track_object (object, res);
  return res;

}

/**
 * ges_timeline_object_create_track_objects:
 * @object: The origin #GESTimelineObject
 * @track: The #GESTrack to create each #GESTrackObject for.
 *
 * Creates all #GESTrackObjects supported by this object and adds them to the
 * provided track. The track is responsible for calling
 * #ges_timeline_release_track_object on these objects when it is finished
 * with them.
 *
 * Returns: %TRUE if each track object was created successfully, or %FALSE if an
 * error occured.
 */

gboolean
ges_timeline_object_create_track_objects (GESTimelineObject * object,
    GESTrack * track)
{
  GESTimelineObjectClass *klass;

  klass = GES_TIMELINE_OBJECT_GET_CLASS (object);

  if (!(klass->create_track_objects)) {
    GST_WARNING ("no GESTimelineObject::create_track_objects implentation");
    return FALSE;
  }

  return klass->create_track_objects (object, track);
}

/*
 * default implementation of GESTimelineObjectClass::create_track_objects
 */
gboolean
ges_timeline_object_create_track_objects_func (GESTimelineObject * object,
    GESTrack * track)
{
  GESTrackObject *result;

  result = ges_timeline_object_create_track_object (object, track);
  if (!result) {
    GST_WARNING ("couldn't create track object");
    return FALSE;
  }
  return ges_track_add_object (track, result);
}

/**
 * ges_timeline_object_add_track_object:
 * @object: a #GESTimelineObject
 * @trobj: the GESTrackObject
 *
 * Add a track object to the timeline object. Should only be called by
 * subclasses implementing the create_track_objects (plural) vmethod.
 *
 * Takes a reference on @trobj.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 */

gboolean
ges_timeline_object_add_track_object (GESTimelineObject * object, GESTrackObject
    * trobj)
{
  ObjectMapping *mapping;

  GST_LOG ("Got a TrackObject : %p , setting the timeline object as its"
      "creator", trobj);

  if (!trobj)
    return FALSE;

  ges_track_object_set_timeline_object (trobj, object);
  g_object_ref (trobj);

  mapping = g_slice_new0 (ObjectMapping);
  mapping->object = trobj;
  object->priv->mappings = g_list_append (object->priv->mappings, mapping);

  GST_DEBUG ("Adding TrackObject to the list of controlled track objects");
  /* We steal the initial reference */
  object->priv->trackobjects =
      g_list_append (object->priv->trackobjects, trobj);

  GST_DEBUG ("Setting properties on newly created TrackObject");

  ges_track_object_set_start (trobj, object->start);
  ges_track_object_set_priority (trobj, object->priority);
  ges_track_object_set_duration (trobj, object->duration);
  ges_track_object_set_inpoint (trobj, object->inpoint);

  GST_DEBUG ("Returning trobj:%p", trobj);

  /* Listen to all property changes */
  mapping->start_notifyid =
      g_signal_connect (G_OBJECT (trobj), "notify::start",
      G_CALLBACK (track_object_start_changed_cb), object);
  mapping->duration_notifyid =
      g_signal_connect (G_OBJECT (trobj), "notify::duration",
      G_CALLBACK (track_object_duration_changed_cb), object);
  mapping->inpoint_notifyid =
      g_signal_connect (G_OBJECT (trobj), "notify::inpoint",
      G_CALLBACK (track_object_inpoint_changed_cb), object);
  mapping->priority_notifyid =
      g_signal_connect (G_OBJECT (trobj), "notify::priority",
      G_CALLBACK (track_object_priority_changed_cb), object);

  return TRUE;
}

/**
 * ges_timeline_object_release_track_object:
 * @object: a #GESTimelineObject
 * @trackobject: the #GESTrackObject to release
 *
 * Release the @trackobject from the control of @object.
 *
 * Returns: %TRUE if the @trackobject was properly released, else %FALSE.
 */
gboolean
ges_timeline_object_release_track_object (GESTimelineObject * object,
    GESTrackObject * trackobject)
{
  GList *tmp;
  ObjectMapping *mapping = NULL;

  GST_DEBUG ("object:%p, trackobject:%p", object, trackobject);

  if (!(g_list_find (object->priv->trackobjects, trackobject))) {
    GST_WARNING ("TrackObject isn't controlled by this object");
    return FALSE;
  }

  /* FIXME : Do we need to tell the subclasses ?
   * If so, add a new virtual-method */

  for (tmp = object->priv->mappings; tmp; tmp = tmp->next) {
    mapping = (ObjectMapping *) tmp->data;
    if (mapping->object == trackobject)
      break;
  }

  if (tmp && mapping) {

    /* Disconnect all notify listeners */
    g_signal_handler_disconnect (trackobject, mapping->start_notifyid);
    g_signal_handler_disconnect (trackobject, mapping->duration_notifyid);
    g_signal_handler_disconnect (trackobject, mapping->inpoint_notifyid);
    g_signal_handler_disconnect (trackobject, mapping->priority_notifyid);

    g_slice_free (ObjectMapping, mapping);

    object->priv->mappings = g_list_delete_link (object->priv->mappings, tmp);
  }

  object->priv->trackobjects =
      g_list_remove (object->priv->trackobjects, trackobject);

  ges_track_object_set_timeline_object (trackobject, NULL);

  GST_DEBUG ("Removing reference to track object %p", trackobject);

  g_object_unref (trackobject);

  /* FIXME : resync properties ? */

  return TRUE;
}

void
ges_timeline_object_set_layer (GESTimelineObject * object,
    GESTimelineLayer * layer)
{
  GST_DEBUG ("object:%p, layer:%p", object, layer);

  object->priv->layer = layer;
}

gboolean
ges_timeline_object_fill_track_object (GESTimelineObject * object,
    GESTrackObject * trackobj, GstElement * gnlobj)
{
  GESTimelineObjectClass *class;
  gboolean res = TRUE;

  GST_DEBUG ("object:%p, trackobject:%p, gnlobject:%p",
      object, trackobj, gnlobj);

  class = GES_TIMELINE_OBJECT_GET_CLASS (object);

  if (class->need_fill_track) {
    if (G_UNLIKELY (class->fill_track_object == NULL)) {
      GST_WARNING ("No 'fill_track_object' implementation available");
      return FALSE;
    }

    res = class->fill_track_object (object, trackobj, gnlobj);
  }

  GST_DEBUG ("Returning res:%d", res);

  return res;
}

gboolean
ges_timeline_object_fill_track_object_func (GESTimelineObject * object,
    GESTrackObject * trackobj, GstElement * gnlobj)
{
  GST_WARNING ("No 'fill_track_object' implementation !");

  return FALSE;
}

static ObjectMapping *
find_object_mapping (GESTimelineObject * object, GESTrackObject * child)
{
  GList *tmp;

  for (tmp = object->priv->mappings; tmp; tmp = tmp->next) {
    ObjectMapping *map = (ObjectMapping *) tmp->data;
    if (map->object == child)
      return map;
  }

  return NULL;
}

/**
 * ges_timeline_object_set_start:
 * @object: a #GESTimelineObject
 * @start: the position in #GstClockTime
 *
 * Set the position of the object in its containing layer
 */
void
ges_timeline_object_set_start (GESTimelineObject * object, guint64 start)
{
  GList *tmp;
  GESTrackObject *tr;
  ObjectMapping *map;

  GST_DEBUG ("object:%p, start:%" GST_TIME_FORMAT,
      object, GST_TIME_ARGS (start));

  object->priv->ignore_notifies = TRUE;

  for (tmp = object->priv->trackobjects; tmp; tmp = g_list_next (tmp)) {
    tr = (GESTrackObject *) tmp->data;
    map = find_object_mapping (object, tr);

    if (ges_track_object_is_locked (tr)) {
      /* Move the child... */
      ges_track_object_set_start (tr, start + map->start_offset);
    } else {
      /* ... or update the offset */
      map->start_offset = start - tr->start;
    }
  }

  object->priv->ignore_notifies = FALSE;

  object->start = start;
}

/**
 * ges_timeline_object_set_inpoint:
 * @object: a #GESTimelineObject
 * @inpoint: the in-point in #GstClockTime
 *
 * Set the in-point, that is the moment at which the @object will start
 * outputting data from its contents.
 */
void
ges_timeline_object_set_inpoint (GESTimelineObject * object, guint64 inpoint)
{
  GList *tmp;
  GESTrackObject *tr;

  GST_DEBUG ("object:%p, inpoint:%" GST_TIME_FORMAT,
      object, GST_TIME_ARGS (inpoint));

  for (tmp = object->priv->trackobjects; tmp; tmp = g_list_next (tmp)) {
    tr = (GESTrackObject *) tmp->data;

    if (ges_track_object_is_locked (tr))
      /* call set_inpoint on each trackobject */
      ges_track_object_set_inpoint (tr, inpoint);
  }

  object->inpoint = inpoint;
}

/**
 * ges_timeline_object_set_duration:
 * @object: a #GESTimelineObject
 * @duration: the duration in #GstClockTime
 *
 * Set the duration of the object
 */
void
ges_timeline_object_set_duration (GESTimelineObject * object, guint64 duration)
{
  GList *tmp;
  GESTrackObject *tr;

  GST_DEBUG ("object:%p, duration:%" GST_TIME_FORMAT,
      object, GST_TIME_ARGS (duration));

  for (tmp = object->priv->trackobjects; tmp; tmp = g_list_next (tmp)) {
    tr = (GESTrackObject *) tmp->data;

    if (ges_track_object_is_locked (tr))
      /* call set_duration on each trackobject */
      ges_track_object_set_duration (tr, duration);
  }

  object->duration = duration;
}

/**
 * ges_timeline_object_set_priority:
 * @object: a #GESTimelineObject
 * @priority: the priority
 *
 * Sets the priority of the object within the containing layer
 */
void
ges_timeline_object_set_priority (GESTimelineObject * object, guint priority)
{
  GList *tmp;
  GESTrackObject *tr;
  ObjectMapping *map;

  GST_DEBUG ("object:%p, priority:%" GST_TIME_FORMAT,
      object, GST_TIME_ARGS (priority));

  object->priv->ignore_notifies = TRUE;

  for (tmp = object->priv->trackobjects; tmp; tmp = g_list_next (tmp)) {
    tr = (GESTrackObject *) tmp->data;
    map = find_object_mapping (object, tr);

    if (ges_track_object_is_locked (tr)) {
      /* Move the child... */
      ges_track_object_set_priority (tr, priority + map->priority_offset);
    } else {
      /* ... or update the offset */
      map->priority_offset = priority - tr->priority;
    }
  }

  object->priv->ignore_notifies = FALSE;

  object->priority = priority;
}

/**
 * ges_timeline_object_find_track_object:
 * @object: a #GESTimelineObject
 * @track: a #GESTrack or NULL
 * @type: a #GType indicating the type of track object you are looking
 * for or %G_TYPE_NONE if you do not care about the track type.
 *
 * Finds the #GESTrackObject controlled by @object that is used in @track. You
 * may optionally specify a GType to further narrow search criteria.
 *
 * Note: The reference count of the returned #GESTrackObject will be increased,
 * unref when done with it.
 *
 * Returns: (transfer full): The #GESTrackObject used by @track, else #NULL.
 */

GESTrackObject *
ges_timeline_object_find_track_object (GESTimelineObject * object,
    GESTrack * track, GType type)
{
  GESTrackObject *ret = NULL;
  GList *tmp;
  GESTrackObject *otmp;

  for (tmp = object->priv->trackobjects; tmp; tmp = g_list_next (tmp)) {
    otmp = (GESTrackObject *) tmp->data;

    if (ges_track_object_get_track (otmp) == track) {
      if ((type != G_TYPE_NONE) && !G_TYPE_CHECK_INSTANCE_TYPE (tmp->data,
              type))
        continue;

      ret = GES_TRACK_OBJECT (tmp->data);
      g_object_ref (ret);
      break;
    }
  }

  return ret;
}

/**
 * ges_timeline_object_get_layer:
 * @object: a #GESTimelineObject
 *
 * Note: The reference count of the returned #GESTimelineLayer will be increased,
 * The user is responsible for unreffing it.
 *
 * Returns: (transfer full): The #GESTimelineLayer where this @object is being used, #NULL if 
 * it is not used on any layer.
 */
GESTimelineLayer *
ges_timeline_object_get_layer (GESTimelineObject * object)
{
  g_return_val_if_fail (GES_IS_TIMELINE_OBJECT (object), NULL);

  if (object->priv->layer != NULL)
    g_object_ref (G_OBJECT (object->priv->layer));

  return object->priv->layer;
}

/**
 * ges_timeline_object_get_track_objects:
 * @object: a #GESTimelineObject
 *
 * Get the list of #GESTrackObject contained in @object
 *
 * Returns: (transfer full) (element-type GESTrackObject): The list of
 * trackobject contained in @object.
 * The user is responsible for unreffing the contained objects 
 * and freeing the list.
 */
GList *
ges_timeline_object_get_track_objects (GESTimelineObject * object)
{
  GList *ret;
  GList *tmp;

  g_return_val_if_fail (GES_IS_TIMELINE_OBJECT (object), NULL);

  ret = g_list_copy (object->priv->trackobjects);

  for (tmp = ret; tmp; tmp = tmp->next) {
    g_object_ref (tmp->data);
  }

  return ret;
}


/*
 * PROPERTY NOTIFICATIONS FROM TRACK OBJECTS
 */

static void
track_object_start_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object)
{
  ObjectMapping *map;

  if (object->priv->ignore_notifies)
    return;

  map = find_object_mapping (object, child);
  if (G_UNLIKELY (map == NULL))
    /* something massively screwed up if we get this */
    return;

  if (!ges_track_object_is_locked (child)) {
    /* Update the internal start_offset */
    map->start_offset = object->start - child->start;
  } else {
    /* Or update the parent start */
    ges_timeline_object_set_start (object, child->start + map->start_offset);
  }
}

static void
track_object_inpoint_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object)
{
  if (object->priv->ignore_notifies)
    return;

}

static void
track_object_duration_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object)
{
  if (object->priv->ignore_notifies)
    return;

}

static void
track_object_priority_changed_cb (GESTrackObject * child,
    GParamSpec * arg G_GNUC_UNUSED, GESTimelineObject * object)
{
  ObjectMapping *map;

  if (object->priv->ignore_notifies)
    return;

  map = find_object_mapping (object, child);
  if (G_UNLIKELY (map == NULL))
    /* something massively screwed up if we get this */
    return;

  if (!ges_track_object_is_locked (child)) {
    GList *tmp;
    guint32 min_prio = G_MAXUINT32, max_prio = 0;

    /* Update the internal priority_offset */
    map->priority_offset = object->priority - child->priority;

    /* Go over all childs and check if height has changed */
    for (tmp = object->priv->trackobjects; tmp; tmp = tmp->next) {
      GESTrackObject *tmpo = (GESTrackObject *) tmp->data;

      if (tmpo->priority < min_prio)
        min_prio = tmpo->priority;
      if (tmpo->priority > max_prio)
        max_prio = tmpo->priority;
    }

    /* FIXME : We only grow the height */
    if (object->height < (max_prio - min_prio + 1)) {
      object->height = max_prio - min_prio + 1;
#if GLIB_CHECK_VERSION(2,26,0)
      g_object_notify_by_pspec (G_OBJECT (object), properties[PROP_HEIGHT]);
#else
      g_object_notify (G_OBJECT (object), "height");
#endif
    }
  } else {
    /* Or update the parent priority */
    ges_timeline_object_set_priority (object,
        child->priority + map->priority_offset);
    /* For the locked situation, we don't need to check the height,
     * since all object priorities are moving together */
  }
}
