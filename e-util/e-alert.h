/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#ifndef _E_ALERT_H
#define _E_ALERT_H

#include <stdarg.h>
#include <gtk/gtk.h>

/*
 * Some standard alerts, if these are altered or added to,
 * update devel-docs/misc/errors.txt
 *
 * Several more basic ones are needed.
 */

#define E_ALERT_INFO "builtin:info"
#define E_ALERT_INFO_PRIMARY "builtin:info-primary"
#define E_ALERT_WARNING "builtin:warning"
#define E_ALERT_WARNING_PRIMARY "builtin:warning-primary"
#define E_ALERT_ERROR "builtin:error"
#define E_ALERT_ERROR_PRIMARY "builtin:error-primary"

/* takes filename, returns OK if yes */
#define E_ALERT_ASK_FILE_EXISTS_OVERWRITE "system:ask-save-file-exists-overwrite"
/* takes filename, reason */
#define E_ALERT_NO_SAVE_FILE "system:no-save-file"
/* takes filename, reason */
#define E_ALERT_NO_LOAD_FILE "system:no-save-file"

G_BEGIN_DECLS

#define E_TYPE_ALERT e_alert_get_type()

#define E_ALERT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  E_TYPE_ALERT, EAlert))

#define E_ALERT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  E_TYPE_ALERT, EAlertClass))

#define E_IS_ALERT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  E_TYPE_ALERT))

#define E_IS_ALERT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  E_TYPE_ALERT))

#define E_ALERT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  E_TYPE_ALERT, EAlertClass))

typedef struct _EAlert EAlert;
typedef struct _EAlertClass EAlertClass;
typedef struct _EAlertPrivate EAlertPrivate;

struct _e_alert_button {
	struct _e_alert_button *next;
	const gchar *stock;
	const gchar *label;
	gint response;
};

struct _EAlert
{
  GObject parent;
  EAlertPrivate *priv;
};

struct _EAlertClass
{
  GObjectClass parent_class;
};

GType e_alert_get_type (void);

EAlert *e_alert_new (const gchar *tag, ...) G_GNUC_NULL_TERMINATED;
EAlert *e_alert_new_valist (const gchar *tag, va_list ap);
EAlert *e_alert_new_array (const gchar *tag, GPtrArray *args);

guint32 e_alert_get_flags (EAlert *alert);
const gchar *e_alert_peek_stock_image (EAlert *alert);
gint e_alert_get_default_response (EAlert *alert);
gchar *e_alert_get_title (EAlert *alert, gboolean escaped);
gchar *e_alert_get_primary_text (EAlert *alert, gboolean escaped);
gchar *e_alert_get_secondary_text (EAlert *alert, gboolean escaped);
const gchar *e_alert_peek_help_uri (EAlert *alert);
gboolean e_alert_get_scroll (EAlert *alert);
struct _e_alert_button *e_alert_peek_buttons (EAlert *alert);

G_END_DECLS

#endif /* _E_ALERT_H */
