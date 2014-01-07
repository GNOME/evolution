/*
 * e-mail-config-sidebar.h
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

#ifndef E_MAIL_CONFIG_SIDEBAR_H
#define E_MAIL_CONFIG_SIDEBAR_H

#include <mail/e-mail-config-notebook.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SIDEBAR \
	(e_mail_config_sidebar_get_type ())
#define E_MAIL_CONFIG_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SIDEBAR, EMailConfigSidebar))
#define E_MAIL_CONFIG_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SIDEBAR, EMailConfigSidebarClass))
#define E_IS_MAIL_CONFIG_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SIDEBAR))
#define E_IS_MAIL_CONFIG_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SIDEBAR))
#define E_MAIL_CONFIG_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SIDEBAR, EMailConfigSidebarClass))

G_BEGIN_DECLS

typedef struct _EMailConfigSidebar EMailConfigSidebar;
typedef struct _EMailConfigSidebarClass EMailConfigSidebarClass;
typedef struct _EMailConfigSidebarPrivate EMailConfigSidebarPrivate;

struct _EMailConfigSidebar {
	GtkButtonBox parent;
	EMailConfigSidebarPrivate *priv;
};

struct _EMailConfigSidebarClass {
	GtkButtonBoxClass parent_class;
};

GType		e_mail_config_sidebar_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_sidebar_new	(EMailConfigNotebook *notebook);
gint		e_mail_config_sidebar_get_active
						(EMailConfigSidebar *sidebar);
void		e_mail_config_sidebar_set_active
						(EMailConfigSidebar *sidebar,
						 gint active);
EMailConfigNotebook *
		e_mail_config_sidebar_get_notebook
						(EMailConfigSidebar *sidebar);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SIDEBAR_H */

