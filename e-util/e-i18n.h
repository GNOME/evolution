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

#include <bonobo/bonobo-i18n.h>

G_BEGIN_DECLS

#ifdef ENABLE_NLS
	/* this function is defined in e-util.c */
	extern char *e_gettext (const char *msgid);
#    undef _
#    define _(String)  e_gettext (String)
#endif

G_END_DECLS

#endif /* __E_I18N_H__ */
