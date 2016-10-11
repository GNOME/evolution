/*
 * e-mail-part-headers.h
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

#ifndef E_MAIL_PART_HEADERS_H
#define E_MAIL_PART_HEADERS_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_HEADERS \
	(e_mail_part_headers_get_type ())
#define E_MAIL_PART_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_HEADERS, EMailPartHeaders))
#define E_MAIL_PART_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_HEADERS, EMailPartHeadersClass))
#define E_IS_MAIL_PART_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_HEADERS))
#define E_IS_MAIL_PART_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_HEADERS))
#define E_MAIL_PART_HEADERS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_HEADERS, EMailPartHeadersClass))

#define E_MAIL_PART_HEADERS_MIME_TYPE \
	"application/vnd.evolution.headers"

G_BEGIN_DECLS

typedef struct _EMailPartHeaders EMailPartHeaders;
typedef struct _EMailPartHeadersClass EMailPartHeadersClass;
typedef struct _EMailPartHeadersPrivate EMailPartHeadersPrivate;

struct _EMailPartHeaders {
	EMailPart parent;
	EMailPartHeadersPrivate *priv;
};

struct _EMailPartHeadersClass {
	EMailPartClass parent_class;
};

typedef enum {
	E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE,
	E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_NAME,
	E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_VALUE,
	E_MAIL_PART_HEADERS_PRINT_MODEL_NUM_COLUMNS
} EMailPartHeadersPrintModelColumns;

GType		e_mail_part_headers_get_type	(void) G_GNUC_CONST;
EMailPart *	e_mail_part_headers_new		(CamelMimePart *mime_part,
						 const gchar *id);
gchar **	e_mail_part_headers_dup_default_headers
						(EMailPartHeaders *part);
void		e_mail_part_headers_set_default_headers
						(EMailPartHeaders *part,
						 const gchar * const *default_headers);
gboolean	e_mail_part_headers_is_default	(EMailPartHeaders *part,
						 const gchar *header_name);
GtkTreeModel *	e_mail_part_headers_ref_print_model
						(EMailPartHeaders *part);

G_END_DECLS

#endif /* E_MAIL_PART_HEADERS_H */

