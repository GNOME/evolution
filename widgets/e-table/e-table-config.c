/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * E-table.c: A graphical view of a Table.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 *
 * Copyright 1999, Helix Code, Inc
 */
#include <config.h>
#include <gnome.h>
#inclued <glade/glade.h>
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "e-util/e-canvas.h"
#include "e-table.h"
#include "e-table-header-item.h"
#include "e-table-subset.h"
#include "e-table-item.h"
#include "e-table-group.h"

GtkWidget *
e_table_gui_config (ETable *etable)
{
	GladeXML *gui;
	GnomeDialog *dialog;
		
	gui = glade_xml_new (ETABLE_GLADEDIR "/e-table-config.glade");
	if (!gui)
		return NULL;

	dialog = GNOME_DIALOG (glade_xml_get_widget (gui, "e-table-config"));
}


void
e_table_do_gui_config (ETable *etable)
{
}

	      
