/*
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
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *		Jeffrey Stedfast <fejj@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MAIL_COMPONENT_H_
#define _MAIL_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include "shell/evolution-component.h"
#include "mail/Evolution-Mail.h"
#include "mail/em-folder-tree-model.h"
#include "filter/rule-context.h"
#include "misc/e-activity-handler.h"

#define MAIL_TYPE_COMPONENT			(mail_component_get_type ())
#define MAIL_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), MAIL_TYPE_COMPONENT, MailComponent))
#define MAIL_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), MAIL_TYPE_COMPONENT, MailComponentClass))
#define MAIL_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAIL_TYPE_COMPONENT))
#define MAIL_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), MAIL_TYPE_COMPONENT))

typedef struct _MailComponent        MailComponent;
typedef struct _MailComponentPrivate MailComponentPrivate;
typedef struct _MailComponentClass   MailComponentClass;

enum _mail_component_folder_t {
	MAIL_COMPONENT_FOLDER_INBOX = 0,
	MAIL_COMPONENT_FOLDER_DRAFTS,
	MAIL_COMPONENT_FOLDER_OUTBOX,
	MAIL_COMPONENT_FOLDER_SENT,
	MAIL_COMPONENT_FOLDER_TEMPLATES,
	MAIL_COMPONENT_FOLDER_LOCAL_INBOX
};

struct _MailComponent {
	EvolutionComponent parent;

	MailComponentPrivate *priv;
};

struct _MailComponentClass {
	EvolutionComponentClass parent_class;

	POA_GNOME_Evolution_MailComponent__epv epv;
};

GType  mail_component_get_type  (void);

MailComponent *mail_component_peek  (void);

/* NOTE: Using NULL as the component implies using the default component */
const gchar       *mail_component_peek_base_directory    (MailComponent *component);
RuleContext      *mail_component_peek_search_context    (MailComponent *component);
EActivityHandler *mail_component_peek_activity_handler  (MailComponent *component);

CamelSession *mail_component_peek_session(MailComponent *);

void        mail_component_add_store            (MailComponent *component,
						 CamelStore    *store,
						 const gchar    *name);
CamelStore *mail_component_load_store_by_uri    (MailComponent *component,
						 const gchar    *uri,
						 const gchar    *name);

void        mail_component_remove_store         (MailComponent *component,
						 CamelStore    *store);
void        mail_component_remove_store_by_uri  (MailComponent *component,
						 const gchar    *uri);

gint          mail_component_get_store_count  (MailComponent *component);
void         mail_component_stores_foreach   (MailComponent *component,
					      GHFunc         func,
					      void          *data);

void mail_component_remove_folder (MailComponent *component, CamelStore *store, const gchar *path);

EMFolderTreeModel *mail_component_peek_tree_model (MailComponent *component);

CamelStore *mail_component_peek_local_store (MailComponent *mc);
CamelFolder *mail_component_get_folder(MailComponent *mc, enum _mail_component_folder_t id);
const gchar *mail_component_get_folder_uri(MailComponent *mc, enum _mail_component_folder_t id);

gint status_check (GNOME_Evolution_ShellState shell_state);

void mail_indicate_new_mail (gboolean have_new_mail);
void mail_component_show_logger (gpointer);
void mail_component_show_status_bar (gboolean show);

#endif /* _MAIL_COMPONENT_H_ */
