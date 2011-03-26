#include "ges-types.h"
#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include "ges.h"
#include "ges-internal.h"
#include "stdint.h"
#include "ges-timeline-file-source.h"

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_file_to_uri (GESFormatter * pitivi_formatter,
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
gboolean parse_track_objects (GList * list);
GHashTable *parse_timeline_objects (GHashTable * sources_table,
    xmlXPathContextPtr xpathCtx);
void set_source_properties (GObject * src, GHashTable * table,
    GESTimelineLayer * layer);
void make_transition (GESTimelineLayer * layer, gint64 start, gint64 prev_end,
    gchar * type);

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

gboolean
parse_track_objects (GList * list)
{
  xmlXPathObjectPtr xpathObj;
  xmlNodeSetPtr nodes;
  int size, j;
  GHashTable *table, *source_table;
  gchar *id;
  xmlChar *type;
  gchar *mode, *filename;
  gint64 start, duration, inpoint, end;
  gint64 prev_end = 0, prev_video_end = 0, prev_audio_end = 0;
  xmlXPathContextPtr xpathCtx = g_list_first (list)->data;
  GESTimelineLayer *layer = g_list_next (list)->data;
  GHashTable *sources_table = g_list_last (list)->data;
  GESTimelineTestSource *testsrc = NULL;
  GESTimelineFileSource *src = NULL;

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
        g_ascii_strtoll (g_strsplit ((gchar *) g_hash_table_lookup (table,
                (gchar *) "start"), ")", (gint) 0)[1], NULL, 0);
    duration =
        g_ascii_strtoll (g_strsplit ((gchar *) g_hash_table_lookup (table,
                (gchar *) "duration"), ")", (gint) 0)[1], NULL, 0);
    inpoint =
        g_ascii_strtoll (g_strsplit ((gchar *) g_hash_table_lookup (table,
                (gchar *) "in_point"), ")", (gint) 0)[1], NULL, 0);
    filename =
        (gchar *) g_hash_table_lookup (source_table, (gchar *) "filename");
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
      }
      end = duration + start;
      if (start < prev_end) {
        make_transition (layer, start, prev_end, (gchar *) "standard");
      } else if (start < prev_video_end) {
        make_transition (layer, start, prev_video_end, (gchar *) "video");
      } else if (start < prev_audio_end) {
        make_transition (layer, start, prev_audio_end, (gchar *) "audio");
      }
      prev_end = end;
    } else if (!xmlStrcmp (type, (xmlChar *) "pitivi.stream.VideoStream")) {
      src = ges_timeline_filesource_new (filename);
      ges_timeline_filesource_set_video_only (src, TRUE);
      end = duration + start;
      if (start < prev_video_end) {
        make_transition (layer, start, prev_video_end, (gchar *) "video");
      } else if (start < prev_end) {
        make_transition (layer, start, prev_end, (gchar *) "video");
      }
      prev_video_end = end;
    } else {
      src = ges_timeline_filesource_new (filename);
      ges_timeline_filesource_set_audio_only (src, TRUE);
      end = duration + start;
      if (start < prev_audio_end) {
        make_transition (layer, start, prev_audio_end, (gchar *) "audio");
      } else if (start < prev_end) {
        make_transition (layer, start, prev_end, (gchar *) "audio");
      }
      prev_audio_end = end;
    }
    if (!g_strcmp0 (filename, (gchar *) "test")
        || !g_strcmp0 (filename, (gchar *) "test2")) {
      set_source_properties (G_OBJECT (testsrc), table, layer);
    } else {
      set_source_properties (G_OBJECT (src), table, layer);
    }
  }
  return FALSE;
}

void
make_transition (GESTimelineLayer * layer, gint64 start, gint64 prev_end,
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
      (gint64) prev_end - start, "in-point", (gint64) 0, NULL);
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
  printf ("prop : %s had been set to : %lld\n", prop_name, converted);
  g_object_set (src, prop_name, converted, NULL);
  g_object_get (src, prop_name, &real, NULL);
  printf ("the real value is : %lld\n", real);
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
      if (simple && !cur_node->next->next) {
        res = (gchar *) "double";
      } else {
        res = (gchar *) "simple";
      }
      simple = FALSE;
      g_hash_table_insert (source_table, g_strdup ("mode"), g_strdup (res));
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
  gboolean ret = TRUE;
  xmlXPathContextPtr xpathCtx;
  GHashTable *source_table;
  GList *list = NULL;

  layer = ges_timeline_layer_new ();
  g_object_set (layer, "priority", (gint32) 0, NULL);
  ges_timeline_add_layer (timeline, layer);
  doc = create_doc (uri);
  xpathCtx = xmlXPathNewContext (doc);
  if (!create_tracks (xpathCtx, timeline)) {
    ret = FALSE;
    goto freedoc;
  }

  source_table = list_sources (xpathCtx, layer);
  source_table = parse_timeline_objects (source_table, xpathCtx);
  list = g_list_append (list, xpathCtx);
  list = g_list_append (list, layer);
  list = g_list_append (list, source_table);
  parse_track_objects (list);
freedoc:
  xmlFreeDoc (doc);
  return ret;
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
