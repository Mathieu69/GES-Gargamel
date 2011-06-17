#include <ges/ges.h>

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_timeline_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * self,
    GESTimeline * timeline, const gchar * uri);

static xmlDocPtr create_doc (const gchar * uri);

static GHashTable *get_nodes_infos (xmlNodePtr nodes);
static gboolean create_tracks (GESPitiviFormatterPrivate * priv);
static GHashTable *list_sources (GESPitiviFormatterPrivate * priv);
static gboolean parse_track_objects (GESPitiviFormatterPrivate * priv);
static gboolean parse_timeline_objects (GESPitiviFormatterPrivate * priv);
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
static gboolean make_timeline_objects (GESPitiviFormatterPrivate * priv);
void set_properties (GObject * obj, GHashTable * props_table);
void make_source (GList * ref_list,
    GHashTable * source_table, GESPitiviFormatterPrivate * priv);
void modify_transition (GESTimelineStandardTransition * tr, gint64 start,
    gint64 duration, gint type);

static void track_object_added_cb (GESTimelineObject * object,
    GESTrack * tck, GHashTable * props_table);

struct _GESPitiviFormatterPrivate
{
  gint not_done;
  xmlXPathContextPtr xpathCtx;
  GHashTable *source_table, *track_objects_table, *timeline_objects_table,
      *layers_table;
  GESTimeline *timeline;
  gboolean parsed;
  GESTrack *tracka, *trackv;
};

static void
ges_pitivi_formatter_class_init (GESPitiviFormatterClass * klass)
{
  GESFormatterClass *formatter_klass;

  formatter_klass = GES_FORMATTER_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GESPitiviFormatterPrivate));

  formatter_klass->save_to_uri = save_pitivi_timeline_to_uri;
  formatter_klass->load_from_uri = load_pitivi_file_from_uri;
}

