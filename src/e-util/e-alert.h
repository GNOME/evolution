/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ALERT_H
#define E_ALERT_H

#include <stdarg.h>
#include <gtk/gtk.h>

#include <e-util/e-ui-action.h>

/* Standard GObject macros */
#define E_TYPE_ALERT \
	(e_alert_get_type ())
#define E_ALERT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALERT, EAlert))
#define E_ALERT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALERT, EAlertClass))
#define E_IS_ALERT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALERT))
#define E_IS_ALERT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALERT))
#define E_ALERT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALERT, EAlertClass))

/* takes filename, returns OK if yes */
#define E_ALERT_ASK_FILE_EXISTS_OVERWRITE \
	"system:ask-save-file-exists-overwrite"
/* takes filename, reason */
#define E_ALERT_NO_SAVE_FILE "system:no-save-file"
/* takes filename, reason */
#define E_ALERT_NO_LOAD_FILE "system:no-save-file"

G_BEGIN_DECLS

struct _EAlertSink;

typedef struct _EAlert EAlert;
typedef struct _EAlertClass EAlertClass;
typedef struct _EAlertPrivate EAlertPrivate;

struct _EAlert {
	GObject parent;
	EAlertPrivate *priv;
};

struct _EAlertClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*response)		(EAlert *alert,
						 gint response_id);
};

GType		e_alert_get_type		(void) G_GNUC_CONST;
EAlert *	e_alert_new			(const gchar *tag,
						 ...) G_GNUC_NULL_TERMINATED;
EAlert *	e_alert_new_valist		(const gchar *tag,
						 va_list va);
EAlert *	e_alert_new_array		(const gchar *tag,
						 GPtrArray *args);
const gchar *	e_alert_get_tag			(EAlert *alert);
gint		e_alert_get_default_response	(EAlert *alert);
void		e_alert_set_default_response	(EAlert *alert,
						 gint response_id);
GtkMessageType	e_alert_get_message_type	(EAlert *alert);
void		e_alert_set_message_type	(EAlert *alert,
						 GtkMessageType message_type);
const gchar *	e_alert_get_primary_text	(EAlert *alert);
void		e_alert_set_primary_text	(EAlert *alert,
						 const gchar *primary_text);
const gchar *	e_alert_get_secondary_text	(EAlert *alert);
void		e_alert_set_secondary_text	(EAlert *alert,
						 const gchar *secondary_text);
const gchar *	e_alert_get_icon_name		(EAlert *alert);
void		e_alert_add_action		(EAlert *alert,
						 EUIAction *action,
						 gint response_id,
						 gboolean is_destructive);
GList *		e_alert_peek_actions		(EAlert *alert); /* EUIAction * */
void		e_alert_add_widget		(EAlert *alert,
						 GtkWidget *widget);
GList *		e_alert_peek_widgets		(EAlert *alert);
GtkWidget *	e_alert_create_image		(EAlert *alert,
						 GtkIconSize size);
void		e_alert_response		(EAlert *alert,
						 gint response_id);
void		e_alert_start_timer		(EAlert *alert,
						 guint seconds);

void		e_alert_submit			(struct _EAlertSink *alert_sink,
						 const gchar *tag,
						 ...) G_GNUC_NULL_TERMINATED;
void		e_alert_submit_valist		(struct _EAlertSink *alert_sink,
						 const gchar *tag,
						 va_list va);
GtkWidget *	e_alert_create_button_for_action(EUIAction *for_action);

G_END_DECLS

#endif /* E_ALERT_H */
