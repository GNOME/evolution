/*
	Copyright 2000 Helix Code Inc.
*/
#include <glib.h>

#include "ibex.h"

#define IBEX_VERSION "ibex1"

struct ibex {
  char *path;
  GTree *files;
  GHashTable *words;
  GPtrArray *oldfiles;
  gboolean dirty;
};

struct ibex_file {
  char *name;
  long index;
};
typedef struct ibex_file ibex_file;

gint ibex__strcmp(gconstpointer a, gconstpointer b);

#define IBEX_BUFSIZ 1024
