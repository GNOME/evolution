/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _RULE_EDITOR_H
#define _RULE_EDITOR_H

#include <gtk/gtk.h>
#include <libgnomeui/gnome-dialog.h>

#define RULE_EDITOR(obj)	GTK_CHECK_CAST (obj, rule_editor_get_type (), RuleEditor)
#define RULE_EDITOR_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, rule_editor_get_type (), RuleEditorClass)
#define IS_RULE_EDITOR(obj)      GTK_CHECK_TYPE (obj, rule_editor_get_type ())

typedef struct _RuleEditor	RuleEditor;
typedef struct _RuleEditorClass	RuleEditorClass;

struct _RuleEditor {
	GnomeDialog parent;

	GtkList *list;
	struct _RuleContext *context;
	struct _FilterRule *current;
	struct _FilterRule *edit;	/* for editing/adding rules, so we only do 1 at a time */

	char *source;

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

