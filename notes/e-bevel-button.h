/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
#ifndef __E_BEVEL_BUTTON_H__
#define __E_BEVEL_BUTTON_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkbutton.h>

#define E_TYPE_BEVEL_BUTTON            (e_bevel_button_get_type ())
#define E_BEVEL_BUTTON(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_BEVEL_BUTTON, EBevelButton))
#define E_BEVEL_BUTTON_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_BEVEL_BUTTON, EBevelButtonClass))
#define E_IS_BEVEL_BUTTON(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_BEVEL_BUTTON))
#define E_IS_BEVEL_BUTTON_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_BEVEL_BUTTON))

typedef struct _EBevelButton        EBevelButton;
typedef struct _EBevelButtonPrivate EBevelButtonPrivate;
typedef struct _EBevelButtonClass   EBevelButtonClass;

struct _EBevelButton {
	GtkButton parent;

	EBevelButtonPrivate *priv;
};

struct _EBevelButtonClass {
	GtkButtonClass parent_class;
};

GtkType e_bevel_button_get_type (void);

GtkWidget *e_bevel_button_new (void);
void e_bevel_button_set_base_color (EBevelButton *button, GdkColor *color);

#endif /* __E_BEVEL_BUTTON_H__ */
				  
