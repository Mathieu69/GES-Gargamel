#include <ges/ges.h>

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);

xmlDocPtr create_doc (const gchar * uri);
gboolean add_to_timeline (GESTrack * track, GESTimeline * timeline);

void set_property (GObject * src, gchar * prop_name, gchar * prop_value);
GHashTable *print_xpath_nodes (xmlNodePtr nodes);
gboolean create_tracks (xmlXPathContextPtr xpathCtx, GESTimeline * timeline);
GESTrack *create_track (GHashTable * table);
GHashTable *list_sources (xmlXPathContextPtr xpathCtx,
    GESTimelineLayer * layer);
void parse_track_objects (xmlXPathContextPtr xpathCtx, GESTimelineLayer * layer,
    GHashTable * sources_table);
GHashTable *parse_timeline_objects (GHashTable * sources_table,
    xmlXPathContextPtr xpathCtx);
void set_source_properties (GObject * src, GHashTable * table,
    GESTimelineLayer * layer);
void make_transition (GESTimelineLayer * layer, gint64 start, gint64 prev_end,
    gchar * type);
void calculate_transitions (GList * list, GESTimelineLayer * layer);
void make_transitions (GList * audio_tr_list, GList * video_tr_list,
    GESTimelineLayer * layer);
void make_unorthodox_transition (GESTimelineLayer * layer, gint64 start,
    gint64 duration, gint64 audio_start, gint64 audio_duration);
void save_tracks (GESTimeline * timeline, xmlTextWriterPtr writer,
    GList * source_list);
GList *save_sources (GESTimeline * timeline, xmlTextWriterPtr writer);
void create_new_source_table (gchar * key, gchar * value, GHashTable * table);
void save_track_objects (xmlTextWriterPtr writer, GList * source_list,
    gchar * res, gint * id);
void save_timeline_objects (xmlTextWriterPtr writer, GList * list);

struct _GESPitiviFormatterPrivate
{
  gchar *bin_description;
};

GESTimelineFileSource *layer_sources_list[50];
int i = 0;
GESTrack *timeline_tracks_list[2];
int k = 0;

static void
ges_pitivi_formatter_class_init (GESPitiviFormatterClass * klass)
{
  GESFormatterClass *formatter_klass;
  g_type_class_add_private (klass, sizeof (GESPitiviFormatterPrivate));

  formatter_klass = GES_FORMATTER_CLASS (klass);

  formatter_klass->save_to_uri = save_pitivi_timeline_to_uri;
  formatter_klass->load_from_uri = load_pitivi_file_from_uri;
}

static void
ges_pitivi_formatter_init (GESPitiviFormatter * object)
{
}

static gboolean
save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri)
{
  xmlTextWriterPtr writer;
  GList *list = NULL;
  writer = xmlNewTextWriterFilename (uri, 0);
  xmlTextWriterSetIndent (writer, 1);
  xmlTextWriterStartElement (writer, BAD_CAST "pitivi");

  list = save_sources (timeline, writer);
  save_tracks (timeline, writer, list);
  save_timeline_objects (writer, list);

  xmlTextWriterEndDocument (writer);
  xmlFreeTextWriter (writer);
  return TRUE;
}

void
save_timeline_objects (xmlTextWriterPtr writer, GList * list)
{
  GList *tmp;
  xmlTextWriterStartElement (writer, BAD_CAST "timeline-objects");
  for (tmp = list; tmp; tmp = tmp->next) {
    GList *elem;
    xmlTextWriterStartElement (writer, BAD_CAST "timeline-object");
    elem = tmp->data;
    xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
        BAD_CAST
        xmlXPathCastNumberToString (GPOINTER_TO_INT (g_list_first
                (elem)->data)));
    xmlTextWriterEndElement (writer);
    xmlTextWriterStartElement (writer, BAD_CAST "track-object-refs");
    xmlTextWriterStartElement (writer, BAD_CAST "track-object-ref");
    if (g_list_length (elem) == 5) {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 3)->data));
      xmlTextWriterEndElement (writer);
      xmlTextWriterStartElement (writer, BAD_CAST "track-object-ref");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 4)->data));
    } else {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 3)->data));
    }
    xmlTextWriterEndElement (writer);
    xmlTextWriterEndElement (writer);
    xmlTextWriterEndElement (writer);
  }
  xmlTextWriterEndElement (writer);
}

