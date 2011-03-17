#include "ges-types.h"
#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include "ges.h"
#include "ges-internal.h"
#include "stdint.h"

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_file_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri);

xmlDocPtr create_doc (gchar * uri);
xmlNodePtr get_root_node (xmlDocPtr doc);
gboolean add_to_timeline (GESTrack * track, GESTimeline * timeline);
void create_source (xmlNodePtr node, GESTimeline * timeline,
    GESTimelineLayer * layer);
void add_track_objects (GESTimelineFileSource * src, xmlNodePtr node,
    xmlChar * ref);
void set_properties (GESTimelineObject * src, xmlNodePtr cur_node,
    gchar * prop_name);
GHashTable *print_xpath_nodes (xmlNodePtr nodes);
gboolean create_tracks (xmlXPathContextPtr xpathCtx, GESTimeline * timeline);
GESTrack *create_track (GHashTable * table);
void parse_timeline_objects (xmlXPathContextPtr xpathCtx);

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
    GESTimeline * timeline, gchar * uri)
{
  return TRUE;
}

xmlDocPtr
create_doc (gchar * uri)
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

xmlNodePtr
get_root_node (xmlDocPtr doc)
{
  xmlNodePtr cur;
  cur = xmlDocGetRootElement (doc);

  if (cur == NULL) {
    fprintf (stderr, "empty document\n");
    xmlFreeDoc (doc);
    return NULL;
  }

  if (xmlStrcmp (cur->name, (const xmlChar *) "pitivi")) {
    fprintf (stderr, "document of the wrong type, root node != pitivi");
    xmlFreeDoc (doc);
    return NULL;
  }
  return cur;
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
    printf ("bien bien %s\n", type_field);
    g_value_init (&v, GES_TYPE_TRACK_TYPE);
    gst_value_deserialize (&v, type_field);
    track = ges_track_new (g_value_get_flags (&v), caps);
  } else if (!g_strcmp0 (type, (gchar *) "pitivi.stream.VideoStream")) {
    gchar type_field[] = "GES_TRACK_TYPE_VIDEO";
    printf ("good good %s\n", type_field);
    g_value_init (&v, GES_TYPE_TRACK_TYPE);
    gst_value_deserialize (&v, type_field);
    track = ges_track_new (g_value_get_flags (&v), caps);
  }
  return track;
}

void
create_source (xmlNodePtr node, GESTimeline * timeline,
    GESTimelineLayer * layer)
{
  xmlAttrPtr attr;
  gchar *converted;
  GESTimelineFileSource *src;
  xmlChar *ref;
  for (attr = node->properties; NULL != attr; attr = attr->next) {
    if ((!xmlStrcmp (attr->name, (const xmlChar *) "filename"))) {
      converted = (gchar *) (xmlGetProp (node, attr->name));
      ref = xmlGetProp (node, (const xmlChar *) "id");
      printf ("%s id\n", ref);
      src = ges_timeline_filesource_new (converted);
      layer_sources_list[i] = src;
      i++;
      ges_timeline_layer_add_object (layer, GES_TIMELINE_OBJECT (src));
      ges_timeline_filesource_set_mute (src, TRUE);
      ges_timeline_filesource_set_blind (src, TRUE);
      printf ("blinded and muted");
      add_track_objects (src, node->parent->parent, ref);
      printf ("%s\n", attr->name);
    }
  }
}

