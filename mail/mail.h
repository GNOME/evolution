/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

/* This file is a F*CKING MESS.  Shame to us!  */

#include <gtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include "camel/camel.h"
#include "composer/e-msg-composer.h"
#include "mail-callbacks.h"
#include "mail-config.h"
#include "mail-config-gui.h"
/*#include "folder-browser.h"*/
#include "mail-session.h"
#include "mail-types.h"
#include "shell/evolution-storage.h"

extern char *evolution_dir;

/* mail-format */
void mail_format_mime_message (CamelMimeMessage *mime_message,
			       MailDisplay *md);

typedef gboolean (*MailMimeHandlerFn) (CamelMimePart *part,
				       const char *mime_type,
				       MailDisplay *md);
typedef struct {
	gboolean generic;
	OAF_ServerInfo *component;
	GnomeVFSMimeApplication *application;
	MailMimeHandlerFn builtin;
} MailMimeHandler;
MailMimeHandler *mail_lookup_handler (const char *mime_type);

gboolean mail_part_is_inline (CamelMimePart *part);

EMsgComposer *mail_generate_reply (CamelMimeMessage *mime_message,
				   gboolean to_all);

char *mail_get_message_body (CamelDataWrapper *data, gboolean want_plain,
			     gboolean *is_html);

/* mail-identify */
char *mail_identify_mime_part (CamelMimePart *part);

/* mail view */
GtkWidget *mail_view_create (CamelFolder *source, const char *uid, CamelMimeMessage *msg);

/* component factory for lack of a better place */
/*takes a GSList of MailConfigServices */
void mail_load_storages (GNOME_Evolution_Shell corba_shell, GSList *sources);
/* used in the subscribe dialog code */
EvolutionStorage *mail_lookup_storage (CamelStore *store);