GList *
save_sources (GESTimeline * timeline, xmlTextWriterPtr writer)
{
  GList *layers, *objects, *tmp;
  GESTimelineLayer *layer;
  int id = 1;
  GHashTable *source_table;
  GList *source_list = NULL;

  layers = ges_timeline_get_layers (timeline);
  layer = g_list_first (layers)->data;
  objects = ges_timeline_layer_get_objects (layer);
  source_table =
      g_hash_table_new_full (g_str_hash, g_int_equal, g_free, g_free);

  xmlTextWriterStartElement (writer, BAD_CAST "factories");
  xmlTextWriterStartElement (writer, BAD_CAST "sources");

  for (tmp = objects; tmp; tmp = tmp->next) {
    GList *ref_type_list = NULL;
    GESTimelineObject *object;
    gchar *tfs_uri;
    object = tmp->data;
    if GES_IS_TIMELINE_TEST_SOURCE
      (object) {
      tfs_uri = (gchar *) "test";
      if (!g_hash_table_lookup (source_table, tfs_uri)) {
        g_hash_table_insert (source_table, tfs_uri, GINT_TO_POINTER (id));

      } else {
        tfs_uri = (gchar *) "test2";
        g_hash_table_insert (source_table, tfs_uri, GINT_TO_POINTER (id));
      }
      xmlTextWriterStartElement (writer, BAD_CAST "source");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "filename",
          BAD_CAST tfs_uri);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST xmlXPathCastNumberToString (id));
      xmlTextWriterEndElement (writer);
      id++;
      ref_type_list =
          g_list_append (ref_type_list, g_hash_table_lookup (source_table,
              tfs_uri));
      ref_type_list = g_list_append (ref_type_list, object);
      ref_type_list = g_list_append (ref_type_list, g_strdup ("simple"));
      source_list = g_list_append (source_list, g_list_copy (ref_type_list));
    } else if GES_IS_TIMELINE_FILE_SOURCE
      (object) {
      tfs_uri = (gchar *) ges_timeline_filesource_get_uri
          (GES_TIMELINE_FILE_SOURCE (object));
      if (!g_hash_table_lookup (source_table, tfs_uri)) {
        g_hash_table_insert (source_table, tfs_uri, GINT_TO_POINTER (id));
        xmlTextWriterStartElement (writer, BAD_CAST "source");
        xmlTextWriterWriteAttribute (writer, BAD_CAST "filename",
            BAD_CAST tfs_uri);
        xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
            BAD_CAST xmlXPathCastNumberToString (id));
        xmlTextWriterEndElement (writer);
        id++;
      }
      ref_type_list =
          g_list_append (ref_type_list, g_hash_table_lookup (source_table,
              tfs_uri));
      ref_type_list = g_list_append (ref_type_list, object);
      if (ges_timeline_filesource_get_audio_only
          (GES_TIMELINE_FILE_SOURCE (object))) {
        ref_type_list = g_list_append (ref_type_list, g_strdup ("audio"));
      } else if (ges_timeline_filesource_get_video_only
          (GES_TIMELINE_FILE_SOURCE (object))) {
        ref_type_list = g_list_append (ref_type_list, g_strdup ("video"));
      } else {
        ref_type_list = g_list_append (ref_type_list, g_strdup ("simple"));
      }
      source_list = g_list_append (source_list, g_list_copy (ref_type_list));
      }
  }
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
  return source_list;
}

void
save_tracks (GESTimeline * timeline, xmlTextWriterPtr writer,
    GList * source_list)
{
  GList *tracks, *tmp;
  gint id = 0;
  xmlTextWriterStartElement (writer, BAD_CAST "timeline");
  xmlTextWriterStartElement (writer, BAD_CAST "tracks");
  tracks = ges_timeline_get_tracks (timeline);
  for (tmp = tracks; tmp; tmp = tmp->next) {
    gchar *type, *caps, *res;
    GESTrack *track;
    GValue v = { 0 };
    track = GES_TRACK (tmp->data);
    xmlTextWriterStartElement (writer, BAD_CAST "track");
    xmlTextWriterStartElement (writer, BAD_CAST "stream");
    g_value_init (&v, GES_TYPE_TRACK_TYPE);
    g_object_get_property (G_OBJECT (track), "track-type", &v);
    type = gst_value_serialize (&v);
    caps = gst_caps_to_string (ges_track_get_caps (track));
    xmlTextWriterWriteAttribute (writer, BAD_CAST "caps", BAD_CAST caps);
    if (!g_strcmp0 (type, "GES_TRACK_TYPE_AUDIO")) {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
          BAD_CAST "pitivi.stream.AudioStream");
      xmlTextWriterEndElement (writer);
      res = (gchar *) "audio";
    } else {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "type",
          BAD_CAST "pitivi.stream.VideoStream");
      xmlTextWriterEndElement (writer);
      res = (gchar *) "video";
    }
    save_track_objects (writer, source_list, res, &id);
    xmlTextWriterEndElement (writer);
  }
  xmlTextWriterEndElement (writer);
}

