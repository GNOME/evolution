/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#ifndef _E_EXPANDER_H_
#define _E_EXPANDER_H_

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_EXPANDER            (e_expander_get_type ())
#define E_EXPANDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EXPANDER, EExpander))
#define E_EXPANDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_EXPANDER, EExpanderClass))
#define GTK_IS_EXPANDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EXPANDER))
#define GTK_IS_EXPANDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_EXPANDER))
#define E_EXPANDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EXPANDER, EExpanderClass))
/* ESTUFF #define E_EXPANDER_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_EXPANDER, EExpanderPrivate)) */

typedef struct _EExpander      EExpander;
typedef struct _EExpanderClass EExpanderClass;

struct _EExpander
{
  GtkBin bin;
};

struct _EExpanderClass
{
  GtkBinClass parent_class;

  void (* activate) (EExpander *expander);
};

GType                 e_expander_get_type          (void);

GtkWidget            *e_expander_new               (const gchar *label);
GtkWidget            *e_expander_new_with_mnemonic (const gchar *label);

void                  e_expander_set_expanded      (EExpander   *expander,
                                                    gboolean     expanded);
gboolean              e_expander_get_expanded      (EExpander   *expander);

/* Spacing between the expander/label and the child */
void                  e_expander_set_spacing       (EExpander   *expander,
						      gint       spacing);
gint                  e_expander_get_spacing       (EExpander   *expander);

void                  e_expander_set_label         (EExpander   *expander,
                                                    const gchar *label);
G_CONST_RETURN gchar *e_expander_get_label         (EExpander   *expander);

void                  e_expander_set_use_underline (EExpander   *expander,
                                                    gboolean     use_underline);
gboolean              e_expander_get_use_underline (EExpander   *expander);

void                  e_expander_set_label_widget  (EExpander   *expander,
                                                    GtkWidget   *label_widget);
GtkWidget            *e_expander_get_label_widget  (EExpander   *expander);

G_END_DECLS

#endif /* _E_EXPANDER_H_ */
