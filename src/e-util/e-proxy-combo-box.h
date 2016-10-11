/*
 * e-proxy-combo-box.h
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

#ifndef E_PROXY_COMBO_BOX_H
#define E_PROXY_COMBO_BOX_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_PROXY_COMBO_BOX \
	(e_proxy_combo_box_get_type ())
#define E_PROXY_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PROXY_COMBO_BOX, EProxyComboBox))
#define E_PROXY_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PROXY_COMBO_BOX, EProxyComboBoxClass))
#define E_IS_PROXY_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PROXY_COMBO_BOX))
#define E_IS_PROXY_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PROXY_COMBO_BOX))
#define E_PROXY_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PROXY_COMBO_BOX, EProxyComboBoxClass))

G_BEGIN_DECLS

typedef struct _EProxyComboBox EProxyComboBox;
typedef struct _EProxyComboBoxClass EProxyComboBoxClass;
typedef struct _EProxyComboBoxPrivate EProxyComboBoxPrivate;

/**
 * EProxyComboBox:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EProxyComboBox {
	GtkComboBox parent;
	EProxyComboBoxPrivate *priv;
};

struct _EProxyComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_proxy_combo_box_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_proxy_combo_box_new		(ESourceRegistry *registry);
void		e_proxy_combo_box_refresh	(EProxyComboBox *combo_box);
ESourceRegistry *
		e_proxy_combo_box_get_registry	(EProxyComboBox *combo_box);

G_END_DECLS

#endif /* E_PROXY_COMBO_BOX_H */