void
save_track_objects (xmlTextWriterPtr writer, GList * source_list, gchar * res,
    gint * id)
{
  GList *tmp;
  xmlTextWriterStartElement (writer, BAD_CAST "track-objects");
  for (tmp = source_list; tmp; tmp = tmp->next) {
    GList *elem;
    guint i, n;
    elem = tmp->data;
    if (!g_strcmp0 ((gchar *) g_list_nth (elem, (guint) 2)->data, res) ||
        !g_strcmp0 ((gchar *) g_list_nth (elem, (guint) 2)->data,
            (gchar *) "simple")) {
      GESTimelineObject *object;
      GParamSpec **properties;
      xmlTextWriterStartElement (writer, BAD_CAST "track-object");
      object = g_list_next (elem)->data;
      properties =
          g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &n);
      for (i = 0; i < n; i++) {
        GParamSpec *p = properties[i];
        GValue v = { 0 };
        gchar *serialized;
        if (!g_strcmp0 (p->name, (gchar *) "duration") ||
            !g_strcmp0 (p->name, (gchar *) "start") ||
            !g_strcmp0 (p->name, (gchar *) "in-point")) {
          g_value_init (&v, p->value_type);
          g_object_get_property (G_OBJECT (object), p->name, &v);
          serialized = gst_value_serialize (&v);
          serialized = g_strconcat ((gchar *) "(gint64)", serialized, NULL);
          if (!g_strcmp0 (p->name, (gchar *) "in-point")) {
            xmlTextWriterWriteAttribute (writer, BAD_CAST (gchar *) "in_point",
                BAD_CAST serialized);
          } else {
            xmlTextWriterWriteAttribute (writer, BAD_CAST p->name,
                BAD_CAST serialized);
          }
          g_free (serialized);
        }
      }
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST xmlXPathCastNumberToString (*id));
      xmlTextWriterEndElement (writer);
      xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST
          xmlXPathCastNumberToString (GPOINTER_TO_INT (g_list_first
                  (elem)->data)));
      xmlTextWriterEndElement (writer);
      elem = g_list_append (elem, xmlXPathCastNumberToString (*id));
      *id = *id + 1;
    }
  }
  xmlTextWriterEndElement (writer);
}

xmlDocPtr
create_doc (const gchar * uri)
{
  xmlDocPtr doc;
  doc = xmlParseFile (uri);

  if (doc == NULL) {
    GST_DEBUG ("Document not parsed successfully. \n");
  } else {
    GST_DEBUG ("Document parsed successfully.\n");
  }
  return doc;
}


GESTrack *
create_track (GHashTable * table)
{
  gchar *type, *caps_field;
  GValue v = { 0 };
  GstCaps *caps = { 0 };
  GESTrack *track = NULL;

  type = (gchar *) g_hash_table_lookup (table, (gchar *) "type");
  caps_field = (gchar *) g_hash_table_lookup (table, (gchar *) "caps");
  caps = gst_caps_from_string (caps_field);

  if (!g_strcmp0 (type, (gchar *) "pitivi.stream.AudioStream")) {
    gchar type_field[] = "GES_TRACK_TYPE_AUDIO";
    g_value_init (&v, GES_TYPE_TRACK_TYPE);
    gst_value_deserialize (&v, type_field);
    track = ges_track_new (g_value_get_flags (&v), caps);
  } else if (!g_strcmp0 (type, (gchar *) "pitivi.stream.VideoStream")) {
    gchar type_field[] = "GES_TRACK_TYPE_VIDEO";
    g_value_init (&v, GES_TYPE_TRACK_TYPE);
    gst_value_deserialize (&v, type_field);
    track = ges_track_new (g_value_get_flags (&v), caps);
  }
  return track;
}

