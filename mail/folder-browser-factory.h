/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#ifndef _FOLDER_BROWSER_FACTORY_H
#define _FOLDER_BROWSER_FACTORY_H

#include <bonobo.h>

BonoboControl *folder_browser_factory_new_control              (const char *uri);
GList         *folder_browser_factory_get_active_control_list  (void);

#endif /* _FOLDER_BROWSER_FACTORY_H */