static void
ges_pitivi_formatter_init (GESPitiviFormatter * self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GES_TYPE_PITIVI_FORMATTER, GESPitiviFormatterPrivate);

  self->priv->not_done = 0;

  self->priv->track_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->priv->timeline_objects_table =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->priv->layers_table =
      g_hash_table_new_full (g_int_hash, g_str_equal, NULL, NULL);

  self->priv->parsed = FALSE;
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
  printf ("done !\n");
  return TRUE;
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * self,
    GESTimeline * timeline, const gchar * uri)
{
  xmlDocPtr doc;
  GESTimelineLayer *layer;
  GESPitiviFormatterPrivate *priv = GES_PITIVI_FORMATTER (self)->priv;

  gboolean ret = TRUE;
  gint *prio = malloc (sizeof (gint));

  *prio = 0;
  layer = ges_timeline_layer_new ();

  g_hash_table_insert (priv->layers_table, prio, layer);
  priv->timeline = timeline;
  g_object_set (layer, "priority", (gint32) 0, NULL);

  if (!ges_timeline_add_layer (timeline, layer)) {
    GST_ERROR ("Couldn't add layer");
    return FALSE;
  }

  if (!(doc = create_doc (uri))) {
    GST_ERROR ("The xptv file for uri %s was badly formed or did not exist",
        uri);
    return FALSE;
  }

  priv->xpathCtx = xmlXPathNewContext (doc);

  if (!create_tracks (priv)) {
    GST_ERROR ("Couldn't create tracks");
    ret = FALSE;
    goto fail;
  }

  priv->source_table = list_sources (priv);

  if (!parse_timeline_objects (priv)) {
    GST_ERROR ("Couldn't find timeline objects markup in the xptv file");
    ret = FALSE;
    goto fail;
  }
  if (!parse_track_objects (priv)) {
    GST_ERROR ("Couldn't find track objects markup in the xptv file");
    ret = FALSE;
  }

  if (!make_timeline_objects (priv))
    ret = FALSE;

fail:
  xmlXPathFreeContext (priv->xpathCtx);
  xmlFreeDoc (doc);
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
  GList *objects, *tmp, *tmp_tck;
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
      printf ("pupute\n");
      for (tmp_tck = ges_timeline_object_get_track_objects (object); tmp_tck;
          tmp_tck = tmp_tck->next) {
        printf ("un obj dans pupute\n");
      }
      }
    if GES_IS_TIMELINE_FILE_SOURCE
      (object) {
      tfs_uri = (gchar *) ges_timeline_filesource_get_uri
          (GES_TIMELINE_FILE_SOURCE (object));
      printf ("pupute\n");
      for (tmp_tck = ges_timeline_object_get_track_objects (object); tmp_tck;
          tmp_tck = tmp_tck->next) {
        printf ("un obj dans pupute\n");
      }

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
      ref_type_list = g_list_append (ref_type_list, g_strdup ("simple"));
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
      xmlTextWriterStartElement (writer, BAD_CAST "factory-ref");
      cast = g_list_first (elem)->data;
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST cast);
      xmlTextWriterEndElement (writer);
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
list_sources (GESPitiviFormatterPrivate * priv)
{
  xmlXPathObjectPtr xpathObj;
  GHashTable *table, *sources_table;
  int size, j;
  gchar *id;
  xmlNodeSetPtr nodes;

  sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/factories/sources/source", priv->xpathCtx);
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

static gboolean
make_timeline_objects (GESPitiviFormatterPrivate * priv)
{
  GHashTable *source_table;
  GESTimelineTestSource *background;
  GESTimelineLayer *back_layer;
  gint i;
  gint *prio = malloc (sizeof (gint));

  GList *keys = NULL, *tmp = NULL, *ref_list = NULL;

  *prio = 0;

  background = ges_timeline_test_source_new ();
  back_layer = ges_timeline_layer_new ();
  ges_timeline_layer_set_priority (back_layer, 99);
  if (!ges_timeline_add_layer (priv->timeline, back_layer)) {
    GST_ERROR ("Couldn't add layer");
    return FALSE;
  }

  if (!ges_timeline_layer_add_object (back_layer,
          GES_TIMELINE_OBJECT (background))) {
    GST_ERROR ("Couldn't add background to the layer");
    return FALSE;
  }

  keys = g_hash_table_get_keys (priv->timeline_objects_table);

  for (tmp = keys, i = 1; tmp; tmp = tmp->next, i++) {
    if (i == g_list_length (keys))
      priv->parsed = TRUE;
    ref_list =
        g_hash_table_lookup (priv->timeline_objects_table, (gchar *) tmp->data);
    source_table =
        g_hash_table_lookup (priv->source_table, (gchar *) tmp->data);
    make_source (ref_list, source_table, priv);
  }
  g_hash_table_insert (priv->layers_table, prio, back_layer);
  return TRUE;
}

void
make_source (GList * ref_list,
    GHashTable * source_table, GESPitiviFormatterPrivate * priv)
{
  GHashTable *props_table, *prev_props_table = NULL, *effect_table;
  gchar **prio_array;
  GESTimelineLayer *layer;

  gchar *fac_ref = NULL, *media_type = NULL, *filename =
      NULL, *prev_media_type = NULL;
  GList *tmp = NULL, *keys, *tmp_key;
  GESTimelineFileSource *src = NULL;
  gint cast_prio = 0;
  gint *prio = malloc (sizeof (gint));
  gboolean a_avail = FALSE, v_avail = FALSE, video = FALSE;

  for (tmp = ref_list; tmp; tmp = tmp->next) {
    props_table =
        g_hash_table_lookup (priv->track_objects_table, (gchar *) tmp->data);
    prio_array =
        g_strsplit ((gchar *) g_hash_table_lookup (props_table,
            (gchar *) "priority"), ")", 0);
    cast_prio = (gint) g_ascii_strtod (prio_array[1], NULL);
    *prio = cast_prio;
    fac_ref = (gchar *) g_hash_table_lookup (props_table, (gchar *) "fac_ref");
    media_type =
        (gchar *) g_hash_table_lookup (props_table, (gchar *) "media_type");

    g_strfreev (prio_array);

    if (!g_strcmp0 (media_type, (gchar *) "pitivi.stream.VideoStream"))
      video = TRUE;
    else
      video = FALSE;

    if (!(layer = g_hash_table_lookup (priv->layers_table, &cast_prio))) {
      layer = ges_timeline_layer_new ();
      ges_timeline_layer_set_priority (layer, cast_prio);
      ges_timeline_add_layer (priv->timeline, layer);
      g_hash_table_insert (priv->layers_table, prio, layer);
    }

    if (g_strcmp0 (fac_ref, (gchar *) "effect") && a_avail && (!video)) {
      a_avail = FALSE;
      priv->not_done++;
      priv->not_done++;
      g_hash_table_insert (props_table, (gchar *) "private", priv);
      g_signal_connect (src, "track-object-added",
          G_CALLBACK (track_object_added_cb), props_table);

    } else if (g_strcmp0 (fac_ref, (gchar *) "effect") && v_avail && (video)) {
      v_avail = FALSE;
      priv->not_done++;
      priv->not_done++;
      g_hash_table_insert (props_table, (gchar *) "private", priv);
      g_signal_connect (src, "track-object-added",
          G_CALLBACK (track_object_added_cb), props_table);

    } else if (g_strcmp0 (fac_ref, (gchar *) "effect")) {
      if ((a_avail || v_avail)) {
        priv->not_done++;
        priv->not_done++;
        if (!g_hash_table_lookup (prev_props_table, (gchar *) "private")) {
          g_hash_table_insert (prev_props_table, (gchar *) "private", priv);
        }
        g_hash_table_insert (prev_props_table, (gchar *) "remove",
            g_strdup (prev_media_type));
        g_signal_connect (src, "track-object-added",
            G_CALLBACK (track_object_added_cb), prev_props_table);
      }
      filename =
          (gchar *) g_hash_table_lookup (source_table, (gchar *) "filename");
      src = ges_timeline_filesource_new ((gchar *)
          g_hash_table_lookup (source_table, (gchar *) "filename"));
      priv->not_done++;
      priv->not_done++;
      g_hash_table_insert (props_table, (gchar *) "private", priv);
      g_hash_table_insert (props_table, (gchar *) "transitions",
          (gchar *) "transitions");
      g_signal_connect (src, "track-object-added",
          G_CALLBACK (track_object_added_cb), props_table);
      if (!video) {
        v_avail = TRUE;
        a_avail = FALSE;
      } else {
        a_avail = TRUE;
        v_avail = FALSE;
      }
      set_properties (G_OBJECT (src), props_table);
      ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));

    } else if (!g_strcmp0 (fac_ref, (gchar *) "effect")) {
      GESTrackParseLaunchEffect *effect;
      gchar *active = (gchar *)
          g_hash_table_lookup (props_table, (gchar *) "active");
      effect = ges_track_parse_launch_effect_new ((gchar *)
          g_hash_table_lookup (props_table, (gchar *) "effect_name"));
      effect_table =
          g_hash_table_lookup (props_table, (gchar *) "effect_props");
      ges_timeline_object_add_track_object (GES_TIMELINE_OBJECT (src),
          GES_TRACK_OBJECT (effect));

      if (!g_strcmp0 (active, (gchar *) "(bool)False"))
        ges_track_object_set_active (GES_TRACK_OBJECT (effect), FALSE);

      if (video)
        ges_track_add_object (priv->trackv, GES_TRACK_OBJECT (effect));
      else
        ges_track_add_object (priv->tracka, GES_TRACK_OBJECT (effect));

      keys = g_hash_table_get_keys (effect_table);

      for (tmp_key = keys; tmp_key; tmp_key = tmp_key->next) {
        gchar **value_array =
            g_strsplit ((gchar *) g_hash_table_lookup (effect_table,
                (gchar *) tmp_key->data),
            (gchar *) ")", (gint) 0);
        gchar *value = value_array[1];

        if (!g_strcmp0 (value, (gchar *) "True"))
          ges_track_object_set_child_property (GES_TRACK_OBJECT (effect),
              (gchar *) tmp_key->data, TRUE, NULL);
        else if (!g_strcmp0 (value, (gchar *) "True"))
          ges_track_object_set_child_property (GES_TRACK_OBJECT (effect),
              (gchar *) tmp_key->data, FALSE, NULL);
        else if (!g_strcmp0 (value, (gchar *) "effect"))
          continue;
        else if (!g_strcmp0 ((gchar *) "(guint", value_array[0])
            || !g_strcmp0 ((gchar *) "(GEnum", value_array[0])
            || !g_strcmp0 ((gchar *) "(gint", value_array[0]))
          ges_track_object_set_child_property (GES_TRACK_OBJECT (effect),
              (gchar *) tmp_key->data, g_ascii_strtoll (value, NULL, 0), NULL);
        else
          ges_track_object_set_child_property (GES_TRACK_OBJECT (effect),
              (gchar *) tmp_key->data, g_ascii_strtod (value, NULL), NULL);
      }
    }
    prev_media_type = media_type;
    prev_props_table = props_table;
  }

  if ((a_avail || v_avail)) {
    priv->not_done++;
    priv->not_done++;
    if (!g_hash_table_lookup (props_table, (gchar *) "private")) {
      g_hash_table_insert (props_table, (gchar *) "private", priv);
    }
    g_hash_table_insert (props_table, (gchar *) "remove",
        g_strdup (media_type));
    g_signal_connect (src, "track-object-added",
        G_CALLBACK (track_object_added_cb), props_table);
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
    printf ("%s : %lld\n", list[i], prop_value);
    g_object_set (obj, list[i], prop_value, NULL);
  }
}

