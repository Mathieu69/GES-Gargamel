#include <ges/ges.h>

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);

static xmlDocPtr create_doc (const gchar * uri);

static GHashTable *get_nodes_infos (xmlNodePtr nodes);
static gboolean create_tracks (GESTimeline * timeline);
static GHashTable *list_sources (xmlXPathContextPtr xpathCtx);
static gboolean parse_track_objects (xmlXPathContextPtr xpathCtx,
    GList * layers_list, GHashTable * sources_table, GESTimeline * timeline);
static gboolean parse_timeline_objects (GHashTable * sources_table,
    xmlXPathContextPtr xpathCtx, GHashTable * track_objects_table);
static gboolean calculate_transitions (GESTimelineLayer * layer);
static void save_tracks (GESTimeline * timeline, xmlTextWriterPtr writer,
    GList * source_list);
static GList *save_sources (GESTimelineLayer * layer, xmlTextWriterPtr writer);
static void save_track_objects (xmlTextWriterPtr writer, GList * source_list,
    gchar * res, gint * id);
static void save_timeline_objects (xmlTextWriterPtr writer, GList * list);
static void destroy_all (GList * list);
static void create_new_source_table (gchar * key, gchar * value,
    GHashTable * table);
static void make_timeline_objects (GESTimeline * timeline,
    GHashTable * timeline_objects_table, GHashTable * track_objects_table,
    GHashTable * source_table, GList * layers_list);
void set_properties (GObject * obj, GHashTable * props_table);
void make_source (GESTimeline * timeline, GList * ref_list,
    GHashTable * source_table, GHashTable * track_objects_table,
    GList * layers_list);

static void remove_track_object_cb (GESTimelineObject * object,
    GESTrack * track, gchar * last_type);

static void all_added_cb (GESTrack * track,
    GESTrackObject * tck_object, GESTimeline * timeline);
static void track_object_added_cb (GESTimelineObject * object,
    GESTrack * tck, GHashTable * props_table);

GESTrack *TRACKA, *TRACKV;
gint NOT_DONE = 0;
gboolean PARSED = FALSE;

static void
ges_pitivi_formatter_class_init (GESPitiviFormatterClass * klass)
{
  GESFormatterClass *formatter_klass;

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
  GList *list = NULL, *layers = NULL, *tmp = NULL;

  writer = xmlNewTextWriterFilename (uri, 0);

  xmlTextWriterSetIndent (writer, 1);
  xmlTextWriterStartElement (writer, BAD_CAST "pitivi");

  layers = ges_timeline_get_layers (timeline);
  xmlTextWriterStartElement (writer, BAD_CAST "factories");
  xmlTextWriterStartElement (writer, BAD_CAST "sources");

  for (tmp = layers; tmp; tmp = tmp->next) {

    // 99 is the priority of the background source.
    if (ges_timeline_layer_get_priority (tmp->data) != 99) {
      list = save_sources (tmp->data, writer);
    }
  }
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
  save_tracks (timeline, writer, list);
  save_timeline_objects (writer, list);

  xmlTextWriterEndDocument (writer);
  xmlFreeTextWriter (writer);

  g_list_free (layers);
  g_list_foreach (list, (GFunc) destroy_all, NULL);
  g_list_free (list);
  return TRUE;
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri)
{
  xmlDocPtr doc;
  GESTimelineLayer *layer;
  xmlXPathContextPtr xpathCtx;
  GHashTable *source_table, *track_objects_table, *timeline_objects_table;

  GList *layers_list = NULL;
  gboolean ret = TRUE;

  track_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  timeline_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  layer = ges_timeline_layer_new ();
  layers_list = g_list_append (layers_list, layer);
  g_object_set (layer, "priority", (gint32) 0, NULL);

  if (!ges_timeline_add_layer (timeline, layer)) {
    GST_ERROR ("Couldn't add layer");
    return FALSE;
  }

  doc = create_doc (uri);

  if (doc == NULL) {
    GST_ERROR ("Couldn't parse document successfully");
    return FALSE;
  }
  xpathCtx = xmlXPathNewContext (doc);

  if (!create_tracks (timeline)) {
    GST_ERROR ("Couldn't create tracks");
    ret = FALSE;
    goto fail;
  }

  source_table = list_sources (xpathCtx);

  if (!parse_timeline_objects (source_table, xpathCtx, timeline_objects_table)) {
    ret = FALSE;
    goto fail;
  }

  if (!parse_track_objects (xpathCtx, layers_list, track_objects_table,
          timeline)) {
    GST_ERROR ("Couldn't create objects");
    ret = FALSE;
  }

  make_timeline_objects (timeline, timeline_objects_table, track_objects_table,
      source_table, layers_list);
  //g_hash_table_foreach (track_objects_table, (GHFunc) destroy_tables, NULL);

//objects_fail:
  //g_hash_table_foreach (source_table, (GHFunc) destroy_tables, NULL);
  //g_hash_table_destroy (source_table);
  //g_hash_table_destroy (track_objects_table);

fail:
  xmlXPathFreeContext (xpathCtx);
  xmlFreeDoc (doc);
  g_list_free (layers_list);
  return ret;
}

