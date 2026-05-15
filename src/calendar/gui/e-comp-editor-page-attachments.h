/*
 * SPDX-FileCopyrightText: (C) 2015 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef E_COMP_EDITOR_PAGE_ATTACHMENTS_H
#define E_COMP_EDITOR_PAGE_ATTACHMENTS_H

#include <e-util/e-util.h>
#include <calendar/gui/e-comp-editor.h>
#include <calendar/gui/e-comp-editor-page.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS \
	(e_comp_editor_page_attachments_get_type ())
#define E_COMP_EDITOR_PAGE_ATTACHMENTS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS, ECompEditorPageAttachments))
#define E_COMP_EDITOR_PAGE_ATTACHMENTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS, ECompEditorPageAttachmentsClass))
#define E_IS_COMP_EDITOR_PAGE_ATTACHMENTS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS))
#define E_IS_COMP_EDITOR_PAGE_ATTACHMENTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS))
#define E_COMP_EDITOR_PAGE_ATTACHMENTS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PAGE_ATTACHMENTS, ECompEditorPageAttachmentsClass))

typedef struct _ECompEditorPageAttachments ECompEditorPageAttachments;
typedef struct _ECompEditorPageAttachmentsClass ECompEditorPageAttachmentsClass;
typedef struct _ECompEditorPageAttachmentsPrivate ECompEditorPageAttachmentsPrivate;

struct _ECompEditorPageAttachments {
	ECompEditorPage parent;

	ECompEditorPageAttachmentsPrivate *priv;
};

struct _ECompEditorPageAttachmentsClass {
	ECompEditorPageClass parent_class;
};

GType		e_comp_editor_page_attachments_get_type	(void) G_GNUC_CONST;
ECompEditorPage *
		e_comp_editor_page_attachments_new	(ECompEditor *editor);
gint		e_comp_editor_page_attachments_get_active_view
							(ECompEditorPageAttachments *page_attachments);
void		e_comp_editor_page_attachments_set_active_view
							(ECompEditorPageAttachments *page_attachments,
							 gint view);
EAttachmentStore *
		e_comp_editor_page_attachments_get_store
							(ECompEditorPageAttachments *page_attachments);

G_END_DECLS

#endif /* E_COMP_EDITOR_PAGE_ATTACHMENTS_H */
