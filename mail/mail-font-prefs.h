/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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
#ifndef __MAIL_FONT_PREFS_H__
#define __MAIL_FONT_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml-propmanager.h>

#include <shell/Evolution.h>
#include "evolution-config-control.h"

#define MAIL_FONT_PREFS_TYPE          (mail_font_prefs_get_type())
#define MAIL_FONT_PREFS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_FONT_PREFS_TYPE, MailFontPrefs))
#define MAIL_FONT_PREFS_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), MAIL_FONT_PREFS_TYPE, MailFontPrefsClass))
#define IS_MAIL_FONT_PREFS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_FONT_PREFS_TYPE))
#define IS_MAIL_FONT_PREFS_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_FONT_PREFS_TYPE))

typedef struct _MailFontPrefs MailFontPrefs;
typedef struct _MailFontPrefsClass MailFontPrefsClass;

struct _MailFontPrefs {
	GtkVBox parent_object;

	GtkHTMLPropmanager *pman;
	GladeXML *gui;
	EvolutionConfigControl *control;
};
	
struct _MailFontPrefsClass {
	GtkVBoxClass parent_object;
};

GtkType      mail_font_prefs_get_type (void);
GtkWidget *  mail_font_prefs_new (void);
void         mail_font_prefs_apply (MailFontPrefs *prefs);

#define MAIL_FONT_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_FontPrefs_ConfigControl:" BASE_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __MAIL_FONT_PREFS_H__ */
