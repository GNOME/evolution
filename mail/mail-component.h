/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-component.h
 *
 * Copyright (C) 2003  Ximian Inc.
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
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jeffrey Stedfast <fejj@ximian.com>
 *   Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _MAIL_COMPONENT_H_
#define _MAIL_COMPONENT_H_

#include <bonobo/bonobo-object.h>

#include "shell/Evolution.h"

struct _CamelStore;

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
	MAIL_COMPONENT_FOLDER_LOCAL_INBOX,
};

struct _MailComponent {
	BonoboObject parent;
	
	MailComponentPrivate *priv;
};

struct _MailComponentClass {
	BonoboObjectClass parent_class;
	
	POA_GNOME_Evolution_Component__epv epv;
};

GType  mail_component_get_type  (void);

MailComponent *mail_component_peek  (void);

/* NOTE: Using NULL as the component implies using the default component */
const char       *mail_component_peek_base_directory    (MailComponent *component);
struct _RuleContext      *mail_component_peek_search_context    (MailComponent *component);
struct _EActivityHandler *mail_component_peek_activity_handler  (MailComponent *component);

void        mail_component_add_store            (MailComponent *component,
						 struct _CamelStore    *store,
						 const char    *name);
struct _CamelStore *mail_component_load_store_by_uri    (MailComponent *component,
						 const char    *uri,
						 const char    *name);

void        mail_component_remove_store         (MailComponent *component,
						 struct _CamelStore    *store);
void        mail_component_remove_store_by_uri  (MailComponent *component,
						 const char    *uri);

int          mail_component_get_store_count  (MailComponent *component);
void         mail_component_stores_foreach   (MailComponent *component,
					      GHFunc         func,
					      void          *data);

void mail_component_remove_folder (MailComponent *component, struct _CamelStore *store, const char *path);

struct _EMFolderTreeModel *mail_component_peek_tree_model (MailComponent *component);

struct _CamelStore *mail_component_peek_local_store (MailComponent *mc);
struct _CamelFolder *mail_component_get_folder(MailComponent *mc, enum _mail_component_folder_t id);
const char *mail_component_get_folder_uri(MailComponent *mc, enum _mail_component_folder_t id);

#endif /* _MAIL_COMPONENT_H_ */