static gboolean
parse_track_objects (GESPitiviFormatterPrivate * priv)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  gchar *id, *fac_ref;
  GHashTable *table, *new_table, *effect_table;
  xmlNode *ref_node;
  gchar *media_type;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/track-objects/track-object",
      priv->xpathCtx);

  if (xpathObj == NULL) {
    xmlXPathFreeObject (xpathObj);
    return FALSE;
  }
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;

  for (j = 0; j < size; ++j) {
    GHashTable *new_effect_table = NULL;
    table = get_nodes_infos (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    ref_node = nodes->nodeTab[j]->children->next;
    fac_ref = (gchar *) xmlGetProp (ref_node, (xmlChar *) "id");
    new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    if (!g_strcmp0 ((gchar *) ref_node->name, (gchar *) "effect")) {
      fac_ref = (gchar *) "effect";
      ref_node = ref_node->children->next;
      new_effect_table =
          g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert (table, (gchar *) "effect_name",
          (gchar *) xmlGetProp (ref_node, (xmlChar *) "name"));
      effect_table = get_nodes_infos (ref_node->next->next);
      g_hash_table_foreach (effect_table, (GHFunc) create_new_source_table,
          new_effect_table);
    }

    g_hash_table_insert (table, (gchar *) "fac_ref", g_strdup (fac_ref));
    media_type =
        (gchar *) xmlGetProp (nodes->nodeTab[j]->parent->prev->prev,
        (xmlChar *) "type");
    g_hash_table_insert (table, (gchar *) "media_type", g_strdup (media_type));
    g_hash_table_foreach (table, (GHFunc) create_new_source_table, new_table);
    if (new_effect_table) {
      g_hash_table_insert (new_table, (gchar *) "effect_props",
          new_effect_table);
    }
    g_hash_table_insert (priv->track_objects_table, g_strdup (id), new_table);
  }

  return TRUE;
}

