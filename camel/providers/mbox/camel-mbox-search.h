
#ifndef _CAMEL_MBOX_SEARCH_H
#define _CAMEL_MBOX_SEARCH_H

#include "camel-mbox-folder.h"

GList *camel_mbox_folder_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);

#endif /* ! _CAMEL_MBOX_SEARCH_H */