static void
save_timeline_objects (xmlTextWriterPtr writer, GList * list)
{
  GList *tmp;

  xmlTextWriterStartElement (writer, BAD_CAST "timeline-objects");

  for (tmp = list; tmp; tmp = tmp->next) {

    GList *elem;
    xmlChar *cast;

    xmlTextWriterStartElement (writer, BAD_CAST "timeline-object");
    elem = tmp->data;
    xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
    cast = g_list_first (elem)->data;
    xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
    xmlTextWriterEndElement (writer);
    xmlTextWriterStartElement (writer, BAD_CAST "track-object-refs");
    xmlTextWriterStartElement (writer, BAD_CAST "track-object-ref");

    if (g_list_length (elem) == 6) {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 4)->data));
      xmlTextWriterEndElement (writer);
      xmlTextWriterStartElement (writer, BAD_CAST "track-object-ref");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 5)->data));
    } else {
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id",
          BAD_CAST (gchar *) (g_list_nth (elem, (guint) 4)->data));
    }

    xmlTextWriterEndElement (writer);
    xmlTextWriterEndElement (writer);
    xmlTextWriterEndElement (writer);
  }

  xmlTextWriterEndElement (writer);
}

static GList *
save_sources (GESTimelineLayer * layer, xmlTextWriterPtr writer)
{
  GList *objects, *tmp;
  GHashTable *source_table;

  GList *source_list = NULL;
  int id = 1;
  objects = ges_timeline_layer_get_objects (layer);
  source_table = g_hash_table_new_full (g_str_hash, g_int_equal, NULL, g_free);

  for (tmp = objects; tmp; tmp = tmp->next) {
    GList *ref_type_list = NULL;
    GESTimelineObject *object;
    gchar *tfs_uri;
    xmlChar *cast;
    object = tmp->data;

    if GES_IS_TIMELINE_TEST_SOURCE
      (object) {
      tfs_uri = (gchar *) "test";

      if (!g_hash_table_lookup (source_table, tfs_uri)) {
        cast = xmlXPathCastNumberToString (id);
        g_hash_table_insert (source_table, tfs_uri, g_strdup ((gchar *) cast));
        xmlFree (cast);
      } else {
        tfs_uri = (gchar *) "test2";
        cast = xmlXPathCastNumberToString (id);
        g_hash_table_insert (source_table, tfs_uri, g_strdup ((gchar *) cast));
        xmlFree (cast);
      }

      xmlTextWriterStartElement (writer, BAD_CAST "source");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "filename",
          BAD_CAST tfs_uri);
      cast = xmlXPathCastNumberToString (id);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
      xmlFree (cast);
      xmlTextWriterEndElement (writer);
      id++;
      ref_type_list =
          g_list_append (ref_type_list,
          g_strdup (g_hash_table_lookup (source_table, tfs_uri)));
      ref_type_list = g_list_append (ref_type_list, object);
      ref_type_list = g_list_append (ref_type_list, g_strdup ("simple"));
      ref_type_list =
          g_list_append (ref_type_list,
          GINT_TO_POINTER (ges_timeline_layer_get_priority (layer)));
      source_list = g_list_append (source_list, g_list_copy (ref_type_list));
      g_list_free (ref_type_list);

    } else if GES_IS_TIMELINE_FILE_SOURCE
      (object) {
      tfs_uri = (gchar *) ges_timeline_filesource_get_uri
          (GES_TIMELINE_FILE_SOURCE (object));

      if (!g_hash_table_lookup (source_table, tfs_uri)) {
        cast = xmlXPathCastNumberToString (id);
        g_hash_table_insert (source_table, tfs_uri, g_strdup ((gchar *) cast));
        xmlFree (cast);
        xmlTextWriterStartElement (writer, BAD_CAST "source");
        xmlTextWriterWriteAttribute (writer, BAD_CAST "filename",
            BAD_CAST tfs_uri);
        cast = xmlXPathCastNumberToString (id);
        xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
        xmlFree (cast);
        xmlTextWriterEndElement (writer);
        id++;
      }

      ref_type_list =
          g_list_append (ref_type_list,
          g_strdup (g_hash_table_lookup (source_table, tfs_uri)));
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

      ref_type_list =
          g_list_append (ref_type_list,
          GINT_TO_POINTER (ges_timeline_layer_get_priority (layer)));
      source_list = g_list_append (source_list, g_list_copy (ref_type_list));
      g_list_free (ref_type_list);
      }
  }

  g_object_unref (G_OBJECT (layer));
  g_list_free (objects);
  g_hash_table_destroy (source_table);
  return source_list;
}

