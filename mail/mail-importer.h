/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-importer.h
 *
 * Authors: Iain Holmes <iain@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef __MAIL_IMPORTER_H__
#define __MAIL_IMPORTER_H__

#include <bonobo/bonobo-listener.h>
#include <camel/camel-folder.h>
#include <camel/camel-stream-mem.h>
#include <evolution-shell-client.h>

typedef struct _MailImporter MailImporter;
struct _MailImporter {
	CamelFolder *folder;
	CamelStreamMem *mstream;

	gboolean frozen; /* Is folder frozen? */
};

void mail_importer_init (EvolutionShellClient *client);
void mail_importer_uninit (void);

void mail_importer_add_line (MailImporter *importer,
			     const char *str,
			     gboolean finished);
void mail_importer_create_folder (const char *parent_path,
				  const char *name,
				  const char *description,
				  const BonoboListener *listener);
#endif
