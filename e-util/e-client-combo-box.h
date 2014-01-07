/*
 * e-client-combo-box.h
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

#ifndef E_CLIENT_COMBO_BOX_H
#define E_CLIENT_COMBO_BOX_H

#include <e-util/e-client-cache.h>
#include <e-util/e-source-combo-box.h>

/* Standard GObject macros */
#define E_TYPE_CLIENT_COMBO_BOX \
	(e_client_combo_box_get_type ())
#define E_CLIENT_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CLIENT_COMBO_BOX, EClientComboBox))
#define E_CLIENT_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CLIENT_COMBO_BOX, EClientComboBoxClass))
#define E_IS_CLIENT_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CLIENT_COMBO_BOX))
#define E_IS_CLIENT_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CLIENT_COMBO_BOX))
#define E_CLIENT_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CLIENT_COMBO_BOX, EClientComboBoxClass))

G_BEGIN_DECLS

typedef struct _EClientComboBox EClientComboBox;
typedef struct _EClientComboBoxClass EClientComboBoxClass;
typedef struct _EClientComboBoxPrivate EClientComboBoxPrivate;

struct _EClientComboBox {
	ESourceComboBox parent;
	EClientComboBoxPrivate *priv;
};

struct _EClientComboBoxClass {
	ESourceComboBoxClass parent_class;
};

GType		e_client_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_client_combo_box_new		(EClientCache *client_cache,
						 const gchar *extension_name);
EClientCache *	e_client_combo_box_ref_client_cache
						(EClientComboBox *combo_box);
void		e_client_combo_box_set_client_cache
						(EClientComboBox *combo_box,
						 EClientCache *client_cache);
EClient *	e_client_combo_box_get_client_sync
						(EClientComboBox *combo_box,
						 ESource *source,
						 GCancellable *cancellable,
						 GError **error);
void		e_client_combo_box_get_client	(EClientComboBox *combo_box,
						 ESource *source,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EClient *	e_client_combo_box_get_client_finish
						(EClientComboBox *combo_box,
						 GAsyncResult *result,
						 GError **error);
EClient *	e_client_combo_box_ref_cached_client
						(EClientComboBox *combo_box,
						 ESource *source);

G_END_DECLS

#endif /* E_CLIENT_COMBO_BOX_H */

