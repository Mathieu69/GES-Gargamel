#include "ges-types.h"
#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include "ges.h"
#include "ges-internal.h"

G_DEFINE_TYPE (GESPitiviFormatter, ges_pitivi_formatter, GES_TYPE_FORMATTER);

static gboolean save_pitivi_file (GESFormatter * pitivi_formatter,
    GESTimeline * timeline);
static gboolean load_pitivi_file (GESFormatter * pitivi_formatter,
    GESTimeline * timeline);

static void
ges_pitivi_formatter_class_init (GESPitiviFormatterClass * klass)
{
  GESFormatterClass *formatter_klass;

  formatter_klass = GES_FORMATTER_CLASS (klass);

  formatter_klass->save = save_pitivi_file;
  formatter_klass->load = load_pitivi_file;
}

static void
ges_pitivi_formatter_init (GESPitiviFormatter * object)
{
}

static gboolean
save_pitivi_file (GESFormatter * pitivi_formatter, GESTimeline * timeline)
{
  return 1;
}

static gboolean
load_pitivi_file (GESFormatter * pitivi_formatter, GESTimeline * timeline)
{
  return 1;
}
