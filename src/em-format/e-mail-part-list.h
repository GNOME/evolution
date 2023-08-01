/*
 * e-mail-part-list.h
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

#ifndef E_MAIL_PART_LIST_H
#define E_MAIL_PART_LIST_H

#include <camel/camel.h>
#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_LIST \
	(e_mail_part_list_get_type ())
#define E_MAIL_PART_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_LIST, EMailPartList))
#define E_MAIL_PART_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_LIST, EMailPartListClass))
#define E_IS_MAIL_PART_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_LIST))
#define E_IS_MAIL_PART_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_LIST))
#define E_MAIL_PART_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_LIST, EMailPartListClass))

G_BEGIN_DECLS

typedef struct _EMailAutocryptKey {
	CamelGpgKeyInfo *info;
	guint8 *keydata;
	gsize keydata_size;
} EMailAutocryptKey;

EMailAutocryptKey *
		e_mail_autocrypt_key_new	(CamelGpgKeyInfo *info,
						 guint8 *keydata,
						 gsize keydata_size);
void		e_mail_autocrypt_key_free	(EMailAutocryptKey *key);

typedef struct _EMailPartList EMailPartList;
typedef struct _EMailPartListClass EMailPartListClass;
typedef struct _EMailPartListPrivate EMailPartListPrivate;

struct _EMailPartList {
	GObject parent;
	EMailPartListPrivate *priv;
};

struct _EMailPartListClass {
	GObjectClass parent_class;
};

GType		e_mail_part_list_get_type	(void) G_GNUC_CONST;
EMailPartList *	e_mail_part_list_new		(CamelMimeMessage *message,
						 const gchar *message_uid,
						 CamelFolder *folder);
CamelFolder *	e_mail_part_list_get_folder	(EMailPartList *part_list);
CamelMimeMessage *
		e_mail_part_list_get_message	(EMailPartList *part_list);
const gchar *	e_mail_part_list_get_message_uid
						(EMailPartList *part_list);
void		e_mail_part_list_add_part	(EMailPartList *part_list,
						 EMailPart *part);
EMailPart *	e_mail_part_list_ref_part	(EMailPartList *part_list,
						 const gchar *part_id);
guint		e_mail_part_list_queue_parts	(EMailPartList *part_list,
						 const gchar *part_id,
						 GQueue *result_queue);
gboolean	e_mail_part_list_is_empty	(EMailPartList *part_list);
void		e_mail_part_list_sum_validity	(EMailPartList *part_list,
						 EMailPartValidityFlags *out_validity_pgp_sum,
						 EMailPartValidityFlags *out_validity_smime_sum);
GPtrArray * /* EMailAutocryptKey * */
		e_mail_part_list_get_autocrypt_keys
						(EMailPartList *part_list);
void		e_mail_part_list_take_autocrypt_keys
						(EMailPartList *part_list,
						 GPtrArray *keys); /* EMailAutocryptKey * */

CamelObjectBag *
		e_mail_part_list_get_registry	(void);

G_END_DECLS

#endif /* E_MAIL_PART_LIST_H */ 
