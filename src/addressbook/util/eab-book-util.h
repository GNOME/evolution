/*
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
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EAB_BOOK_UTIL_H
#define EAB_BOOK_UTIL_H

#include <libebook/libebook.h>

G_BEGIN_DECLS

typedef struct {
	EContactField field_id;
	const gchar *type_1;
	const gchar *type_2;
	const gchar *text;
} EABTypeLabel;

const EABTypeLabel *eab_get_phone_type_labels 	(gint *n_elements);
const gchar *	eab_get_phone_label_text 	(EVCardAttribute *attr);
gint		eab_get_phone_type_index 	(EVCardAttribute *attr);
void		eab_phone_index_to_type 	(gint index,
						 const gchar **type_1,
						 const gchar **type_2);

const EABTypeLabel *eab_get_im_type_labels	(gint *n_elements);
gint		eab_get_im_type_index 		(EVCardAttribute *attr);
const gchar * 	eab_get_im_label_text		(EVCardAttribute *attr);
const gchar *	eab_get_impp_label_text		(const gchar *impp_value,
						 EContactField *out_equiv_field,
						 guint *out_scheme_len);

const EABTypeLabel *eab_get_email_type_labels 	(gint *n_elements);
const gchar *	eab_get_email_label_text 	(EVCardAttribute *attr);
gint		eab_get_email_type_index 	(EVCardAttribute *attr);
void		eab_email_index_to_type 	(gint index,
						 const gchar **type_1);

const EABTypeLabel *eab_get_sip_type_labels 	(gint *n_elements);
const gchar *	eab_get_sip_label_text 		(EVCardAttribute *attr);
gint		eab_get_sip_type_index 		(EVCardAttribute *attr);
void		eab_sip_index_to_type 		(gint index,
						 const gchar **type_1);

GSList *	eab_contact_list_from_string	(const gchar *str);
gchar *		eab_contact_list_to_string	(const GSList *contacts);
gchar *		eab_contact_array_to_string	(const GPtrArray *contacts); /* EContact * */

gboolean	eab_source_and_contact_list_from_string
						(ESourceRegistry *registry,
						 const gchar *str,
						 ESource **out_source,
						 GSList **out_contacts);
gchar *		eab_book_and_contact_array_to_string
						(EBookClient *book_client,
						 const GPtrArray *contacts);
gint		e_utf8_casefold_collate_len	(const gchar *str1,
						 const gchar *str2,
						 gint len);
gint		e_utf8_casefold_collate		(const gchar *str1,
						 const gchar *str2);

/* To parse quoted printable address & return email & name fields */
gboolean	eab_parse_qp_email		(const gchar *string,
						 gchar **name,
						 gchar **email);
gchar *		eab_parse_qp_email_to_html	(const gchar *string);

EContact *	eab_new_contact_for_book	(EBookClient *book_client);

G_END_DECLS

#endif /* EAB_BOOK_UTIL_H */
