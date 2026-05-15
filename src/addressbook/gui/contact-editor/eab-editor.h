/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Toshok <toshok@ximian.com>
 */

#ifndef __EAB_EDITOR_H__
#define __EAB_EDITOR_H__

#include <gtk/gtk.h>
#include <libebook/libebook.h>
#include <shell/e-shell.h>

/* Standard GObject macros */
#define EAB_TYPE_EDITOR \
	(eab_editor_get_type ())
#define EAB_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EAB_TYPE_EDITOR, EABEditor))
#define EAB_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EAB_TYPE_EDITOR, EABEditorClass))
#define EAB_IS_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EAB_TYPE_EDITOR))
#define EAB_IS_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EAB_TYPE_EDITOR))
#define EAB_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EAB_EDITOR_TYPE, EABEditorClass))

G_BEGIN_DECLS

typedef struct _EABEditor EABEditor;
typedef struct _EABEditorClass EABEditorClass;
typedef struct _EABEditorPrivate EABEditorPrivate;

struct _EABEditor {
	GObject parent;
	EABEditorPrivate *priv;
};

struct _EABEditorClass {
	GObjectClass parent_class;

	/* virtual functions */
	void (* show)             (EABEditor *editor);
	void (* close)		  (EABEditor *editor);
	void (* raise)		  (EABEditor *editor);
	void (* save_contact)     (EABEditor *editor, gboolean should_close);
	gboolean (* is_valid)     (EABEditor *editor);
	gboolean (* is_changed)   (EABEditor *editor);
	GtkWindow * (* get_window) (EABEditor *editor);

	/* signals */
	void (* editor_closed)    (EABEditor *editor);
};

GType		eab_editor_get_type		(void);
EShell *	eab_editor_get_shell		(EABEditor *editor);

/* virtual functions */
void		eab_editor_show			(EABEditor *editor);
void		eab_editor_close		(EABEditor *editor);
void		eab_editor_raise		(EABEditor *editor);
void		eab_editor_save_contact		(EABEditor *editor,
						 gboolean should_close);
gboolean	eab_editor_is_valid		(EABEditor *editor);
gboolean	eab_editor_is_changed		(EABEditor *editor);
GtkWindow *	eab_editor_get_window		(EABEditor *editor);
void		eab_editor_maybe_report_error	(EABEditor *editor,
						 const gchar *message,
						 const GError *error);

gboolean	eab_editor_prompt_to_save_changes
						(EABEditor *editor,
						 GtkWindow *window);

void		eab_editor_emit_closed		(EABEditor *editor);

G_END_DECLS

#endif /* __E_CONTACT_EDITOR_H__ */