static void
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
    g_free (caps);

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
    g_free (type);
    save_track_objects (writer, source_list, res, &id);
    xmlTextWriterEndElement (writer);
  }

  g_list_free (tracks);
  xmlTextWriterEndElement (writer);
}

static void
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
      xmlChar *cast;
      gchar *prio_str;
      xmlTextWriterStartElement (writer, BAD_CAST "track-object");
      cast =
          xmlXPathCastNumberToString (GPOINTER_TO_INT (g_list_nth (elem,
                  (guint) 3)->data));
      prio_str = g_strconcat ((gchar *) "(int)", (gchar *) cast, NULL);
      xmlFree (cast);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "priority",
          BAD_CAST prio_str);
      g_free (prio_str);
      object = g_list_next (elem)->data;
      properties =
          g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &n);

      for (i = 0; i < n; i++) {
        GParamSpec *p = properties[i];
        GValue v = { 0 };
        gchar *serialized, *concatenated;

        if (!g_strcmp0 (p->name, (gchar *) "duration") ||
            !g_strcmp0 (p->name, (gchar *) "start") ||
            !g_strcmp0 (p->name, (gchar *) "in-point")) {
          g_value_init (&v, p->value_type);
          g_object_get_property (G_OBJECT (object), p->name, &v);
          serialized = gst_value_serialize (&v);
          concatenated = g_strconcat ((gchar *) "(gint64)", serialized, NULL);

          if (!g_strcmp0 (p->name, (gchar *) "in-point")) {
            xmlTextWriterWriteAttribute (writer, BAD_CAST (gchar *) "in_point",
                BAD_CAST concatenated);
          } else {
            xmlTextWriterWriteAttribute (writer, BAD_CAST p->name,
                BAD_CAST concatenated);
          }
          g_free (concatenated);
          g_free (serialized);
        }
      }
      g_free (properties);
      cast = xmlXPathCastNumberToString (*id);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
      xmlFree (cast);
      xmlTextWriterEndElement (writer);
      xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
      cast = g_list_first (elem)->data;
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
      xmlTextWriterEndElement (writer);
      elem = g_list_append (elem, xmlXPathCastNumberToString (*id));
      *id = *id + 1;
    }
  }

  xmlTextWriterEndElement (writer);
}

static xmlDocPtr
create_doc (const gchar * uri)
{
  xmlDocPtr doc;
  doc = xmlParseFile (uri);
  return doc;
}

static GHashTable *
list_sources (xmlXPathContextPtr xpathCtx)
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
    table = get_nodes_infos (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    g_hash_table_insert (sources_table, g_strdup (id), table);
  }

  xmlXPathFreeObject (xpathObj);
  return sources_table;
}

