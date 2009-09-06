/*
 * e-signature-preview.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SIGNATURE_PREVIEW_H
#define E_SIGNATURE_PREVIEW_H

#include <e-util/e-signature.h>
#include <misc/e-web-view.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_PREVIEW \
	(e_signature_preview_get_type ())
#define E_SIGNATURE_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_PREVIEW, ESignaturePreview))
#define E_SIGNATURE_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_PREVIEW, ESignaturePreviewClass))
#define E_IS_SIGNATURE_PREVIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_PREVIEW))
#define E_IS_SIGNATURE_PREVIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_PREVIEW))
#define E_SIGNATURE_PREVIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_PREVIEW, ESignaturePreview))

G_BEGIN_DECLS

typedef struct _ESignaturePreview ESignaturePreview;
typedef struct _ESignaturePreviewClass ESignaturePreviewClass;
typedef struct _ESignaturePreviewPrivate ESignaturePreviewPrivate;

struct _ESignaturePreview {
	EWebView parent;
	ESignaturePreviewPrivate *priv;
};

struct _ESignaturePreviewClass {
	EWebViewClass parent_class;

	/* Signals */
	void		(*refresh)		(ESignaturePreview *preview);
};

GType		e_signature_preview_get_type	(void);
GtkWidget *	e_signature_preview_new		(void);
void		e_signature_preview_refresh	(ESignaturePreview *preview);
gboolean	e_signature_preview_get_allow_scripts
						(ESignaturePreview *preview);
void		e_signature_preview_set_allow_scripts
						(ESignaturePreview *preview,
						 gboolean allow_scripts);
ESignature *	e_signature_preview_get_signature
						(ESignaturePreview *preview);
void		e_signature_preview_set_signature
						(ESignaturePreview *preview,
						 ESignature *signature);

G_END_DECLS

#endif /* E_SIGNATURE_PREVIEW_H */
