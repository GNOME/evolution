/*
 * e-mail-formatter.h
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

#ifndef E_MAIL_FORMATTER_H_
#define E_MAIL_FORMATTER_H_

#include <gdk/gdk.h>
#include <libemail-engine/e-mail-enums.h>

#include <em-format/e-mail-extension-registry.h>
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

typedef enum {
	E_MAIL_FORMATTER_MODE_INVALID			= -1,
	E_MAIL_FORMATTER_MODE_NORMAL			= 0,
	E_MAIL_FORMATTER_MODE_SOURCE,
	E_MAIL_FORMATTER_MODE_RAW,
	E_MAIL_FORMATTER_MODE_CID,
	E_MAIL_FORMATTER_MODE_PRINTING,
	E_MAIL_FORMATTER_MODE_ALL_HEADERS
} EMailFormatterMode;

typedef enum {
	E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSABLE	= 1 << 0,
	E_MAIL_FORMATTER_HEADER_FLAG_COLLAPSED		= 1 << 1,
	E_MAIL_FORMATTER_HEADER_FLAG_HTML		= 1 << 2,
	E_MAIL_FORMATTER_HEADER_FLAG_NOCOLUMNS		= 1 << 3,
	E_MAIL_FORMATTER_HEADER_FLAG_BOLD		= 1 << 4,
	E_MAIL_FORMATTER_HEADER_FLAG_NODEC		= 1 << 5,
	E_MAIL_FORMATTER_HEADER_FLAG_HIDDEN		= 1 << 6,
	E_MAIL_FORMATTER_HEADER_FLAG_NOLINKS		= 1 << 7,
	E_MAIL_FORMATTER_HEADER_FLAG_NOELIPSIZE		= 1 << 8
} EMailFormatterHeaderFlags;

typedef enum {
	E_MAIL_FORMATTER_COLOR_BODY,		/* header area background */
	E_MAIL_FORMATTER_COLOR_CITATION,	/* citation font color */
	E_MAIL_FORMATTER_COLOR_CONTENT,		/* message area background */
	E_MAIL_FORMATTER_COLOR_FRAME,		/* frame around message area */
	E_MAIL_FORMATTER_COLOR_HEADER,		/* header font color */
	E_MAIL_FORMATTER_COLOR_TEXT,		/* message font color */
	E_MAIL_FORMATTER_NUM_COLOR_TYPES
} EMailFormatterColorType;

typedef struct _EMailFormatter EMailFormatter;
typedef struct _EMailFormatterClass EMailFormatterClass;
typedef struct _EMailFormatterPrivate EMailFormatterPrivate;
typedef struct _EMailFormatterHeader EMailFormatterHeader;
typedef struct _EMailFormatterContext EMailFormatterContext;

struct _EMailFormatterHeader {
	EMailFormatterHeaderFlags flags;
	gchar *name;
	gchar *value;
};

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

	/* Colors should apply globally */
	GdkColor colors[E_MAIL_FORMATTER_NUM_COLOR_TYPES];

	/* sizeof(EMailFormatterContext) or some derivative struct */
	gsize context_size;

	void		(*run)			(EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 CamelStream *stream,
						 GCancellable *cancellable);

	void		(*set_style)		(EMailFormatter *formatter,
						 GtkStyle *style,
						 GtkStateType state);

	/* Signals */
	void		(*need_redraw)		(EMailFormatter *formatter);
};

GType		e_mail_formatter_get_type	(void);

EMailFormatter *
		e_mail_formatter_new		(void);

void		e_mail_formatter_format_sync	(EMailFormatter *formatter,
						 EMailPartList *parts_list,
						 CamelStream *stream,
						 EMailFormatterHeaderFlags flags,
						 EMailFormatterMode mode,
						 GCancellable *cancellable);

void		e_mail_formatter_format		(EMailFormatter *formatter,
						 EMailPartList *parts_list,
						 CamelStream *stream,
						 EMailFormatterHeaderFlags flags,
						 EMailFormatterMode mode,
						 GAsyncReadyCallback callback,
						 GCancellable *cancellable,
						 gpointer user_data);

CamelStream *	e_mail_formatter_format_finished
						(EMailFormatter *formatter,
						 GAsyncResult *result,
						 GError *error);

gboolean	e_mail_formatter_format_as	(EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 EMailPart *part,
						 CamelStream *stream,
						 const gchar *as_mime_type,
						 GCancellable *cancellable);

void		e_mail_formatter_format_text	(EMailFormatter *formatter,
						 EMailPart *part,
						 CamelStream *stream,
						 GCancellable *cancellable);
gchar *		e_mail_formatter_get_html_header
						(EMailFormatter *formatter);
EMailExtensionRegistry *
		e_mail_formatter_get_extension_registry
						(EMailFormatter *formatter);

CamelMimeFilterToHTMLFlags
		e_mail_formatter_get_text_format_flags
						(EMailFormatter *formatter);

const GdkColor *
		e_mail_formatter_get_color	(EMailFormatter *formatter,
						 EMailFormatterColorType type);
void		e_mail_formatter_set_color	(EMailFormatter *efh,
						 EMailFormatterColorType type,
						 const GdkColor *color);
void		e_mail_formatter_set_style	(EMailFormatter *formatter,
						 GtkStyle *style,
						 GtkStateType state);

EMailImageLoadingPolicy
		e_mail_formatter_get_image_loading_policy
						(EMailFormatter *formatter);
void		e_mail_formatter_set_image_loading_policy
						(EMailFormatter *formatter,
						 EMailImageLoadingPolicy policy);

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

gboolean        e_mail_formatter_get_animate_images
                                                (EMailFormatter *formatter);
void            e_mail_formatter_set_animate_images
                                                (EMailFormatter *formatter,
                                                 gboolean animate_images);

gboolean	e_mail_formatter_get_show_real_date
						(EMailFormatter *formatter);
void		e_mail_formatter_set_show_real_date
						(EMailFormatter *formatter,
						 gboolean show_real_date);

const gchar *	e_mail_formatter_get_charset	(EMailFormatter *formatter);
void		e_mail_formatter_set_charset	(EMailFormatter *formatter,
						 const gchar *charset);

const gchar *	e_mail_formatter_get_default_charset
						(EMailFormatter *formatter);
void		e_mail_formatter_set_default_charset
						(EMailFormatter *formatter,
						 const gchar *charset);

const GQueue *	e_mail_formatter_get_headers	(EMailFormatter *formatter);

void		e_mail_formatter_clear_headers	(EMailFormatter *formatter);

void		e_mail_formatter_set_default_headers
						(EMailFormatter *formatter);

void		e_mail_formatter_add_header	(EMailFormatter *formatter,
						 const gchar *name,
						 const gchar *value,
						 EMailFormatterHeaderFlags flags);

void		e_mail_formatter_add_header_struct
						(EMailFormatter *formatter,
						 const EMailFormatterHeader *header);

void		e_mail_formatter_remove_header	(EMailFormatter *formatter,
						 const gchar *name,
						 const gchar *value);

void		e_mail_formatter_remove_header_struct
						(EMailFormatter *formatter,
						 const EMailFormatterHeader *header);

EMailFormatterHeader *
		e_mail_formatter_header_new	(const gchar *name,
						 const gchar *value);

void		e_mail_formatter_header_free	(EMailFormatterHeader *header);

G_END_DECLS

#endif /* E_MAIL_FORMATTER_H_ */