static void
make_timeline_objects (GESTimeline * timeline,
    GHashTable * timeline_objects_table, GHashTable * track_objects_table,
    GHashTable * sources_table, GList * layers_list)
{
  GHashTable *source_table;
  GESTimelineTestSource *background;
  GESTimelineLayer *layer;
  gint64 duration;
  int i;

  GList *keys = NULL, *tmp = NULL, *ref_list = NULL;

  background = ges_timeline_test_source_new ();
  layer = ges_timeline_layer_new ();
  ges_timeline_layer_set_priority (layer, 99);
  ges_timeline_add_layer (timeline, layer);

  ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (background));

  keys = g_hash_table_get_keys (timeline_objects_table);

  g_signal_connect (TRACKA, "track-object-added",
      G_CALLBACK (all_added_cb), timeline);
  g_signal_connect (TRACKV, "track-object-added",
      G_CALLBACK (all_added_cb), timeline);

  for (tmp = keys, i = 1; tmp; tmp = tmp->next, i++) {
    if (i == g_list_length (keys))
      PARSED = TRUE;
    ref_list =
        g_hash_table_lookup (timeline_objects_table, (gchar *) tmp->data);
    source_table = g_hash_table_lookup (sources_table, (gchar *) tmp->data);
    make_source (timeline, ref_list, source_table, track_objects_table,
        layers_list);
  }

  g_object_get (TRACKV, "duration", &duration, NULL);
}

void
make_source (GESTimeline * timeline, GList * ref_list,
    GHashTable * source_table, GHashTable * track_objects_table,
    GList * layers_list)
{
  GHashTable *props_table;
  gchar **prio_array;
  GESTimelineLayer *layer;

  gchar *fac_ref = NULL, *media_type = NULL, *last_type = NULL, *filename =
      NULL, *saved_start = NULL;
  GList *tmp = NULL, *tmpl = NULL, *a_effect_list = NULL, *v_effect_list = NULL;
  gboolean ungrouped = FALSE;
  GESTimelineFileSource *src = NULL;
  GESTimelineTestSource *testsrc = NULL;
  gint i = 0, cast_prio = 0;

  if (!g_strcmp0 (ref_list->data, (gchar *) "ungrouped")) {
    ref_list = ref_list->next;
    ungrouped = TRUE;
  }

  props_table =
      g_hash_table_lookup (track_objects_table, (gchar *) ref_list->data);
  prio_array =
      g_strsplit ((gchar *) g_hash_table_lookup (props_table,
          (gchar *) "priority"), ")", 0);
  cast_prio = (gint) g_ascii_strtod (prio_array[1], NULL);
  g_strfreev (prio_array);

  if (!(tmpl = g_list_nth (layers_list, cast_prio))) {
    layer = ges_timeline_layer_new ();
    ges_timeline_layer_set_priority (layer, cast_prio);
    ges_timeline_add_layer (timeline, layer);
    layers_list = g_list_append (layers_list, layer);
  } else {
    layer = tmpl->data;
  }

  for (tmp = ref_list; tmp; tmp = tmp->next) {
    props_table =
        g_hash_table_lookup (track_objects_table, (gchar *) tmp->data);
    fac_ref = (gchar *) g_hash_table_lookup (props_table, (gchar *) "fac_ref");
    media_type =
        (gchar *) g_hash_table_lookup (props_table, (gchar *) "media_type");

    if (g_strcmp0 (fac_ref, (gchar *) "effect") && i == 0) {
      i++;
      filename =
          (gchar *) g_hash_table_lookup (source_table, (gchar *) "filename");
      if (!g_strcmp0 (filename, "test") || !g_strcmp0 (filename, "test2")) {
        testsrc = ges_timeline_test_source_new ();
        g_object_set (testsrc, "volume", (gdouble) 1, NULL);
        set_properties (G_OBJECT (testsrc), props_table);
        if (!g_strcmp0 (filename, "test")) {
          ges_timeline_test_source_set_vpattern (testsrc,
              GES_VIDEO_TEST_PATTERN_RED);
          g_object_set (testsrc, "freq", (gdouble) 880, NULL);
        }

      } else {
        src = ges_timeline_filesource_new ((gchar *)
            g_hash_table_lookup (source_table, (gchar *) "filename"));
        set_properties (G_OBJECT (src), props_table);
        ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));
      }
      NOT_DONE++;
      NOT_DONE++;
      saved_start =
          (gchar *) g_hash_table_lookup (props_table, (gchar *) "start");
      last_type = g_strdup (media_type);

    } else if (g_strcmp0 (fac_ref, (gchar *) "effect") && i == 1 && ungrouped) {
      i++;
      if (src != NULL) {
        g_signal_connect (src, "track-object-added",
            G_CALLBACK (track_object_added_cb), props_table);
      } else {
        g_signal_connect (testsrc, "track-object-added",
            G_CALLBACK (track_object_added_cb), props_table);
      }

    } else if (g_strcmp0 (fac_ref, (gchar *) "effect") && i == 1) {
      i++;

    } else if (!g_strcmp0 (fac_ref, (gchar *) "effect")) {
      if (!g_strcmp0 (media_type, (gchar *) "pitivi.stream.AudioStream")) {
        a_effect_list =
            g_list_prepend (a_effect_list,
            ges_track_parse_launch_effect_new ((gchar *)
                g_hash_table_lookup (props_table, (gchar *) "effect_name")));

      } else {
        v_effect_list =
            g_list_prepend (v_effect_list,
            ges_track_parse_launch_effect_new ((gchar *)
                g_hash_table_lookup (props_table, (gchar *) "effect_name")));
      }
    }
  }

  for (tmp = a_effect_list; tmp; tmp = tmp->next) {
    ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (src),
        GES_TRACK_OBJECT (tmp->data));
    ges_track_add_object (TRACKA, GES_TRACK_OBJECT (tmp->data));
  }

  for (tmp = v_effect_list; tmp; tmp = tmp->next) {
    if (src != NULL) {
      ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (src),
          GES_TRACK_OBJECT (tmp->data));
    } else {
      ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (testsrc),
          GES_TRACK_OBJECT (tmp->data));
    }
    ges_track_add_object (TRACKV, GES_TRACK_OBJECT (tmp->data));
  }

  if (i == 1 && (!ungrouped)) {
    g_signal_connect (src, "track-object-added",
        G_CALLBACK (remove_track_object_cb), last_type);
  }

  if (testsrc != NULL) {
    ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (testsrc));
  }
}