static gboolean
calculate_transitions (GESTimelineLayer * layer)
{
  GESTimelineStandardTransition *tr = NULL;

  GList *tl_objects = NULL, *tck_objects = NULL;
  GList *tmp = NULL, *tmp_tck = NULL;
  gint64 prev_video_end = 0, prev_audio_end = 0;
  gboolean v_avail = FALSE, a_avail = FALSE;
  gint prio, offset = 1;
  tl_objects = ges_timeline_layer_get_objects (layer);

  for (tmp = tl_objects; tmp; tmp = tmp->next) {
    gint64 duration, start, in_point;
    if GES_IS_TIMELINE_TEXT_OVERLAY
      (tmp->data) {
      continue;
      }
    tck_objects = ges_timeline_object_get_track_objects (tmp->data);
    for (tmp_tck = tck_objects; tmp_tck; tmp_tck = tmp_tck->next) {
      if (!ges_track_object_is_active (tmp_tck->data)) {
        continue;
      }

      if GES_IS_TRACK_PARSE_LAUNCH_EFFECT
        (tmp_tck->data) {
        continue;
        }
      if (ges_track_object_get_track (tmp_tck->data)->type ==
          GES_TRACK_TYPE_VIDEO) {
        g_object_get (tmp_tck->data, "duration", &duration, NULL);
        g_object_get (tmp_tck->data, "start", &start, NULL);
        g_object_get (tmp_tck->data, "in_point", &in_point, NULL);
        printf ("video : %lld, %lld, %lld, %lld\n", start, duration, in_point,
            prev_video_end);
        if (start < prev_video_end && (!v_avail)) {
          if (a_avail) {
            ges_timeline_standard_transition_set_video_only (tr, TRUE);
            ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
            printf ("added\n");
          }
          tr = ges_timeline_standard_transition_new_for_nick ((char *)
              "crossfade");
          g_object_set (tr, "start", (gint64) start, "duration",
              (gint64) prev_video_end - start, "in_point", (gint64) 0, NULL);
          printf ("transition : %lld, %lld\n", start, prev_video_end - start);
          a_avail = TRUE;
        } else if (start < prev_video_end) {
          ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
          printf ("added\n");
          printf ("modifying transition videovisually : %lld, %lld\n", start,
              prev_audio_end - start);
          modify_transition (tr, start, prev_video_end - start, 0);
          v_avail = FALSE;
          a_avail = FALSE;
        }

        prev_video_end = duration + start;

      } else {
        g_object_get (tmp_tck->data, "duration", &duration, NULL);
        g_object_get (tmp_tck->data, "start", &start, NULL);
        g_object_get (tmp_tck->data, "in_point", &in_point, NULL);
        printf ("audio : %lld, %lld, %lld\n", start, duration, in_point);
        if (start < prev_audio_end && (!a_avail)) {
          if (v_avail) {
            ges_timeline_standard_transition_set_audio_only (tr, TRUE);
            ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
            printf ("added\n");
          }
          printf ("audio transition : %lld, %lld\n", start,
              prev_audio_end - start);
          tr = ges_timeline_standard_transition_new_for_nick ((char *)
              "crossfade");
          g_object_set (tr, "start", (gint64) start, "duration",
              (gint64) prev_audio_end - start, "in_point", (gint64) 0, NULL);
          v_avail = TRUE;
        } else if (start < prev_audio_end) {
          printf ("added\n");
          ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
          printf ("modifying transition audiophonically : %lld, %lld\n", start,
              prev_audio_end - start);
          modify_transition (tr, start, prev_audio_end - start, 1);
          a_avail = FALSE;
          v_avail = FALSE;
        }
        prev_audio_end = duration + start;
      }
    }
  }

  if (tr != NULL) {
    printf ("added\n");
    if (v_avail) {
      ges_timeline_standard_transition_set_audio_only (tr, TRUE);
    } else if (a_avail) {
      printf ("removed\n");
      ges_timeline_standard_transition_set_video_only (tr, TRUE);
    }
    ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (tr));
  }

  offset = 1;
  tl_objects = ges_timeline_layer_get_objects (layer);

  for (tmp = tl_objects; tmp; tmp = tmp->next) {
    g_object_get (tmp->data, "priority", &prio, NULL);
    if (GES_IS_TIMELINE_TEXT_OVERLAY (tmp->data)) {
      continue;
    }
    if (GES_IS_TIMELINE_STANDARD_TRANSITION (tmp->data)) {
      offset = offset + 1;
      continue;
    }
    g_object_set (tmp->data, "priority", offset, NULL);
    tck_objects = ges_timeline_object_get_track_objects (tmp->data);
  }

  g_list_free (tl_objects);
  return TRUE;
}