gboolean
add_to_timeline (GESTrack * track, GESTimeline * timeline)
{
  if (!ges_timeline_add_track (timeline, track)) {
    g_object_unref (track);
    return FALSE;
  }
  return TRUE;
}

GHashTable *
list_sources (xmlXPathContextPtr xpathCtx, GESTimelineLayer * layer)
{
  xmlXPathObjectPtr xpathObj;
  GHashTable *table, *sources_table;
  int size, j;
  gchar *id;
  xmlNodeSetPtr nodes;

  sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources/source", xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    table = print_xpath_nodes (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    g_hash_table_insert (sources_table, g_strdup (id), table);
  }
  return sources_table;
}

void
parse_track_objects (xmlXPathContextPtr xpathCtx, GESTimelineLayer * layer,
    GHashTable * sources_table)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  GHashTable *table, *source_table;
  gchar *id;
  xmlChar *type;
  gchar *mode, *filename, *start, *duration;
  GESTimelineTestSource *testsrc = NULL;
  GESTimelineFileSource *src = NULL;
  GList *list = NULL;
  GList *lists_list = NULL;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/track-objects/track-object", xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    table = print_xpath_nodes (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    source_table = g_hash_table_lookup (sources_table, id);
    type =
        xmlGetProp (nodes->nodeTab[j]->parent->prev->prev, (xmlChar *) "type");
    mode = (gchar *) g_hash_table_lookup (source_table, (gchar *) "mode");
    start =
        g_strsplit ((gchar *) g_hash_table_lookup (table,
            (gchar *) "start"), ")", (gint) 0)[1];
    duration =
        g_strsplit ((gchar *) g_hash_table_lookup (table,
            (gchar *) "duration"), ")", (gint) 0)[1];
    filename =
        (gchar *) g_hash_table_lookup (source_table, (gchar *) "filename");
    list = NULL;
    if (!g_strcmp0 (mode, (gchar *) "simple")) {
      if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.AudioStream")) {
        continue;
      }
      if (!g_strcmp0 (filename, (gchar *) "test")
          || !g_strcmp0 (filename, (gchar *) "test2")) {
        testsrc = ges_timeline_test_source_new ();
        g_object_set (testsrc, "volume", (gdouble) 1, NULL);
        if (!g_strcmp0 (filename, (gchar *) "test")) {
          g_object_set (testsrc, "freq", (gdouble) 880, NULL);
          ges_timeline_test_source_set_vpattern (testsrc,
              GES_VIDEO_TEST_PATTERN_RED);
        }
      } else {
        src = ges_timeline_filesource_new (filename);
        type = (xmlChar *) "simple";
      }
    } else if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.VideoStream")) {
      src = ges_timeline_filesource_new (filename);
      ges_timeline_filesource_set_video_only (src, TRUE);
    } else {
      src = ges_timeline_filesource_new (filename);
      ges_timeline_filesource_set_audio_only (src, TRUE);
    }
    list = g_list_append (list, start);
    list = g_list_append (list, duration);
    list = g_list_append (list, type);
    lists_list = g_list_append (lists_list, list);
    if (!g_strcmp0 (filename, (gchar *) "test")
        || !g_strcmp0 (filename, (gchar *) "test2")) {
      set_source_properties (G_OBJECT (testsrc), table, layer);
    } else {
      set_source_properties (G_OBJECT (src), table, layer);
    }
  }
  calculate_transitions (lists_list, layer);
}

void
calculate_transitions (GList * list, GESTimelineLayer * layer)
{
  gint64 start, duration;
  xmlChar *type;
  gint64 end, prev_video_end = 0, prev_audio_end = 0;
  GList *audio_tr_list = NULL, *video_tr_list = NULL;

  for (list = list; list; list = list->next) {
    GValue a = { 0 };
    GValue v = { 0 };
    start =
        g_ascii_strtoll ((gchar *) g_list_first (list->data)->data, NULL, 0);
    duration =
        g_ascii_strtoll ((gchar *) g_list_next (list->data)->data, NULL, 0);
    type = (xmlChar *) g_list_last (list->data)->data;
    g_value_init (&v, GST_TYPE_INT64_RANGE);
    g_value_init (&a, GST_TYPE_INT64_RANGE);
    end = start + duration;
    if (!xmlStrcmp (type, (xmlChar *) "simple")) {
      if (start < prev_video_end) {
        gst_value_set_int64_range (&v, start, prev_video_end);
        video_tr_list =
            g_list_append (video_tr_list, g_strdup_value_contents (&v));
      }
      if (start < prev_audio_end) {
        gst_value_set_int64_range (&a, start, prev_audio_end);
        audio_tr_list =
            g_list_append (audio_tr_list, g_strdup_value_contents (&a));
      }
      prev_video_end = end;
      prev_audio_end = end;
    } else if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.VideoStream")) {
      if (start < prev_video_end) {
        gst_value_set_int64_range (&v, start, prev_video_end);
        video_tr_list =
            g_list_append (video_tr_list, g_strdup_value_contents (&v));
      }
      prev_video_end = end;
    } else if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.AudioStream")) {
      if (start < prev_audio_end) {
        gst_value_set_int64_range (&a, start, prev_audio_end);
        audio_tr_list =
            g_list_append (audio_tr_list, g_strdup_value_contents (&a));
      }
      prev_audio_end = end;
    }
  }
  make_transitions (audio_tr_list, video_tr_list, layer);
}