void
add_track_objects (GESTimelineFileSource * src, xmlNodePtr node, xmlChar * ref)
{
  xmlNode *cur_node = NULL;
  for (cur_node = node; cur_node; cur_node = cur_node->next) {
    if (!xmlStrcmp (cur_node->name, (const xmlChar *) "factory-ref")) {
      printf ("node type: Element, name: %s\n", cur_node->name);
      printf ("%s its this\n", xmlGetProp (cur_node, (const xmlChar *) "id"));
      if (!xmlStrcmp (xmlGetProp (cur_node->parent->parent->prev->prev,
                  (const xmlChar *) "type"),
              (const xmlChar *) "pitivi.stream.VideoStream")) {
        if (!xmlStrcmp (xmlGetProp (cur_node, (const xmlChar *) "id"), ref)) {
          printf ("%s de merde", xmlGetProp (cur_node, (const xmlChar *) "id"));
          ges_timeline_filesource_set_blind (src, FALSE);
          printf ("unblinded");
          set_properties (GES_TIMELINE_OBJECT (src), cur_node,
              (gchar *) "in_point");
          set_properties (GES_TIMELINE_OBJECT (src), cur_node,
              (gchar *) "start");
          set_properties (GES_TIMELINE_OBJECT (src), cur_node,
              (gchar *) "duration");
          set_properties (GES_TIMELINE_OBJECT (src), cur_node,
              (gchar *) "priority");
        }
      } else if (!xmlStrcmp (xmlGetProp (cur_node->parent->parent->prev->prev,
                  (const xmlChar *) "type"),
              (const xmlChar *) "pitivi.stream.AudioStream")) {
        if (!xmlStrcmp (xmlGetProp (cur_node, (const xmlChar *) "id"), ref)) {
          printf ("unmuted");
          ges_timeline_filesource_set_mute (src, FALSE);
        }
      }
    }
    add_track_objects (src, cur_node->children, ref);
  }
}

void
set_properties (GESTimelineObject * src, xmlNodePtr cur_node, gchar * prop_name)
{
  xmlChar *property;
  gint64 converted;
  int substracted;
  property = xmlGetProp (cur_node->parent, (const xmlChar *) prop_name);
  if (prop_name == (gchar *) "priority") {
    substracted = 5;
  } else {
    substracted = 8;
  }
  property = xmlStrsub (property, substracted, 50);
  printf ("%s %s    pute\n", property, prop_name);
  converted = g_ascii_strtoll ((gchar *) property, NULL, 0);
  g_object_set (src, prop_name, converted, NULL);
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

void
parse_timeline_objects (xmlXPathContextPtr xpathCtx)
{
  xmlXPathObjectPtr xpathObj;
  GHashTable *table;
  int size;
  int j;
  gint id;
  xmlNodeSetPtr nodes;
  xpathObj = xmlXPathEvalExpression ((const xmlChar *)
      "/pitivi/timeline/timeline-objects/timeline-object/factory-ref",
      xpathCtx);
  nodes = xpathObj->nodesetval;
  size = (nodes) ? nodes->nodeNr : 0;
  for (j = 0; j < size; ++j) {
    table = print_xpath_nodes (nodes->nodeTab[j]);
    id = (gint) g_hash_table_lookup (table, (gchar *) "id");
    printf ("id : %d\n", id);
    g_hash_table_destroy (table);
  }
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
    printf ("%s \n", cur_attr->name);
    name = (gchar *) cur_attr->name;
    value = (gchar *) xmlGetProp (node, cur_attr->name);
    printf ("real value = %s\n", value);
    g_hash_table_insert (props_table, g_strdup (name), g_strdup (value));
  }
  return props_table;
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri)
{
  xmlDocPtr doc;
  //xmlNodePtr root_node;
  GESTimelineLayer *layer;
  GESTimelinePipeline *pipeline;
  gboolean ret = TRUE;
  xmlXPathContextPtr xpathCtx;
  //GMainLoop *mainloop;

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
  parse_timeline_objects (xpathCtx);
  //root_node = get_root_node (doc);
  //parse_tracks (root_node, timeline);
  //parse_sources (root_node, timeline, layer);
  //printf ("\ncomment ???\n\n\n");
  //ges_timeline_pipeline_set_mode (pipeline, TIMELINE_MODE_PREVIEW_VIDEO);
  //gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
  //mainloop = g_main_loop_new (NULL, FALSE);
  //g_main_loop_run (mainloop);
  printf ("biatch\n%s\n", uri);
freedoc:
  xmlFreeDoc (doc);
  return ret;
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