void
modify_transition (GESTimelineStandardTransition * tr, gint64 start,
    gint64 duration, gint type)
{
  GList *trackobjects, *tmp;

  trackobjects =
      ges_timeline_object_get_track_objects (GES_TIMELINE_OBJECT (tr));

  for (tmp = trackobjects; tmp; tmp = tmp->next) {

    if (GES_IS_TRACK_AUDIO_TRANSITION (tmp->data) && type) {
      GESTrackAudioTransition *obj;
      obj = (GESTrackAudioTransition *) tmp->data;
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), FALSE);
      g_object_set (obj, "start", (gint64) start, "duration",
          (gint64) duration, "in-point", (gint64) 0, NULL);
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), TRUE);

    } else if (GES_IS_TRACK_VIDEO_TRANSITION (tmp->data)) {
      GESTrackVideoTransition *obj;
      obj = (GESTrackVideoTransition *) tmp->data;
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), FALSE);
      g_object_set (obj, "start", (gint64) start, "duration",
          (gint64) duration, "in-point", (gint64) 0, NULL);
      ges_track_object_set_locked (GES_TRACK_OBJECT (obj), TRUE);
    }
  }
}

static gboolean
parse_timeline_objects (GESPitiviFormatterPrivate * priv)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  gchar *id, *ref;

  GList *ref_list = NULL, *tmp_list = NULL, *tmp = NULL;
  xmlNode *cur_node = NULL;
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/timeline-objects/timeline-object/factory-ref",
      priv->xpathCtx);

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
      ref_list = g_list_append (ref_list, g_strdup (ref));
      xmlFree (ref);
    }
    tmp_list = g_hash_table_lookup (priv->timeline_objects_table, id);
    if (tmp_list != NULL) {
      for (tmp = tmp_list; tmp; tmp = tmp->next) {
        ref_list = g_list_append (ref_list, tmp->data);
      }
    }
    g_hash_table_insert (priv->timeline_objects_table, g_strdup (id),
        g_list_copy (ref_list));
    xmlFree (id);
  }

  g_list_free (ref_list);
  g_list_free (tmp_list);
  g_list_free (tmp);
  xmlXPathFreeObject (xpathObj);
  return TRUE;
}

