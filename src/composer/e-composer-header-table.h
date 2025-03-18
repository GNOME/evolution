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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef E_COMPOSER_HEADER_TABLE_H
#define E_COMPOSER_HEADER_TABLE_H

#include <libebook/libebook.h>

#include <composer/e-composer-header.h>

/* Standard GObject macros */
#define E_TYPE_COMPOSER_HEADER_TABLE \
	(e_composer_header_table_get_type ())
#define E_COMPOSER_HEADER_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTable))
#define E_COMPOSER_HEADER_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTableClass))
#define E_IS_COMPOSER_HEADER_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE))
#define E_IS_COMPOSER_HEADER_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_HEADER_TABLE))
#define E_COMPOSER_HEADER_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTableClass))

G_BEGIN_DECLS

typedef struct _EComposerHeaderTable EComposerHeaderTable;
typedef struct _EComposerHeaderTableClass EComposerHeaderTableClass;
typedef struct _EComposerHeaderTablePrivate EComposerHeaderTablePrivate;

/* Headers, listed in the order they should appear in the table. */
typedef enum {
	E_COMPOSER_HEADER_FROM,
	E_COMPOSER_HEADER_REPLY_TO,
	E_COMPOSER_HEADER_MAIL_REPLY_TO,
	E_COMPOSER_HEADER_MAIL_FOLLOWUP_TO,
	E_COMPOSER_HEADER_TO,
	E_COMPOSER_HEADER_CC,
	E_COMPOSER_HEADER_BCC,
	E_COMPOSER_HEADER_POST_TO,
	E_COMPOSER_HEADER_SUBJECT,
	E_COMPOSER_NUM_HEADERS
} EComposerHeaderType;

struct _EComposerHeaderTable {
	GtkGrid parent;
	EComposerHeaderTablePrivate *priv;
};

struct _EComposerHeaderTableClass {
	GtkGridClass parent_class;
};

GType		e_composer_header_table_get_type (void);
GtkWidget *	e_composer_header_table_new	(EClientCache *client_cache);
EClientCache *	e_composer_header_table_ref_client_cache
						(EComposerHeaderTable *table);
EComposerHeader *
		e_composer_header_table_get_header
						(EComposerHeaderTable *table,
						 EComposerHeaderType type);
EMailSignatureComboBox *
		e_composer_header_table_get_signature_combo_box
						(EComposerHeaderTable *table);
EDestination ** e_composer_header_table_get_destinations
						(EComposerHeaderTable *table);
EDestination **	e_composer_header_table_get_destinations_bcc
						(EComposerHeaderTable *table);
void		e_composer_header_table_add_destinations_bcc
						(EComposerHeaderTable *table,
						 EDestination **destination);
void		e_composer_header_table_set_destinations_bcc
						(EComposerHeaderTable *table,
						 EDestination **destinations);
EDestination **	e_composer_header_table_get_destinations_cc
						(EComposerHeaderTable *table);
void		e_composer_header_table_add_destinations_cc
						(EComposerHeaderTable *table,
						 EDestination **destination);
void		e_composer_header_table_set_destinations_cc
						(EComposerHeaderTable *table,
						 EDestination **destinations);
EDestination **	e_composer_header_table_get_destinations_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_add_destinations_to
						(EComposerHeaderTable *table,
						 EDestination **destinations);
void		e_composer_header_table_set_destinations_to
						(EComposerHeaderTable *table,
						 EDestination **destinations);
gchar *		e_composer_header_table_dup_identity_uid
						(EComposerHeaderTable *table,
						 gchar **chosen_alias_name,
						 gchar **chosen_alias_address);
void		e_composer_header_table_set_identity_uid
						(EComposerHeaderTable *table,
						 const gchar *identity_uid,
						 const gchar *alias_name,
						 const gchar *alias_address);
const gchar *	e_composer_header_table_get_from_name
						(EComposerHeaderTable *table);
const gchar *	e_composer_header_table_get_from_address
						(EComposerHeaderTable *table);
GList *		e_composer_header_table_get_post_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_post_to_base
						(EComposerHeaderTable *table,
						 const gchar *base_url,
						 const gchar *post_to);
void		e_composer_header_table_set_post_to_list
						(EComposerHeaderTable *table,
						 GList *folder_list);
const gchar *	e_composer_header_table_get_reply_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_reply_to
						(EComposerHeaderTable *table,
						 const gchar *reply_to);
const gchar *	e_composer_header_table_get_mail_followup_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_mail_followup_to
						(EComposerHeaderTable *table,
						 const gchar *mail_followup_to);
const gchar *	e_composer_header_table_get_mail_reply_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_mail_reply_to
						(EComposerHeaderTable *table,
						 const gchar *mail_reply_to);
const gchar *	e_composer_header_table_get_signature_uid
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_signature_uid
						(EComposerHeaderTable *table,
						 const gchar *signature_uid);
const gchar *	e_composer_header_table_get_subject
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_subject
						(EComposerHeaderTable *table,
						 const gchar *subject);
ESource *	e_composer_header_table_ref_source
						(EComposerHeaderTable *table,
						 const gchar *uid);

G_END_DECLS

#endif /* E_COMPOSER_HEADER_TABLE_H */
