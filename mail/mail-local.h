#ifndef _MAIL_LOCAL_H
#define _MAIL_LOCAL_H

#include "camel/camel-folder.h"
#include "folder-browser.h"

/* mail-local.c */
CamelFolder *local_uri_to_folder(const char *uri, CamelException *ex);
void local_reconfigure_folder(FolderBrowser *fb);

#endif
