/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-i18n.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * Copied from gnome-i18nP.h, because this header is typically not installed
 *
 * This file has to be included before any file from the GNOME libraries
 * to have this override the definitions that are pulled from the gnome-i18n.h
 *
 * the difference is that gnome-i18n.h is used for applications, and this is
 * used by libraries (because libraries have to use dcgettext instead of
 * gettext and they need to provide the translation domain, unlike apps).
 *
 * So you can just put this after you include config.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __E_I18N_H__
#define __E_I18N_H__

#include <glib.h>
#include <libgnome/gnome-defs.h>

BEGIN_GNOME_DECLS

#ifdef ENABLE_NLS
#    include <libintl.h>
#    undef _
#    ifdef GNOME_EXPLICIT_TRANSLATION_DOMAIN
#        define _(String) dgettext (GNOME_EXPLICIT_TRANSLATION_DOMAIN, String)
#    else
#        define _(String) dgettext (PACKAGE, String)
#    endif
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

/*
 * Do not remove the following define, nor do surround it with ifdefs.
 *
 * If you get any `redefined' errors, it means that you are including
 * -incorrectly- a header file provided by gnome-libs before this file.
 * To correctly solve this issue include this file before any libgnome/
 * libgnomeui headers
 */

#define __GNOME_I18N_H__ 1


/* This is copied staight out of the prototypes for gnome-i18n.h */
const char *gnome_i18n_get_language(void);
GList      *gnome_i18n_get_language_list (const gchar *category_name);
void	   gnome_i18n_set_preferred_language (const char *val);
const char *gnome_i18n_get_preferred_language (void);
void gnome_i18n_init (void);

END_GNOME_DECLS

#endif /* __E_I18N_H__ */
