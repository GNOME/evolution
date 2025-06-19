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

enum {
	E_MAIL_IDENTITY_COMBO_BOX_COLUMN_DISPLAY_NAME,
	E_MAIL_IDENTITY_COMBO_BOX_COLUMN_COMBO_ID,
	E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID,
	E_MAIL_IDENTITY_COMBO_BOX_COLUMN_NAME,
	E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS
};

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
const gchar *	e_mail_identity_combo_box_get_none_title
					(EMailIdentityComboBox *combo_box);
void		e_mail_identity_combo_box_set_none_title
					(EMailIdentityComboBox *combo_box,
					 const gchar *none_title);
gboolean	e_mail_identity_combo_box_get_allow_aliases
					(EMailIdentityComboBox *combo_box);
void		e_mail_identity_combo_box_set_allow_aliases
					(EMailIdentityComboBox *combo_box,
					 gboolean allow_aliases);
gboolean	e_mail_identity_combo_box_get_active_uid
					(EMailIdentityComboBox *combo_box,
					 gchar **identity_uid,
					 gchar **alias_name,
					 gchar **alias_address);
gboolean	e_mail_identity_combo_box_set_active_uid
					(EMailIdentityComboBox *combo_box,
					 const gchar *identity_uid,
					 const gchar *alias_name,
					 const gchar *alias_address);
gboolean	e_mail_identity_combo_box_get_refreshing
					(EMailIdentityComboBox *combo_box);
gint		e_mail_identity_combo_box_get_max_natural_width
					(EMailIdentityComboBox *self);
void		e_mail_identity_combo_box_set_max_natural_width
					(EMailIdentityComboBox *self,
					 gint value);
gint		e_mail_identity_combo_box_get_last_natural_width
					(EMailIdentityComboBox *self);

G_END_DECLS

#endif /* E_MAIL_IDENTITY_COMBO_BOX_H */