static void
create_new_source_table (gchar * key, gchar * value, GHashTable * table)
{
  g_hash_table_insert (table, g_strdup (key), g_strdup (value));
}

static gboolean
create_tracks (GESPitiviFormatterPrivate * priv)
{
  priv->tracka = ges_track_audio_raw_new ();
  priv->trackv = ges_track_video_raw_new ();

  if (!ges_timeline_add_track (priv->timeline, priv->tracka)) {
    return FALSE;
  }

  if (!ges_timeline_add_track (priv->timeline, priv->trackv)) {
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
  gchar *media_type = NULL;
  GList *tck_objs = NULL, *tmp = NULL;
  gint64 dur_a, dur_v;
  GList *layers = NULL, *objects = NULL;
  GESPitiviFormatterPrivate *priv;
  printf ("received\n");
  tck_objs = ges_timeline_object_get_track_objects (object);
  media_type =
      (gchar *) g_hash_table_lookup (props_table, (gchar *) "media_type");
  priv = g_hash_table_lookup (props_table, (gchar *) "private");
  priv->not_done--;

  if (g_hash_table_lookup (props_table, "remove")) {
    printf ("->remove\n");
    goto remove;
  }

  if (g_hash_table_lookup (props_table, "transitions")) {
    printf ("->transitions\n");
    goto remove;
  }

  printf ("blop\n");
  for (tmp = tck_objs; tmp; tmp = tmp->next) {
    if (GES_IS_TRACK_PARSE_LAUNCH_EFFECT (tmp->data)) {
      continue;
    }
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

remove:
  for (tmp = tck_objs; tmp; tmp = tmp->next) {
    if GES_IS_TRACK_PARSE_LAUNCH_EFFECT
      (tmp->data) {
      continue;
      }
    if ((ges_track_object_get_track (tmp->data)->type == GES_TRACK_TYPE_AUDIO &&
            (!g_strcmp0 (media_type, (gchar *) "pitivi.stream.VideoStream"))) ||
        (ges_track_object_get_track (tmp->data)->type == GES_TRACK_TYPE_VIDEO &&
            (!g_strcmp0 (media_type, (gchar *) "pitivi.stream.AudioStream")))) {
      if (g_hash_table_lookup (props_table, "remove"))
        ges_track_object_set_active (tmp->data, FALSE);
    }
  }

  if (!priv->not_done && priv->parsed) {
    layers = ges_timeline_get_layers (priv->timeline);
    for (tmp = layers; tmp; tmp = tmp->next) {
      printf ("un layer %d\n", ges_timeline_layer_get_priority (tmp->data));
      if (ges_timeline_layer_get_priority (tmp->data) != 99) {
        calculate_transitions (tmp->data);
      } else {
        objects = ges_timeline_layer_get_objects (tmp->data);
        g_object_get (priv->trackv, "duration", &dur_v, NULL);
        g_object_get (priv->tracka, "duration", &dur_a, NULL);
        printf
            (".....................................dur _v : %lld, dur_a : %lld\n",
            dur_v, dur_a);
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
