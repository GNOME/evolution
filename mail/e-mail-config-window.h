/*
 * e-mail-config-window.h
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
 */

#ifndef E_MAIL_CONFIG_WINDOW_H
#define E_MAIL_CONFIG_WINDOW_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_WINDOW \
	(e_mail_config_window_get_type ())
#define E_MAIL_CONFIG_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_WINDOW, EMailConfigWindow))
#define E_MAIL_CONFIG_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_WINDOW, EMailConfigWindowClass))
#define E_IS_MAIL_CONFIG_WINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_WINDOW))
#define E_IS_MAIL_CONFIG_WINDOW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_WINDOW))
#define E_MAIL_CONFIG_WINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_WINDOW, EMailConfigWindowClass))

G_BEGIN_DECLS

typedef struct _EMailConfigWindow EMailConfigWindow;
typedef struct _EMailConfigWindowClass EMailConfigWindowClass;
typedef struct _EMailConfigWindowPrivate EMailConfigWindowPrivate;

struct _EMailConfigWindow {
	GtkDialog parent;
	EMailConfigWindowPrivate *priv;
};

struct _EMailConfigWindowClass {
	GtkDialogClass parent_class;

	/* Signals */
	void		(*changes_committed)	(EMailConfigWindow *window);
};

GType		e_mail_config_window_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_window_new	(EMailSession *session,
						 ESource *original_source);
EMailSession *	e_mail_config_window_get_session
						(EMailConfigWindow *window);
ESource *	e_mail_config_window_get_original_source
						(EMailConfigWindow *window);

G_END_DECLS

#endif /* E_MAIL_CONFIG_WINDOW_H */

