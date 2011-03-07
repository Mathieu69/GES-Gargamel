#include "ges-types.h"
#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include "ges.h"
#include "ges-internal.h"

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_file_to_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri);
static gboolean load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri);

xmlDocPtr create_doc (gchar * uri);
xmlNodePtr get_root_node (xmlDocPtr doc);
static void parse_tracks (xmlNode * a_node, GESTimeline * timeline);
void create_track (xmlNodePtr node, GESTimeline * timeline);
gboolean add_to_timeline (GESTrack * track, GESTimeline * timeline);
static void parse_sources (xmlNode * a_node, GESTimeline * timeline,
    GESTimelineLayer * layer);
void create_source (xmlNodePtr node, GESTimeline * timeline,
    GESTimelineLayer * layer);

struct _GESPitiviFormatterPrivate
{
  gchar *bin_description;
};

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

void
create_track (xmlNodePtr node, GESTimeline * timeline)
{
  const gchar *converted;
  GstCaps *caps = { 0 };
  xmlChar *caps_field;
  xmlChar *type_field;
  xmlAttrPtr attr;
  GESTrack *track;
  GValue v = { 0 };
  for (attr = node->properties; NULL != attr; attr = attr->next) {
    if ((!xmlStrcmp (attr->name, (const xmlChar *) "caps"))) {
      caps_field = xmlGetProp (node, attr->name);
      printf ("%s\n", caps_field);
      converted = (const gchar *) caps_field;
      caps = gst_caps_from_string (converted);
    }
    if ((!xmlStrcmp (attr->name, (const xmlChar *) "type"))) {
      type_field = xmlGetProp (node, attr->name);
      if ((!xmlStrcmp (xmlGetProp (node, attr->name),
                  (const xmlChar *) "pitivi.stream.AudioStream"))) {
        gchar type[] = "GES_TRACK_TYPE_AUDIO";
        printf ("%s \n", type);
        g_value_init (&v, GES_TYPE_TRACK_TYPE);
        gst_value_deserialize (&v, type);
        track = ges_track_new (g_value_get_flags (&v), caps);
        add_to_timeline (track, timeline);
      } else {
        gchar type[] = "GES_TRACK_TYPE_VIDEO";
        printf ("%s \n", type);
        g_value_init (&v, GES_TYPE_TRACK_TYPE);
        gst_value_deserialize (&v, type);
        track = ges_track_new (g_value_get_flags (&v), caps);
        add_to_timeline (track, timeline);
      }
    }
  }
}

void
create_source (xmlNodePtr node, GESTimeline * timeline,
    GESTimelineLayer * layer)
{
  xmlAttrPtr attr;
  gchar *converted;
  GESTimelineObject *src;
  for (attr = node->properties; NULL != attr; attr = attr->next) {
    if ((!xmlStrcmp (attr->name, (const xmlChar *) "filename"))) {
      converted = (gchar *) (xmlGetProp (node, attr->name));
      src = GES_TIMELINE_OBJECT (ges_timeline_filesource_new (converted));
      ges_timeline_layer_add_object (layer, src);
    }
  }
}

gboolean
add_to_timeline (GESTrack * track, GESTimeline * timeline)
{
  if (!ges_timeline_add_track (timeline, track)) {
    g_object_unref (track);
    return FALSE;
  }
  printf ("YES FAG CAPS LOCK !!!!");
  return TRUE;
}

static void
parse_tracks (xmlNode * a_node, GESTimeline * timeline)
{
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if ((!xmlStrcmp (cur_node->name, (const xmlChar *) "stream")) &&
        (!xmlStrcmp (cur_node->parent->name, (const xmlChar *) "track"))) {
      printf ("node type: Element, name: %s\n", cur_node->name);
      create_track (cur_node, timeline);
    }
    parse_tracks (cur_node->children, timeline);
  }
}

static void
parse_sources (xmlNode * a_node, GESTimeline * timeline,
    GESTimelineLayer * layer)
{
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if ((!xmlStrcmp (cur_node->name, (const xmlChar *) "source"))) {
      printf ("node type: Element, name: %s\n", cur_node->name);
      create_source (cur_node, timeline, layer);
    }
    parse_sources (cur_node->children, timeline, layer);
  }
}

static gboolean
load_pitivi_file_from_uri (GESFormatter * pitivi_formatter,
    GESTimeline * timeline, gchar * uri)
{
  xmlDocPtr doc;
  xmlNodePtr root_node;
  GESTimelineLayer *layer;
  layer = GES_TIMELINE_LAYER (ges_timeline_layer_new ());
  ges_timeline_add_layer (timeline, layer);
  doc = create_doc (uri);
  root_node = get_root_node (doc);
  parse_tracks (root_node, timeline);
  parse_sources (root_node, timeline, layer);
  printf ("biatch\n%s\n", uri);
  return TRUE;
}

GESPitiviFormatter *
ges_pitivi_formatter_new (void)
{
  return g_object_new (GES_TYPE_PITIVI_FORMATTER, NULL);
}