void
make_transitions (GList * audio_tr_list, GList * video_tr_list,
    GESTimelineLayer * layer)
{
  gchar **video_infos, **audio_infos;
  gint64 video_start, video_duration, audio_start, audio_duration;
  for (video_tr_list = video_tr_list; video_tr_list;
      video_tr_list = video_tr_list->next) {
    video_infos = g_strsplit_set ((gchar *) video_tr_list->data, "[],", -1);
    video_start = g_ascii_strtoll (video_infos[1], NULL, 0);
    video_duration = (g_ascii_strtoll (video_infos[2], NULL, 0) - video_start);
    if (audio_tr_list) {
      audio_infos = g_strsplit_set ((gchar *) audio_tr_list->data, "[],", -1);
      audio_start = g_ascii_strtoll (audio_infos[1], NULL, 0);
      audio_duration =
          (g_ascii_strtoll (audio_infos[2], NULL, 0) - audio_start);
      if (audio_start == video_start && audio_duration == video_duration)
        make_transition (layer, video_start, video_duration, NULL);
      else
        make_unorthodox_transition (layer, video_start, video_duration,
            audio_start, audio_duration);
      audio_tr_list = audio_tr_list->next;
    } else {
      make_transition (layer, video_start, video_duration, (gchar *) "video");
    }
  }
  if (audio_tr_list) {
    for (audio_tr_list = audio_tr_list; audio_tr_list;
        audio_tr_list = audio_tr_list->next) {
      audio_infos = g_strsplit_set ((gchar *) audio_tr_list->data, "[],", -1);
      audio_start = g_ascii_strtoll (audio_infos[1], NULL, 0);
      audio_duration =
          (g_ascii_strtoll (audio_infos[2], NULL, 0) - audio_start);
      make_transition (layer, audio_start, audio_duration, (gchar *) "audio");
    }
  }
}

void
make_unorthodox_transition (GESTimelineLayer * layer, gint64 start,
    gint64 duration, gint64 audio_start, gint64 audio_duration)
{
  GESTimelineStandardTransition *tr;
  GList *trackobjects, *tmp;
  tr = ges_timeline_standard_transition_new_for_nick ((char *) "crossfade");
  g_object_set (tr, "start", (gint64) start, "duration",
      (gint64) duration, "in-point", (gint64) 0, NULL);
  ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
  trackobjects =
      ges_timeline_object_get_track_objects (GES_TIMELINE_OBJECT (tr));
  for (tmp = trackobjects; tmp; tmp = tmp->next) {
    GESTrackAudioTransition *obj;
    if (GES_IS_TRACK_AUDIO_TRANSITION (tmp->data)) {
      obj = (GESTrackAudioTransition *) tmp->data;
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), FALSE);
      g_object_set (obj, "start", (gint64) audio_start, "duration",
          (gint64) audio_duration, "in-point", (gint64) 0, NULL);
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), TRUE);
    }
  }
}

void
make_transition (GESTimelineLayer * layer, gint64 start, gint64 duration,
    gchar * type)
{
  GESTimelineStandardTransition *tr;
  tr = ges_timeline_standard_transition_new_for_nick ((char *) "crossfade");
  if (!g_strcmp0 (type, (gchar *) "video"))
    ges_timeline_standard_transition_set_video_only (GES_TIMELINE_OBJECT (tr),
        TRUE);
  else if (!g_strcmp0 (type, (gchar *) "audio"))
    ges_timeline_standard_transition_set_audio_only (GES_TIMELINE_OBJECT (tr),
        TRUE);
  g_object_set (tr, "start", (gint64) start, "duration",
      (gint64) duration, "in-point", (gint64) 0, NULL);
  ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
}

