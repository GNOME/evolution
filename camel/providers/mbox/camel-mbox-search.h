
#ifndef _CAMEL_MBOX_SEARCH_H
#define _CAMEL_MBOX_SEARCH_H

#include <glib.h>
#include "camel-mbox-folder.h"

int camel_mbox_folder_search_by_expression(CamelFolder *folder, const char *expression,
					   CamelSearchFunc *func, void *data, CamelException *ex);
gboolean camel_mbox_folder_search_complete(CamelFolder *folder, int searchid, gboolean wait, CamelException *ex);
void camel_mbox_folder_search_cancel(CamelFolder *folder, int searchid, CamelException *ex);

#endif /* ! _CAMEL_MBOX_SEARCH_H */

