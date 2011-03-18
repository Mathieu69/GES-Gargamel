#include "ges-types.h"
#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include "ges.h"
#include "ges-internal.h"
#include "stdint.h"

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_file_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri);

xmlDocPtr create_doc (const gchar * uri);
gboolean add_to_timeline (GESTrack * track, GESTimeline * timeline);

void
set_property (GESTimelineFileSource * src, gchar * prop_name,
    gchar * prop_value);
GHashTable *print_xpath_nodes (xmlNodePtr nodes);
gboolean create_tracks (xmlXPathContextPtr xpathCtx, GESTimeline * timeline);
GESTrack *create_track (GHashTable * table);
GHashTable *list_sources (xmlXPathContextPtr xpathCtx,
    GESTimelineLayer * layer);
void parse_track_objects (xmlXPathContextPtr xpathCtx,
    GHashTable * sources_table, GESTimelineLayer * layer);
GHashTable *parse_timeline_objects (GHashTable * sources_table,
    xmlXPathContextPtr xpathCtx);

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

  formatter_klass->save_to_uri = save_pitivi_file_to_uri;
  formatter_klass->load_from_uri = load_pitivi_file_from_uri;
}

static void
ges_pitivi_formatter_init (GESPitiviFormatter * object)
{
}

static gboolean
save_pitivi_file_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, const gchar * uri)
{
  return TRUE;
}

xmlDocPtr
create_doc (const gchar * uri)
{
  xmlDocPtr doc;
  doc = xmlParseFile (uri);

  if (doc == NULL) {
    printf ("Document not parsed successfully. \n");
  } else {
    printf ("Document parsed successfully.\n");
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
parse_track_objects (xmlXPathContextPtr xpathCtx, GHashTable * sources_table,
    GESTimelineLayer * layer)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  GHashTable *table, *source_table;
  gchar *id;
  xmlChar *type;
  GESTimelineFileSource *src;

  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/tracks/track/track-objects/track-object", xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    table = print_xpath_nodes (nodes->nodeTab[j]);
    id = (gchar *) g_hash_table_lookup (table, (gchar *) "id");
    source_table = g_hash_table_lookup (sources_table, id);
    src = ges_timeline_filesource_new ((gchar *)
        g_hash_table_lookup (source_table, (gchar *) "filename"));
    type =
        xmlGetProp (nodes->nodeTab[j]->parent->prev->prev, (xmlChar *) "type");
    if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.VideoStream")) {
      ges_timeline_filesource_set_mute (src, TRUE);
    } else {
      ges_timeline_filesource_set_blind (src, TRUE);
    }
    set_property (src, (gchar *) "duration",
        (gchar *) g_hash_table_lookup (table, (gchar *) "duration"));
    set_property (src, (gchar *) "start", (gchar *) g_hash_table_lookup (table,
            (gchar *) "start"));
    set_property (src, (gchar *) "in_point",
        (gchar *) g_hash_table_lookup (table, (gchar *) "in_point"));
    set_property (src, (gchar *) "priority",
        (gchar *) g_hash_table_lookup (table, (gchar *) "priority"));
    ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));
  }
}

void
set_property (GESTimelineFileSource * src, gchar * prop_name,
    gchar * prop_value)
{
  gint64 converted;
  prop_value = g_strsplit (prop_value, (gchar *) ")", (gint) 0)[1];
  converted = g_ascii_strtoll ((gchar *) prop_value, NULL, 0);
  g_object_set (src, prop_name, converted, NULL);
}

GHashTable *
parse_timeline_objects (GHashTable * sources_table, xmlXPathContextPtr xpathCtx)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  GHashTable *track_objects_table, *source_table;
  int size, j;
  xmlNode *cur_node = NULL;
  gchar *id, *ref;

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
    for (cur_node = cur_node; cur_node; cur_node = cur_node->next->next) {
      ref = (gchar *) xmlGetProp (cur_node, (xmlChar *) "id");
      source_table = g_hash_table_lookup (sources_table, id);
      g_hash_table_insert (track_objects_table, g_strdup (ref), source_table);
    }
  }
  return track_objects_table;
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
  GESTimelinePipeline *pipeline;
  gboolean ret = TRUE;
  xmlXPathContextPtr xpathCtx;
  GMainLoop *mainloop;
  GHashTable *source_table;

  pipeline = ges_timeline_pipeline_new ();
  ges_timeline_pipeline_add_timeline (pipeline, timeline);
  layer = GES_TIMELINE_LAYER (ges_timeline_layer_new ());
  ges_timeline_add_layer (timeline, layer);
  doc = create_doc (uri);
  xpathCtx = xmlXPathNewContext (doc);
  if (!create_tracks (xpathCtx, timeline)) {
    ret = FALSE;
    goto freedoc;
  }
  source_table = list_sources (xpathCtx, layer);
  source_table = parse_timeline_objects (source_table, xpathCtx);
  parse_track_objects (xpathCtx, source_table, layer);

  ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);
  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);
freedoc:
  xmlFreeDoc (doc);
  return ret;
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
