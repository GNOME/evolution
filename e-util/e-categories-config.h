/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Categories configuration.
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#ifndef __E_CATEGORIES_CONFIG_H__
#define __E_CATEGORIES_CONFIG_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkentry.h>

G_BEGIN_DECLS

const char *e_categories_config_get_color_for (const char *category);
void e_categories_config_set_color_for (const char *category, const char *color);

gboolean e_categories_config_get_icon_for (const char *category,
					   GdkPixmap **icon,
					   GdkBitmap **mask);
const char *e_categories_config_get_icon_file_for (const char *category);
void e_categories_config_set_icon_for (const char *category,
				       const char *pixmap_file);

void e_categories_config_open_dialog_for_entry (GtkEntry *entry);

G_END_DECLS

#endif
