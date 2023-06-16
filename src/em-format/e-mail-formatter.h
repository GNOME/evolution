/*
 * e-mail-formatter.h
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

#ifndef E_MAIL_FORMATTER_H_
#define E_MAIL_FORMATTER_H_

#include <gdk/gdk.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-formatter-enums.h>
#include <em-format/e-mail-part-list.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER \
	(e_mail_formatter_get_type ())
#define E_MAIL_FORMATTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FORMATTER, EMailFormatter))
#define E_MAIL_FORMATTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FORMATTER, EMailFormatterClass))
#define E_IS_MAIL_FORMATTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FORMATTER))
#define E_IS_MAIL_FORMATTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FORMATTER))
#define E_MAIL_FORMATTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FORMATTER, EMailFormatterClass))

G_BEGIN_DECLS;

typedef struct _EMailFormatter EMailFormatter;
typedef struct _EMailFormatterClass EMailFormatterClass;
typedef struct _EMailFormatterPrivate EMailFormatterPrivate;
typedef struct _EMailFormatterContext EMailFormatterContext;

struct _EMailFormatterContext {
	EMailPartList *part_list;
	EMailFormatterMode mode;
	EMailFormatterHeaderFlags flags;

	gchar *uri;
};

struct _EMailFormatter {
	GObject parent;
	EMailFormatterPrivate *priv;
};

struct _EMailFormatterClass {
	GObjectClass parent_class;

	EMailFormatterExtensionRegistry *extension_registry;
	CamelMimeFilterToHTMLFlags text_html_flags;

	/* sizeof(EMailFormatterContext) or some derivative struct */
	gsize context_size;

	void		(*run)			(EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 GOutputStream *stream,
						 GCancellable *cancellable);

	void		(*update_style)		(EMailFormatter *formatter,
						 GtkStateFlags state);

	/* Signals */
	void		(*need_redraw)		(EMailFormatter *formatter);
	void		(*claim_attachment)	(EMailFormatter *formatter,
						 EAttachment *attachment);
};

GType		e_mail_formatter_get_type	(void);

EMailFormatter *
		e_mail_formatter_new		(void);
void		e_mail_formatter_claim_attachment
						(EMailFormatter *formatter,
						 EAttachment *attachment);
void		e_mail_formatter_format_sync	(EMailFormatter *formatter,
						 EMailPartList *part_list,
						 GOutputStream *stream,
						 EMailFormatterHeaderFlags flags,
						 EMailFormatterMode mode,
						 GCancellable *cancellable);

void		e_mail_formatter_format		(EMailFormatter *formatter,
						 EMailPartList *part_list,
						 GOutputStream *stream,
						 EMailFormatterHeaderFlags flags,
						 EMailFormatterMode mode,
						 GAsyncReadyCallback callback,
						 GCancellable *cancellable,
						 gpointer user_data);

gboolean	e_mail_formatter_format_finish	(EMailFormatter *formatter,
						 GAsyncResult *result,
						 GError **error);

gboolean	e_mail_formatter_format_as	(EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 EMailPart *part,
						 GOutputStream *stream,
						 const gchar *as_mime_type,
						 GCancellable *cancellable);

void		e_mail_formatter_format_text	(EMailFormatter *formatter,
						 EMailPart *part,
						 GOutputStream *stream,
						 GCancellable *cancellable);
const gchar *	e_mail_formatter_get_sub_html_header
						(EMailFormatter *formatter);
gchar *		e_mail_formatter_get_html_header
						(EMailFormatter *formatter);
EMailExtensionRegistry *
		e_mail_formatter_get_extension_registry
						(EMailFormatter *formatter);

CamelMimeFilterToHTMLFlags
		e_mail_formatter_get_text_format_flags
						(EMailFormatter *formatter);

const GdkRGBA *	e_mail_formatter_get_color	(EMailFormatter *formatter,
						 EMailFormatterColor type);
void		e_mail_formatter_set_color	(EMailFormatter *formatter,
						 EMailFormatterColor type,
						 const GdkRGBA *color);
void		e_mail_formatter_update_style	(EMailFormatter *formatter,
						 GtkStateFlags state);

EImageLoadingPolicy
		e_mail_formatter_get_image_loading_policy
						(EMailFormatter *formatter);
void		e_mail_formatter_set_image_loading_policy
						(EMailFormatter *formatter,
						 EImageLoadingPolicy policy);

gboolean	e_mail_formatter_get_mark_citations
						(EMailFormatter *formatter);
void		e_mail_formatter_set_mark_citations
						(EMailFormatter *formatter,
						 gboolean mark_citations);

gboolean	e_mail_formatter_get_show_sender_photo
						(EMailFormatter *formatter);
void		e_mail_formatter_set_show_sender_photo
						(EMailFormatter *formatter,
						 gboolean show_sender_photo);

gboolean	e_mail_formatter_get_animate_images
						(EMailFormatter *formatter);
void		e_mail_formatter_set_animate_images
						(EMailFormatter *formatter,
						 gboolean animate_images);

gboolean	e_mail_formatter_get_show_real_date
						(EMailFormatter *formatter);
void		e_mail_formatter_set_show_real_date
						(EMailFormatter *formatter,
						 gboolean show_real_date);

const gchar *	e_mail_formatter_get_charset	(EMailFormatter *formatter);
gchar *		e_mail_formatter_dup_charset	(EMailFormatter *formatter);
void		e_mail_formatter_set_charset	(EMailFormatter *formatter,
						 const gchar *charset);

const gchar *	e_mail_formatter_get_default_charset
						(EMailFormatter *formatter);
gchar *		e_mail_formatter_dup_default_charset
						(EMailFormatter *formatter);
void		e_mail_formatter_set_default_charset
						(EMailFormatter *formatter,
						 const gchar *charset);

G_END_DECLS

#endif /* E_MAIL_FORMATTER_H_ */
