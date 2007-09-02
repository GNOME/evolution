/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 * Copyright 2004 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <gtkhtml/gtkhtml.h>
#include "mail/em-format-html-display.h"
#include "mail/em-format-html-print.h"
#include <glib.h>
#include <glib/gi18n.h>
#include "mail/em-menu.h"
#include "mail/em-utils.h"
#include "e-util/e-print.h"
#include "e-util/e-dialog-utils.h"
#include "composer/e-msg-composer.h"

void org_gnome_compose_print_message (EPlugin *ep, EMMenuTargetWidget *t);
void org_gnome_print_message (EPlugin *ep, EMMenuTargetWidget *t);
void org_gnome_print_preview (EPlugin *ep, EMMenuTargetWidget *t);

void 
org_gnome_print_message (EPlugin *ep, EMMenuTargetWidget *t)
{
       	EMsgComposer *composer = (EMsgComposer *)t->target.widget;
	GtkPrintOperationAction action;
	CamelMimeMessage *message;
	EMFormatHTMLPrint *efhp;

	action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	message = e_msg_composer_get_message (composer, 1);

	efhp = em_format_html_print_new (NULL, action);
	em_format_html_print_raw_message (efhp, message);
	g_object_unref (efhp);
}

void
org_gnome_print_preview (EPlugin *ep, EMMenuTargetWidget *t)
{
  	EMsgComposer *composer = (EMsgComposer *)t->target.widget;
	GtkPrintOperationAction action;
	CamelMimeMessage *message;
	EMFormatHTMLPrint *efhp;

	action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	message = e_msg_composer_get_message_print (composer, 1);

	efhp = em_format_html_print_new (NULL, action);
	em_format_html_print_raw_message (efhp, message);
	g_object_unref (efhp);
}
