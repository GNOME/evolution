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

#ifndef _MAIL_SEARCH_DIALOGUE_H
#define _MAIL_SEARCH_DIALOGUE_H

#include <gtk/gtkwidget.h>
#include <libgnomeui/gnome-dialog.h>

#include "filter/rule-context.h"
#include "filter/filter-rule.h"

#define MAIL_SEARCH_DIALOGUE(obj)	GTK_CHECK_CAST (obj, mail_search_dialogue_get_type (), MailSearchDialogue)
#define MAIL_SEARCH_DIALOGUE_CLASS(klass)	GTK_CHECK_CLASS_CAST (klass, mail_search_dialogue_get_type (), MailSearchDialogueClass)
#define IS_MAIL_SEARCH_DIALOGUE(obj)      GTK_CHECK_TYPE (obj, mail_search_dialogue_get_type ())

typedef struct _MailSearchDialogue	MailSearchDialogue;
typedef struct _MailSearchDialogueClass	MailSearchDialogueClass;

struct _MailSearchDialogue {
	GnomeDialog parent;

	RuleContext *context;
	FilterRule *rule;
	GtkWidget *guts;
};

struct _MailSearchDialogueClass {
	GnomeDialogClass parent_class;

	/* virtual methods */

	/* signals */
};

guint		mail_search_dialogue_get_type	(void);
MailSearchDialogue	*mail_search_dialogue_new	(void);
MailSearchDialogue	*mail_search_dialogue_new_with_rule(FilterRule *rule);

/* methods */
char *mail_search_dialogue_get_query(MailSearchDialogue *msd);

#endif /* ! _MAIL_SEARCH_DIALOGUE_H */

