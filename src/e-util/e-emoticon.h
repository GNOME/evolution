/*
 * e-emoticon.h
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EMOTICON_H
#define E_EMOTICON_H

#include <glib-object.h>

#define E_TYPE_EMOTICON \
	(e_emoticon_get_type ())

G_BEGIN_DECLS

typedef struct _EEmoticon EEmoticon;

struct _EEmoticon {
	gchar *label;
	gchar *icon_name;
	gchar *unicode_character;
	gchar *text_face;
};

GType		e_emoticon_get_type		(void) G_GNUC_CONST;
gboolean	e_emoticon_equal		(const EEmoticon *emoticon_a,
						 const EEmoticon *emoticon_b);
EEmoticon *	e_emoticon_copy			(const EEmoticon *emoticon);
void		e_emoticon_free			(EEmoticon *emoticon);
gchar *		e_emoticon_dup_uri		(const EEmoticon *emoticon);
const gchar *	e_emoticon_get_name		(const EEmoticon *emoticon);

G_END_DECLS

#endif /* E_EMOTICON_H */
