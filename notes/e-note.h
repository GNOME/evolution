/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#ifndef __E_NOTE_H__
#define __E_NOTE_H__

#include <config.h>
#include <gnome.h>
#include "e-bevel-button.h"

#define E_TYPE_NOTE            (e_note_get_type ())
#define E_NOTE(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_NOTE, ENote))
#define E_NOTE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_NOTE, ENoteClass))
#define E_IS_NOTE(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_NOTE))
#define E_IS_NOTE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_NOTE))

typedef struct _ENote ENote;
typedef struct _ENotePrivate ENotePrivate;
typedef struct _ENoteClass ENoteClass;

struct _ENote {
	GtkWindow parent;

	ENotePrivate *priv;
};

struct _ENoteClass {
	GtkWindowClass parent_class;

	void (* text_changed) (ENote *note);
};

GtkType e_note_get_type (void);
GtkWidget *e_note_new (void);
void e_note_set_text (ENote *note, gchar *text);
gchar *e_note_get_text (ENote *note);

#endif /* __E_NOTE_H__ */
