/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-factory.c: A Bonobo Control factory for Folder Browsers
 *
 * Author:
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * (C) 2000 Ximian, Inc.
 */

#ifndef _FOLDER_BROWSER_FACTORY_H
#define _FOLDER_BROWSER_FACTORY_H

#include <bonobo/bonobo-control.h>
#include "Evolution.h"
#include "e-util/e-list.h"

BonoboControl *folder_browser_factory_new_control       (const char *uri);
EList         *folder_browser_factory_get_control_list  (void);

struct _EMFolderBrowser *folder_browser_factory_get_browser(const char *uri);

#endif /* _FOLDER_BROWSER_FACTORY_H */