void
remove_track_object_cb (GESTimelineObject * object, GESTrack * track,
    gchar * last_type)
{
  GList *tck_objs = NULL, *tmp = NULL;

  tck_objs = ges_timeline_object_get_track_objects (object);

  for (tmp = tck_objs; tmp; tmp = tmp->next) {
    if ((ges_track_object_get_track (tmp->data)->type == GES_TRACK_TYPE_VIDEO &&
            (!g_strcmp0 (last_type, (gchar *) "pitivi.stream.AudioStream"))) ||
        (ges_track_object_get_track (tmp->data)->type == GES_TRACK_TYPE_AUDIO &&
            (!g_strcmp0 (last_type, (gchar *) "pitivi.stream.VideoStream")))) {
      ges_track_object_set_active (tmp->data, FALSE);
      g_signal_handlers_disconnect_by_func (object, remove_track_object_cb,
          last_type);
    }
  }
}


void
set_properties (GObject * obj, GHashTable * props_table)
{
  gint i;
  gchar **prop_array;
  gint64 prop_value;

  gchar list[3][10] = { "duration", "in_point", "start" };

  for (i = 0; i < 3; i++) {
    prop_array =
        g_strsplit ((gchar *) g_hash_table_lookup (props_table, list[i]),
        (gchar *) ")", (gint) 0);
    prop_value = g_ascii_strtoll ((gchar *) prop_array[1], NULL, 0);
    g_object_set (obj, list[i], prop_value, NULL);
  }
}

