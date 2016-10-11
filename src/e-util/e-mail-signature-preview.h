/*
 * e-mail-signature-preview.h
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

#ifndef E_MAIL_SIGNATURE_PREVIEW_H
#define E_MAIL_SIGNATURE_PREVIEW_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-web-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIGNATURE_PREVIEW \
	(e_mail_signature_preview_get_type ())
#define E_MAIL_SIGNATURE_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIGNATURE_PREVIEW, EMailSignaturePreview))
#define E_MAIL_SIGNATURE_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIGNATURE_PREVIEW, EMailSignaturePreviewClass))
#define E_IS_MAIL_SIGNATURE_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIGNATURE_PREVIEW))
#define E_IS_MAIL_SIGNATURE_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIGNATURE_PREVIEW))
#define E_MAIL_SIGNATURE_PREVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIGNATURE_PREVIEW, EMailSignaturePreview))

G_BEGIN_DECLS

typedef struct _EMailSignaturePreview EMailSignaturePreview;
typedef struct _EMailSignaturePreviewClass EMailSignaturePreviewClass;
typedef struct _EMailSignaturePreviewPrivate EMailSignaturePreviewPrivate;

struct _EMailSignaturePreview {
	EWebView parent;
	EMailSignaturePreviewPrivate *priv;
};

struct _EMailSignaturePreviewClass {
	EWebViewClass parent_class;

	/* Signals */
	void		(*refresh)	(EMailSignaturePreview *preview);
};

GType		e_mail_signature_preview_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_signature_preview_new
					(ESourceRegistry *registry);
void		e_mail_signature_preview_refresh
					(EMailSignaturePreview *preview);
ESourceRegistry *
		e_mail_signature_preview_get_registry
					(EMailSignaturePreview *preview);
const gchar *	e_mail_signature_preview_get_source_uid
					(EMailSignaturePreview *preview);
void		e_mail_signature_preview_set_source_uid
					(EMailSignaturePreview *preview,
					 const gchar *source_uid);

G_END_DECLS

#endif /* E_MAIL_SIGNATURE_PREVIEW_H */
