/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _RULE_EDITOR_H
#define _RULE_EDITOR_H

#include <gtk/gtklist.h>
#include <libgnomeui/gnome-dialog.h>

#define RULE_EDITOR(obj)	GTK_CHECK_CAST (obj, rule_editor_get_type (), RuleEditor)
#define RULE_EDITOR_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, rule_editor_get_type (), RuleEditorClass)
#define IS_RULE_EDITOR(obj)      GTK_CHECK_TYPE (obj, rule_editor_get_type ())

typedef struct _RuleEditor	RuleEditor;
typedef struct _RuleEditorClass	RuleEditorClass;
typedef struct _RuleEditorUndo	RuleEditorUndo;

struct _RuleEditor {
	GnomeDialog parent;

	GtkList *list;
	struct _RuleContext *context;
	struct _FilterRule *current;
	struct _FilterRule *edit;	/* for editing/adding rules, so we only do 1 at a time */
	
	GtkWidget *dialog;
	
	char *source;

	struct _RuleEditorUndo *undo_log;	/* cancel/undo log */
	unsigned int undo_active:1; /* we're performing undo */

	struct _RuleEditorPrivate *priv;
};

struct _RuleEditorClass {
	GnomeDialogClass parent_class;

	/* virtual methods */
	void (*set_sensitive)(RuleEditor *);
	void (*set_source)(RuleEditor *, const char *source);

	struct _FilterRule *(*create_rule)(RuleEditor *);

	/* signals */
};

enum {
	RULE_EDITOR_LOG_EDIT,
	RULE_EDITOR_LOG_ADD,
	RULE_EDITOR_LOG_REMOVE,
	RULE_EDITOR_LOG_RANK,
};

struct _RuleEditorUndo {
	struct _RuleEditorUndo *next;

	unsigned int type;
	struct _FilterRule *rule;
	int rank;
	int newrank;
};

struct _GladeXML;
struct _RuleContext;

guint		rule_editor_get_type	(void);
RuleEditor	*rule_editor_new	(struct _RuleContext *, const char *source);
void		rule_editor_construct   (RuleEditor *re, struct _RuleContext *context, struct _GladeXML *gui, const char *source);

/* methods */
void rule_editor_set_source(RuleEditor *re, const char *source);
/* calculates the sensitivity of the editor */
void rule_editor_set_sensitive(RuleEditor *re);
/* used internally to create a new rule appropriate for the editor */
struct _FilterRule *rule_editor_create_rule(RuleEditor *re);

#endif /* ! _RULE_EDITOR_H */

