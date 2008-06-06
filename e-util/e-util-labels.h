/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  e-util-labels.h
 *
 *  Copyright (C) 2007 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef _E_UTIL_LABELS_H
#define _E_UTIL_LABELS_H

#include <gtk/gtk.h>

struct _GtkWindow;
struct _GConfClient;

typedef struct {
	char *tag;
	char *name;
	char *colour;
} EUtilLabel;

#define E_UTIL_LABELS_GCONF_KEY "/apps/evolution/mail/labels"

#define LABEL_DEFAULTS_NUM 5
extern EUtilLabel label_defaults[LABEL_DEFAULTS_NUM];

GSList *    e_util_labels_parse         (struct _GConfClient *client);
void        e_util_labels_free          (GSList *labels);

char *      e_util_labels_add           (const char *name, const GdkColor *color);
char *      e_util_labels_add_with_dlg  (struct _GtkWindow *parent, const char *tag);
gboolean    e_util_labels_remove        (const char *tag);
gboolean    e_util_labels_set_data      (const char *tag, const char *name, const GdkColor *color);

gboolean    e_util_labels_is_system     (const char *tag);
const char *e_util_labels_get_new_tag   (const char *old_tag);

const char *e_util_labels_get_name      (GSList *labels, const char *tag);
gboolean    e_util_labels_get_color     (GSList *labels, const char *tag, GdkColor *color);
const char *e_util_labels_get_color_str (GSList *labels, const char *tag);

#endif /* _E_UTIL_LABELS_H */
