/*
 * e-mail-identity-combo-box.h
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

#ifndef E_MAIL_IDENTITY_COMBO_BOX_H
#define E_MAIL_IDENTITY_COMBO_BOX_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_IDENTITY_COMBO_BOX \
	(e_mail_identity_combo_box_get_type ())
#define E_MAIL_IDENTITY_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_IDENTITY_COMBO_BOX, EMailIdentityComboBox))
#define E_MAIL_IDENTITY_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_IDENTITY_COMBO_BOX, EMailIdentityComboBoxClass))
#define E_IS_MAIL_IDENTITY_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_IDENTITY_COMBO_BOX))
#define E_IS_MAIL_IDENTITY_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_IDENTITY_COMBO_BOX))
#define E_MAIL_IDENTITY_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_IDENTITY_COMBO_BOX, EMailIdentityComboBoxClass))

G_BEGIN_DECLS

typedef struct _EMailIdentityComboBox EMailIdentityComboBox;
typedef struct _EMailIdentityComboBoxClass EMailIdentityComboBoxClass;
typedef struct _EMailIdentityComboBoxPrivate EMailIdentityComboBoxPrivate;

/**
 * EMailIdentityComboBox:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EMailIdentityComboBox {
	GtkComboBox parent;
	EMailIdentityComboBoxPrivate *priv;
};

struct _EMailIdentityComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_mail_identity_combo_box_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_identity_combo_box_new
					(ESourceRegistry *registry);
void		e_mail_identity_combo_box_refresh
					(EMailIdentityComboBox *combo_box);
ESourceRegistry *
		e_mail_identity_combo_box_get_registry
					(EMailIdentityComboBox *combo_box);
gboolean	e_mail_identity_combo_box_get_allow_none
					(EMailIdentityComboBox *combo_box);
void		e_mail_identity_combo_box_set_allow_none
					(EMailIdentityComboBox *combo_box,
					 gboolean allow_none);
gboolean	e_mail_identity_combo_box_get_refreshing
					(EMailIdentityComboBox *combo_box);

G_END_DECLS

#endif /* E_MAIL_IDENTITY_COMBO_BOX_H */
