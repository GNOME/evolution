/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * folder-browser-ui.c: Sets up the Bonobo UI for FolderBrowsers
 *
 * Author:
 *   Peter Williams <peterw@ximian.com>
 *
 * (C) 2001 Ximian, Inc.
 */

#ifndef _FOLDER_BROWSER_UI_H
#define _FOLDER_BROWSER_UI_H

#include "folder-browser.h"

void folder_browser_ui_add_message (FolderBrowser *fb);
void folder_browser_ui_rm_message (FolderBrowser *fb);

void folder_browser_ui_add_list (FolderBrowser *fb);
void folder_browser_ui_rm_list (FolderBrowser *fb);

void folder_browser_ui_add_global (FolderBrowser *fb);
void folder_browser_ui_rm_global (FolderBrowser *fb);

#endif /* _FOLDER_BROWSER_FACTORY_H */
