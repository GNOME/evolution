/*
 * e-mail-part.h
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

#ifndef E_MAIL_PART_H
#define E_MAIL_PART_H

#include <camel/camel.h>
#include <webkit/webkitdom.h>

#include <e-util/e-util.h>

#define E_MAIL_PART_IS(p,s_t) \
		((p != NULL) && (e_mail_part_get_instance_size (p) == sizeof (s_t)))
#define E_MAIL_PART(o) ((EMailPart *) o)

G_BEGIN_DECLS

typedef struct _EMailPart EMailPart;
typedef struct _EMailPartPrivate EMailPartPrivate;

typedef void	(*EMailPartDOMBindFunc)	(EMailPart *part,
					 WebKitDOMElement *element);

typedef enum {
	E_MAIL_PART_VALIDITY_NONE      = 0,
	E_MAIL_PART_VALIDITY_PGP       = 1 << 0,
	E_MAIL_PART_VALIDITY_SMIME     = 1 << 1,
	E_MAIL_PART_VALIDITY_SIGNED    = 1 << 2,
	E_MAIL_PART_VALIDITY_ENCRYPTED = 1 << 3
} EMailPartValidityFlags;

typedef struct _EMailPartValidityPair EMailPartValidityPair;

struct _EMailPartValidityPair {
	EMailPartValidityFlags validity_type;
	CamelCipherValidity *validity;
};

struct _EMailPart {
	EMailPartPrivate *priv;

	EMailPartDOMBindFunc bind_func;

	CamelMimePart *part;
	gchar *id;
	gchar *cid;
	gchar *mime_type;

	GQueue validities;  /* element-type: EMailPartValidityPair */

	gint is_attachment: 1;

	/* Whether the part should be rendered or not.
	 * This is used for example to prevent images
	 * related to text/html parts from being
	 * rendered as attachments. */
	gint is_hidden: 1;

	/* Force attachment to be expanded, even without
	 * content-disposition: inline */
	gint force_inline: 1;

	/* Force attachment to be collapsed, even with
	 * content-disposition: inline */
	gint force_collapse: 1;

	/* Does part contain an error message? */
	gint is_error: 1;
};

EMailPart *	e_mail_part_new			(CamelMimePart *part,
						 const gchar *id);
EMailPart *	e_mail_part_subclass_new	(CamelMimePart *part,
						 const gchar *id,
						 gsize size,
						 GFreeFunc free_func);

EMailPart *	e_mail_part_ref			(EMailPart *part);
void		e_mail_part_unref		(EMailPart *part);

gsize		e_mail_part_get_instance_size	(EMailPart *part);

const gchar *	e_mail_part_get_id		(EMailPart *part);
void		e_mail_part_update_validity	(EMailPart *part,
						 CamelCipherValidity *validity,
						 guint32 validity_type);
CamelCipherValidity *
		e_mail_part_get_validity	(EMailPart *part,
						 guint32 validity_type);

G_END_DECLS

#endif /* E_MAIL_PART_H */
