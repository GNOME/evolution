/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-print.c - Uniform print setting/dialog routines for Evolution
 *
 * Copyright (C) 2005 Novell, Inc.
 *
 * Authors: JP Rosevear <jpr@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#ifndef __E_PRINT__
#define __E_PRINT__

G_BEGIN_DECLS

GnomePrintConfig *e_print_load_config (void);
void e_print_save_config (GnomePrintConfig *config);

GtkWidget *e_print_get_dialog (const char *title, int flags);
GtkWidget *e_print_get_dialog_with_config (const char *title, int flags, GnomePrintConfig *config);

G_END_DECLS

#endif 
