/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SPELL_TEXT_VIEW_H
#define E_SPELL_TEXT_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean	e_spell_text_view_is_supported	(void);
void		e_spell_text_view_attach	(GtkTextView *text_view);
gboolean	e_spell_text_view_get_enabled	(GtkTextView *text_view);
void		e_spell_text_view_set_enabled	(GtkTextView *text_view,
						 gboolean enabled);
void		e_spell_text_view_set_languages	(GtkTextView *text_view,
						 const gchar **languages);

G_END_DECLS

#endif /* E_SPELL_TEXT_VIEW_H */
