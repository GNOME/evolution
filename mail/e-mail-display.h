/*
 * e-mail-display.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_DISPLAY_H
#define E_MAIL_DISPLAY_H

#include <e-util/e-util.h>

#include <em-format/e-mail-formatter.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_DISPLAY \
	(e_mail_display_get_type ())
#define E_MAIL_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplay))
#define E_MAIL_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_DISPLAY, EMailDisplayClass))
#define E_IS_MAIL_DISPLAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_DISPLAY))
#define E_IS_MAIL_DISPLAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_DISPLAY))
#define E_MAIL_DISPLAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_DISPLAY, EMailDisplayClass))

G_BEGIN_DECLS

typedef struct _EMailDisplay EMailDisplay;
typedef struct _EMailDisplayClass EMailDisplayClass;
typedef struct _EMailDisplayPrivate EMailDisplayPrivate;

struct _EMailDisplay {
	EWebView web_view;
	EMailDisplayPrivate *priv;
};

struct _EMailDisplayClass {
	EWebViewClass parent_class;

};

GType		e_mail_display_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_mail_display_new		(void);
EMailFormatterMode
		e_mail_display_get_mode		(EMailDisplay *display);
void		e_mail_display_set_mode		(EMailDisplay *display,
						 EMailFormatterMode mode);
EMailFormatter *
		e_mail_display_get_formatter	(EMailDisplay *display);
EMailPartList *	e_mail_display_get_part_list	(EMailDisplay *display);
void		e_mail_display_set_part_list	(EMailDisplay *display,
						 EMailPartList *part_list);
gboolean	e_mail_display_get_headers_collapsable
						(EMailDisplay *display);
void		e_mail_display_set_headers_collapsable
						(EMailDisplay *display,
						 gboolean collapsable);
gboolean	e_mail_display_get_headers_collapsed
						(EMailDisplay *display);
void		e_mail_display_set_headers_collapsed
						(EMailDisplay *display,
						 gboolean collapsed);
void		e_mail_display_load		(EMailDisplay *display,
						 const gchar *msg_uri);
void		e_mail_display_reload		(EMailDisplay *display);
GtkAction *	e_mail_display_get_action	(EMailDisplay *display,
						 const gchar *action_name);
void		e_mail_display_set_status	(EMailDisplay *display,
						 const gchar *status);
const gchar *	e_mail_display_get_selection_plain_text_sync
						(EMailDisplay *display,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_display_load_images	(EMailDisplay *display);
void		e_mail_display_set_force_load_images
						(EMailDisplay *display,
						 gboolean force_load_images);

G_END_DECLS

#endif /* E_MAIL_DISPLAY_H */