static gboolean
parse_track_objects (xmlXPathContextPtr xpathCtx, GList * layers_list,
    GHashTable * track_objects_table, GESTimeline * timeline)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  gchar *id, *fac_ref;
  GHashTable *table, *new_table;
  xmlNode *ref_node;
  gchar *media_type;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/track-objects/track-object", xpathCtx);

  if (xpathObj == NULL) {
    xmlXPathFreeObject (xpathObj);
    return FALSE;
  }

  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    table = get_nodes_infos (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    ref_node = nodes->nodeTab[j]->children->next;
    fac_ref = (gchar *) xmlGetProp (ref_node, (xmlChar *) "id");
    new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    if (!g_strcmp0 ((gchar *) ref_node->name, (gchar *) "effect")) {
      fac_ref = (gchar *) "effect";
      ref_node = ref_node->children->next;
      g_hash_table_insert (table, (gchar *) "effect_name",
          (gchar *) xmlGetProp (ref_node, (xmlChar *) "name"));
    }

    g_hash_table_insert (table, (gchar *) "fac_ref", g_strdup (fac_ref));
    media_type =
        (gchar *) xmlGetProp (nodes->nodeTab[j]->parent->prev->prev,
        (xmlChar *) "type");
    g_hash_table_insert (table, (gchar *) "media_type", g_strdup (media_type));
    g_hash_table_foreach (table, (GHFunc) create_new_source_table, new_table);
    g_hash_table_insert (track_objects_table, g_strdup (id), new_table);
  }

  return TRUE;
}

static gboolean
calculate_transitions (GESTimelineLayer * layer)
{
  GESTimelineStandardTransition *tr;

  GList *tl_objects = NULL, *tck_objects = NULL;
  GList *tmp = NULL, *tmp_tck = NULL;
  gint64 prev_video_end = 0, prev_audio_end = 0;
  gboolean v_avail = FALSE, a_avail = FALSE;

  tl_objects = ges_timeline_layer_get_objects (layer);

  for (tmp = tl_objects; tmp; tmp = tmp->next) {
    gint64 duration, start, in_point;

    tck_objects = ges_timeline_object_get_track_objects (tmp->data);
    for (tmp_tck = tck_objects; tmp_tck; tmp_tck = tmp_tck->next) {

      if GES_IS_TRACK_PARSE_LAUNCH_EFFECT
        (tmp_tck->data) {
        continue;
        }

      if (ges_track_object_get_track (tmp_tck->data)->type ==
          GES_TRACK_TYPE_VIDEO) {
        g_object_get (tmp_tck->data, "duration", &duration, NULL);
        g_object_get (tmp_tck->data, "start", &start, NULL);
        g_object_get (tmp_tck->data, "in_point", &in_point, NULL);

        if (start < prev_video_end && (!v_avail)) {
          tr = ges_timeline_standard_transition_new_for_nick ((char *)
              "crossfade");
          g_object_set (tr, "start", (gint64) start, "duration",
              (gint64) prev_video_end - start, "in_point", (gint64) 0, NULL);
          //ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
          printf ("transition : %lld, %lld\n", start, prev_video_end - start);
          a_avail = TRUE;
        } else if (start < prev_video_end) {
          v_avail = FALSE;
          a_avail = FALSE;
        }

        prev_video_end = duration + start - in_point;

      } else {
        g_object_get (tmp_tck->data, "duration", &duration, NULL);
        g_object_get (tmp_tck->data, "start", &start, NULL);
        g_object_get (tmp_tck->data, "in_point", &in_point, NULL);

        if (start < prev_audio_end && (!a_avail)) {
          v_avail = TRUE;
        } else if (start < prev_audio_end) {
          a_avail = FALSE;
          v_avail = FALSE;
        }

        prev_audio_end = duration + start - in_point;
      }
    }
  }

  g_list_free (tl_objects);
  return TRUE;
}

static gboolean
parse_timeline_objects (GHashTable * sources_table, xmlXPathContextPtr xpathCtx,
    GHashTable * timeline_objects_table)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  gchar *id, *ref;

  GList *ref_list = NULL, *tmp_list = NULL, *tmp = NULL;
  xmlNode *cur_node = NULL;
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/timeline-objects/timeline-object/factory-ref",
      xpathCtx);

  if (xpathObj == NULL) {
    xmlXPathFreeObject (xpathObj);
    return FALSE;
  }

  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    cur_node = nodes->nodeTab[j];
    id = (gchar *) xmlGetProp (cur_node, (xmlChar *) "id");
    cur_node = cur_node->next->next->children->next;
    ref_list = NULL;

    for (cur_node = cur_node; cur_node; cur_node = cur_node->next->next) {
      ref = (gchar *) xmlGetProp (cur_node, (xmlChar *) "id");
      ref_list = g_list_prepend (ref_list, g_strdup (ref));
      xmlFree (ref);
    }
    tmp_list = g_hash_table_lookup (timeline_objects_table, id);
    if (tmp_list != NULL) {
      for (tmp = tmp_list; tmp; tmp = tmp->next) {
        ref_list = g_list_prepend (ref_list, tmp->data);
      }
      ref_list = g_list_prepend (ref_list, (gchar *) "ungrouped");
    }
    g_hash_table_insert (timeline_objects_table, g_strdup (id),
        g_list_copy (ref_list));
    xmlFree (id);
  }

  xmlXPathFreeObject (xpathObj);
  return TRUE;
}

