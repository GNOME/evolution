/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 1999, Helix Code, Inc.
 */

#ifndef __E_CARD_H__
#define __E_CARD_H__

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <ebook/e-card-fields.h>

BEGIN_GNOME_DECLS

typedef struct _ECardPrivate ECardPrivate;

typedef struct {
	GtkObject     parent;
	ECardPrivate *priv;
} ECard;

typedef struct {
	GtkObjectClass parent;
} ECardClass;

ECard    *e_card_new          (void);
GtkType   e_card_get_type     (void);

char     *e_card_get_string   (ECard    *card,
			       char     *field);
void      e_card_set_string   (ECard    *card,
			       char     *field,
			       char     *value);

gboolean  e_card_get_boolean  (ECard    *card,
			       char     *field);
void      e_card_set_boolean  (ECard    *card,
			       char     *field,
			       gboolean  value);

#define E_CARD_TYPE        (e_card_get_type ())
#define E_CARD(o)          (GTK_CHECK_CAST ((o), E_CARD_TYPE, ECard))
#define E_CARD_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_CARD_TYPE, ECardClass))
#define E_IS_CARD(o)       (GTK_CHECK_TYPE ((o), E_CARD_TYPE))
#define E_IS_CARD_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_CARD_TYPE))

END_GNOME_DECLS

#endif /* ! __E_CARD_H__ */
