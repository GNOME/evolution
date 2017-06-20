/*
 * e-mail-config-activity-page.h
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

/* This is a convenient base class for EMailConfigPages that might need
 * to run an asynchronous method and display an error message.  It adds
 * activity and alert bars to the bottom of the page, it implements the
 * EAlertSink interface, and can create new EActivity instances. */

#ifndef E_MAIL_CONFIG_ACTIVITY_PAGE_H
#define E_MAIL_CONFIG_ACTIVITY_PAGE_H

#include <gtk/gtk.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE \
	(e_mail_config_activity_page_get_type ())
#define E_MAIL_CONFIG_ACTIVITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE, EMailConfigActivityPage))
#define E_MAIL_CONFIG_ACTIVITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE, EMailConfigActivityPageClass))
#define E_IS_MAIL_CONFIG_ACTIVITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE))
#define E_IS_MAIL_CONFIG_ACTIVITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE))
#define E_MAIL_CONFIG_ACTIVITY_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_ACTIVITY_PAGE, EMailConfigActivityPageClass))

G_BEGIN_DECLS

typedef struct _EMailConfigActivityPage EMailConfigActivityPage;
typedef struct _EMailConfigActivityPageClass EMailConfigActivityPageClass;
typedef struct _EMailConfigActivityPagePrivate EMailConfigActivityPagePrivate;

struct _EMailConfigActivityPage {
	GtkScrolledWindow parent;
	EMailConfigActivityPagePrivate *priv;
};

struct _EMailConfigActivityPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_activity_page_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_activity_page_get_internal_box
						(EMailConfigActivityPage *page);
EActivity *	e_mail_config_activity_page_new_activity
						(EMailConfigActivityPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_ACTIVITY_PAGE_H */

