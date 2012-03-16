/*
 * e-mail-config-header-manager.h
 *
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
 */

#ifndef E_MAIL_CONFIG_HEADER_MANAGER_H
#define E_MAIL_CONFIG_HEADER_MANAGER_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_HEADER_MANAGER \
	(e_mail_config_header_manager_get_type ())
#define E_MAIL_CONFIG_HEADER_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_HEADER_MANAGER, EMailConfigHeaderManager))
#define E_MAIL_CONFIG_HEADER_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_HEADER_MANAGER, EMailConfigHeaderManagerClass))
#define E_IS_MAIL_CONFIG_HEADER_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_HEADER_MANAGER))
#define E_IS_MAIL_CONFIG_HEADER_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_HEADER_MANAGER))
#define E_MAIL_CONFIG_HEADER_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_HEADER_MANAGER, EMailConfigHeaderManagerClass))

G_BEGIN_DECLS

typedef struct _EMailConfigHeaderManager EMailConfigHeaderManager;
typedef struct _EMailConfigHeaderManagerClass EMailConfigHeaderManagerClass;
typedef struct _EMailConfigHeaderManagerPrivate EMailConfigHeaderManagerPrivate;

struct _EMailConfigHeaderManager {
	GtkGrid parent;
	EMailConfigHeaderManagerPrivate *priv;
};

struct _EMailConfigHeaderManagerClass {
	GtkGridClass parent_class;
};

GType		e_mail_config_header_manager_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_header_manager_type_register
						(GTypeModule *type_module);
GtkWidget *	e_mail_config_header_manager_new
						(void);
gchar **	e_mail_config_header_manager_dup_headers
						(EMailConfigHeaderManager *manager);
void		e_mail_config_header_manager_set_headers
						(EMailConfigHeaderManager *manager,
						 const gchar * const *headers);

G_END_DECLS

#endif /* E_MAIL_CONFIG_HEADER_MANAGER_H */
