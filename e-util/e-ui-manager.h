/*
 * e-ui-manager.h
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

#ifndef E_UI_MANAGER_H
#define E_UI_MANAGER_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_UI_MANAGER \
	(e_ui_manager_get_type ())
#define E_UI_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_UI_MANAGER, EUIManager))
#define E_UI_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_UI_MANAGER, EUIManagerClass))
#define E_IS_UI_MANAGER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_UI_MANAGER))
#define E_IS_UI_MANAGER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_UI_MANAGER))
#define E_UI_MANAGER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_UI_MANAGER, EUIManagerClass))

G_BEGIN_DECLS

typedef struct _EUIManager EUIManager;
typedef struct _EUIManagerClass EUIManagerClass;
typedef struct _EUIManagerPrivate EUIManagerPrivate;

struct _EUIManager {
	GtkUIManager parent;
	EUIManagerPrivate *priv;
};

struct _EUIManagerClass {
	GtkUIManagerClass parent_class;

	gchar *		(*filter_ui)		(EUIManager *ui_manager,
						 const gchar *ui_definition);
};

GType		e_ui_manager_get_type		(void);
GtkUIManager *	e_ui_manager_new		(void);
gboolean	e_ui_manager_get_express_mode	(EUIManager *ui_manager);
void		e_ui_manager_set_express_mode	(EUIManager *ui_manager,
						 gboolean express_mode);
guint		e_ui_manager_add_ui_from_file	(EUIManager *ui_manager,
						 const gchar *basename);
guint		e_ui_manager_add_ui_from_string	(EUIManager *ui_manager,
						 const gchar *ui_definition,
						 GError **error);

G_END_DECLS

#endif /* E_UI_MANAGER_H */