void
set_source_properties (GObject * src, GHashTable * table,
    GESTimelineLayer * layer)
{
  set_property (src, (gchar *) "start", (gchar *) g_hash_table_lookup (table,
          (gchar *) "start"));
  set_property (src, (gchar *) "duration",
      (gchar *) g_hash_table_lookup (table, (gchar *) "duration"));
  set_property (src, (gchar *) "in_point",
      (gchar *) g_hash_table_lookup (table, (gchar *) "in_point"));
  ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));
}

void
set_property (GObject * src, gchar * prop_name, gchar * prop_value)
{
  gint64 converted;
  gint64 real;
  prop_value = g_strsplit (prop_value, (gchar *) ")", (gint) 0)[1];
  converted = g_ascii_strtoll ((gchar *) prop_value, NULL, 0);
  g_object_set (src, prop_name, converted, NULL);
  g_object_get (src, prop_name, &real, NULL);
}

GHashTable *
parse_timeline_objects (GHashTable * sources_table, xmlXPathContextPtr xpathCtx)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  GHashTable *track_objects_table, *source_table, *new_table;
  int size, j;
  xmlNode *cur_node = NULL;
  gchar *id, *ref;
  gboolean simple;
  gchar *res;

  track_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/timeline-objects/timeline-object/factory-ref",
      xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    cur_node = nodes->nodeTab[j];
    id = (gchar *) xmlGetProp (cur_node, (xmlChar *) "id");
    cur_node = cur_node->next->next->children->next;
    simple = TRUE;
    for (cur_node = cur_node; cur_node; cur_node = cur_node->next->next) {
      ref = (gchar *) xmlGetProp (cur_node, (xmlChar *) "id");
      source_table = g_hash_table_lookup (sources_table, id);
      new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      if (simple && !cur_node->next->next) {
        res = (gchar *) "double";
        g_hash_table_insert (source_table, g_strdup ("mode"), g_strdup (res));
        g_hash_table_insert (track_objects_table, g_strdup (ref), source_table);
      } else {
        res = (gchar *) "simple";
        g_hash_table_foreach (source_table, (GHFunc) create_new_source_table,
            new_table);
        g_hash_table_insert (track_objects_table, g_strdup (ref), new_table);
        g_hash_table_insert (new_table, g_strdup ("mode"), g_strdup (res));
      }
      simple = FALSE;
    }
  }
  return track_objects_table;
}

void
create_new_source_table (gchar * key, gchar * value, GHashTable * table)
{
  g_hash_table_insert (table, g_strdup (key), g_strdup (value));
}

gboolean
create_tracks (xmlXPathContextPtr xpathCtx, GESTimeline * timeline)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  GHashTable *table;
  GESTrack *track;
  int j;
  int size;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/stream", xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    table = print_xpath_nodes (nodes->nodeTab[j]);
    track = create_track (table);
    g_hash_table_destroy (table);
    if (!add_to_timeline (track, timeline)) {
      return FALSE;
    }
  }
  return TRUE;
}

GHashTable *
print_xpath_nodes (xmlNodePtr node)
{
  xmlAttr *cur_attr;
  GHashTable *props_table;
  gchar *name, *value;

  props_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
    name = (gchar *) cur_attr->name;
    value = (gchar *) xmlGetProp (node, cur_attr->name);
    g_hash_table_insert (props_table, g_strdup (name), g_strdup (value));
  }
  return props_table;
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri)
{
  xmlDocPtr doc;
  GESTimelineLayer *layer;
  gboolean ret = TRUE;
  xmlXPathContextPtr xpathCtx;
  GHashTable *source_table;

  layer = ges_timeline_layer_new ();
  g_object_set (layer, "priority", (gint32) 0, NULL);
  ges_timeline_add_layer (timeline, layer);
  doc = create_doc (uri);
  xpathCtx = xmlXPathNewContext (doc);
  if (!create_tracks (xpathCtx, timeline)) {
    ret = FALSE;
    goto bed;
  }
  source_table = list_sources (xpathCtx, layer);
  source_table = parse_timeline_objects (source_table, xpathCtx);
  parse_track_objects (xpathCtx, layer, source_table);
bed:
  xmlFreeDoc (doc);
  return ret;
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