static void
create_new_source_table (gchar * key, gchar * value, GHashTable * table)
{
  g_hash_table_insert (table, g_strdup (key), g_strdup (value));
}

static gboolean
create_tracks (GESTimeline * timeline)
{
  TRACKA = ges_track_audio_raw_new ();
  TRACKV = ges_track_video_raw_new ();

  if (!ges_timeline_add_track (timeline, TRACKA)) {
    return FALSE;
  }

  if (!ges_timeline_add_track (timeline, TRACKV)) {
    return FALSE;
  }

  return TRUE;
}

static GHashTable *
get_nodes_infos (xmlNodePtr node)
{
  xmlAttr *cur_attr;
  GHashTable *props_table;
  gchar *name, *value;

  props_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  for (cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
    name = (gchar *) cur_attr->name;
    value = (gchar *) xmlGetProp (node, cur_attr->name);
    g_hash_table_insert (props_table, g_strdup (name), g_strdup (value));
    xmlFree (value);
  }

  return props_table;
}

static void
destroy_all (GList * list)
{
  g_free (g_list_nth (list, (guint) 2)->data);
  g_object_unref (G_OBJECT (g_list_nth (list, (guint) 1)->data));
  g_free (g_list_nth (list, (guint) 3)->data);
  g_free (g_list_nth (list, (guint) 4)->data);

  if (g_list_length (list) == 6) {
    g_free (g_list_nth (list, (guint) 5)->data);
  }

  g_free (g_list_nth (list, (guint) 0)->data);
  g_list_free (list);
}

static void
track_object_added_cb (GESTimelineObject * object,
    GESTrack * track, GHashTable * props_table)
{
  gchar *media_type;
  GList *tck_objs = NULL, *tmp = NULL;

  tck_objs = ges_timeline_object_get_track_objects (object);
  media_type =
      (gchar *) g_hash_table_lookup (props_table, (gchar *) "media_type");

  if (track->type == GES_TRACK_TYPE_AUDIO) {
    for (tmp = tck_objs; tmp; tmp = tmp->next) {
      if ((!g_strcmp0 (media_type, "pitivi.stream.VideoStream")
              && ges_track_object_get_track (tmp->data)->type ==
              GES_TRACK_TYPE_VIDEO)
          || (!g_strcmp0 (media_type, "pitivi.stream.AudioStream")
              && ges_track_object_get_track (tmp->data)->type ==
              GES_TRACK_TYPE_AUDIO)) {
        ges_track_object_set_locked (tmp->data, FALSE);
        set_properties (G_OBJECT (tmp->data), props_table);
      }
    }
  }
}

static void
all_added_cb (GESTrack * track,
    GESTrackObject * tck_object, GESTimeline * timeline)
{
  gint64 dur_a, dur_v;
  GList *layers = NULL, *tmp = NULL, *objects = NULL;

  if (!GES_IS_TRACK_SOURCE (tck_object)) {
    return;
  }

  layers = ges_timeline_get_layers (timeline);
  NOT_DONE--;

  if (!NOT_DONE && PARSED) {

    for (tmp = layers; tmp; tmp = tmp->next) {

      if (ges_timeline_layer_get_priority (tmp->data) != 99) {
        calculate_transitions (tmp->data);
      } else {
        objects = ges_timeline_layer_get_objects (tmp->data);
        g_object_get (TRACKV, "duration", &dur_v, NULL);
        g_object_get (TRACKA, "duration", &dur_a, NULL);

        if (dur_a > dur_v) {
          g_object_set (objects->data, "duration", dur_a, NULL);
        } else {
          g_object_set (objects->data, "duration", dur_v, NULL);
        }
      }
    }
  }
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
