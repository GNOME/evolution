/*
 * e-auth-combo-box.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_AUTH_COMBO_BOX_H
#define E_AUTH_COMBO_BOX_H

#include <gtk/gtk.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_AUTH_COMBO_BOX \
	(e_auth_combo_box_get_type ())
#define E_AUTH_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_AUTH_COMBO_BOX, EAuthComboBox))
#define E_AUTH_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_AUTH_COMBO_BOX, EAuthComboBoxClass))
#define E_IS_AUTH_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_AUTH_COMBO_BOX))
#define E_IS_AUTH_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_AUTH_COMBO_BOX))
#define E_AUTH_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_AUTH_COMBO_BOX, EAuthComboBoxClass))

G_BEGIN_DECLS

typedef struct _EAuthComboBox EAuthComboBox;
typedef struct _EAuthComboBoxClass EAuthComboBoxClass;
typedef struct _EAuthComboBoxPrivate EAuthComboBoxPrivate;

struct _EAuthComboBox {
	GtkComboBox parent;
	EAuthComboBoxPrivate *priv;
};

struct _EAuthComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_auth_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_auth_combo_box_new		(void);
CamelProvider *	e_auth_combo_box_get_provider	(EAuthComboBox *combo_box);
void		e_auth_combo_box_set_provider	(EAuthComboBox *combo_box,
						 CamelProvider *provider);
void		e_auth_combo_box_add_auth_type	(EAuthComboBox *combo_box,
						 CamelServiceAuthType *auth_type);
void		e_auth_combo_box_remove_auth_type
						(EAuthComboBox *combo_box,
						 CamelServiceAuthType *auth_type);
void		e_auth_combo_box_update_available
						(EAuthComboBox *combo_box,
						 GList *available_authtypes);
void		e_auth_combo_box_pick_highest_available
						(EAuthComboBox *combo_box);

G_END_DECLS

#endif /* E_AUTH_COMBO_BOX_H */

