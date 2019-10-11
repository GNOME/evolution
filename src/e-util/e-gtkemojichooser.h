/*
 * Copyright 2017, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* Copied and adapted a bit from gtk+'s gtkemojichooser.h,
   waiting for it to be made public:
   https://gitlab.gnome.org/GNOME/gtk/issues/86
*/

#ifndef E_GTKEMOJICHOOSER_H
#define E_GTKEMOJICHOOSER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_GTK_TYPE_EMOJI_CHOOSER                 (e_gtk_emoji_chooser_get_type ())
#define E_GTK_EMOJI_CHOOSER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_GTK_TYPE_EMOJI_CHOOSER, EGtkEmojiChooser))
#define E_GTK_EMOJI_CHOOSER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), E_GTK_TYPE_EMOJI_CHOOSER, EGtkEmojiChooserClass))
#define E_GTK_IS_EMOJI_CHOOSER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_GTK_TYPE_EMOJI_CHOOSER))
#define E_GTK_IS_EMOJI_CHOOSER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), E_GTK_TYPE_EMOJI_CHOOSER))
#define E_GTK_EMOJI_CHOOSER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), E_GTK_TYPE_EMOJI_CHOOSER, EGtkEmojiChooserClass))

typedef struct _EGtkEmojiChooser      EGtkEmojiChooser;
typedef struct _EGtkEmojiChooserClass EGtkEmojiChooserClass;

GType		e_gtk_emoji_chooser_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_gtk_emoji_chooser_new		(void);

G_END_DECLS

#endif /* E_GTKEMOJICHOOSER_H */
