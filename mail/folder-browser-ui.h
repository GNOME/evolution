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
void folder_browser_ui_add_list (FolderBrowser *fb);
void folder_browser_ui_add_global (FolderBrowser *fb);

void folder_browser_ui_rm_list (FolderBrowser *fb);
void folder_browser_ui_rm_all (FolderBrowser *fb);

/* these affect the sensitivity of UI elements */
void folder_browser_ui_scan_selection (FolderBrowser *fb);
void folder_browser_ui_set_selection_state (FolderBrowser *fb, FolderBrowserSelectionState state);
void folder_browser_ui_message_loaded (FolderBrowser *fb);

void folder_browser_ui_discard_view_menus (FolderBrowser *fb);
void folder_browser_ui_setup_view_menus (FolderBrowser *fb);
/* Set the sensitivity of a single item */
void fbui_sensitise_item(FolderBrowser *fb, const char *item, int state);

void folder_browser_ui_message_list_focus (FolderBrowser *fb);
void folder_browser_ui_message_list_unfocus (FolderBrowser *fb);

#endif /* _FOLDER_BROWSER_UI_H */
