/*
 * e-mail-part.h
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

#ifndef E_MAIL_PART_H
#define E_MAIL_PART_H

#include <camel/camel.h>

#include <e-util/e-util.h>

#include <em-format/e-mail-formatter-enums.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART \
	(e_mail_part_get_type ())
#define E_MAIL_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART, EMailPart))
#define E_MAIL_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART, EMailPartClass))
#define E_IS_MAIL_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART))
#define E_IS_MAIL_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART))
#define E_MAIL_PART_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART, EMailPartClass))

G_BEGIN_DECLS

struct _EMailPartList;

typedef struct _EMailPart EMailPart;
typedef struct _EMailPartClass EMailPartClass;
typedef struct _EMailPartPrivate EMailPartPrivate;

typedef struct _EMailPartValidityPair EMailPartValidityPair;

struct _EMailPartValidityPair {
	EMailPartValidityFlags validity_type;
	CamelCipherValidity *validity;
};

struct _EMailPart {
	GObject parent;
	EMailPartPrivate *priv;

	GQueue validities;  /* element-type: EMailPartValidityPair */

	/* Whether the part should be rendered or not.
	 * This is used for example to prevent images
	 * related to text/html parts from being
	 * rendered as attachments. */
	guint is_hidden: 1;

	/* Force attachment to be expanded, even without
	 * content-disposition: inline */
	guint force_inline: 1;

	/* Force attachment to be collapsed, even with
	 * content-disposition: inline */
	guint force_collapse: 1;

	/* Does part contain an error message? */
	guint is_error: 1;
};

struct _EMailPartClass {
	GObjectClass parent_class;

	void		(*content_loaded)	(EMailPart *part,
						 EWebView *web_view,
						 const gchar *iframe_id);
};

GType		e_mail_part_get_type		(void) G_GNUC_CONST;
EMailPart *	e_mail_part_new			(CamelMimePart *mime_part,
						 const gchar *id);
const gchar *	e_mail_part_get_id		(EMailPart *part);
const gchar *	e_mail_part_get_cid		(EMailPart *part);
void		e_mail_part_set_cid		(EMailPart *part,
						 const gchar *cid);
gboolean	e_mail_part_id_has_prefix	(EMailPart *part,
						 const gchar *prefix);
gboolean	e_mail_part_id_has_suffix	(EMailPart *part,
						 const gchar *suffix);
gboolean	e_mail_part_id_has_substr	(EMailPart *part,
						 const gchar *substr);
CamelMimePart *	e_mail_part_ref_mime_part	(EMailPart *part);
const gchar *	e_mail_part_get_mime_type	(EMailPart *part);
void		e_mail_part_set_mime_type	(EMailPart *part,
						 const gchar *mime_type);
gboolean	e_mail_part_get_converted_to_utf8
						(EMailPart *part);
void		e_mail_part_set_converted_to_utf8
						(EMailPart *part,
						 gboolean converted_to_utf8);
gboolean	e_mail_part_should_show_inline	(EMailPart *part);
struct _EMailPartList *
		e_mail_part_ref_part_list	(EMailPart *part);
void		e_mail_part_set_part_list	(EMailPart *part,
						 struct _EMailPartList *part_list);
gboolean	e_mail_part_get_is_attachment	(EMailPart *part);
void		e_mail_part_set_is_attachment	(EMailPart *part,
						 gboolean is_attachment);
gboolean	e_mail_part_get_is_printable	(EMailPart *part);
void		e_mail_part_set_is_printable	(EMailPart *part,
						 gboolean is_printable);
void		e_mail_part_content_loaded	(EMailPart *part,
						 EWebView *web_view,
						 const gchar *iframe_id);
void		e_mail_part_update_validity	(EMailPart *part,
						 CamelCipherValidity *validity,
						 EMailPartValidityFlags validity_type);
CamelCipherValidity *
		e_mail_part_get_validity	(EMailPart *part,
						 EMailPartValidityFlags validity_type);
gboolean	e_mail_part_has_validity	(EMailPart *part);
EMailPartValidityFlags
		e_mail_part_get_validity_flags	(EMailPart *part);
void		e_mail_part_verify_validity_sender
						(EMailPart *part,
						 CamelInternetAddress *from_address);

G_END_DECLS

#endif /* E_MAIL_PART_H */
