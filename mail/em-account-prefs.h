/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
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
 *
 */


#ifndef __EM_ACCOUNT_PREFS_H__
#define __EM_ACCOUNT_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkclist.h>
#include <glade/glade.h>
#include <gtk/gtktreeview.h>

#include <gal/e-table/e-table.h>

#include "evolution-config-control.h"

#include <shell/Evolution.h>


#define EM_ACCOUNT_PREFS_TYPE        (em_account_prefs_get_type ())
#define EM_ACCOUNT_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_ACCOUNT_PREFS_TYPE, EMAccountPrefs))
#define EM_ACCOUNT_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_ACCOUNT_PREFS_TYPE, EMAccountPrefsClass))
#define EM_IS_ACCOUNT_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_ACCOUNT_PREFS_TYPE))
#define EM_IS_ACCOUNT_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_ACCOUNT_PREFS_TYPE))

typedef struct _EMAccountPrefs EMAccountPrefs;
typedef struct _EMAccountPrefsClass EMAccountPrefsClass;

struct _EMAccountPrefs {
	GtkVBox parent_object;
	
	GNOME_Evolution_Shell shell;
	
	GladeXML *gui;
	
	GtkWidget *druid;
	GtkWidget *editor;
	
	GtkTreeView *table;
	
	GtkButton *mail_add;
	GtkButton *mail_edit;
	GtkButton *mail_delete;
	GtkButton *mail_default;
	GtkButton *mail_able;
	
	guint destroyed : 1;
};

struct _EMAccountPrefsClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};


GtkType em_account_prefs_get_type (void);

GtkWidget *em_account_prefs_new (GNOME_Evolution_Shell shell);

/* needed by global config */
#define EM_ACCOUNT_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_AccountPrefs_ConfigControl:" BASE_VERSION

#ifdef __cplusplus
}
#endif

#endif /* __EM_ACCOUNT_PREFS_H__ */
