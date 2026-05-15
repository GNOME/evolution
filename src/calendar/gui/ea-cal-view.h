/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Bolian Yin <bolian.yin@sun.com>
 */

#ifndef __EA_CAL_VIEW_H__
#define __EA_CAL_VIEW_H__

#include <gtk/gtk.h>
#include "e-calendar-view.h"
#include "gtk/gtk-a11y.h"

G_BEGIN_DECLS

#define EA_TYPE_CAL_VIEW                     (ea_cal_view_get_type ())
#define EA_CAL_VIEW(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_CAL_VIEW, EaCalView))
#define EA_CAL_VIEW_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_CAL_VIEW, EaCalViewClass))
#define EA_IS_CAL_VIEW(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_CAL_VIEW))
#define EA_IS_CAL_VIEW_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_CAL_VIEW))
#define EA_CAL_VIEW_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_CAL_VIEW, EaCalViewClass))

typedef struct _EaCalView                   EaCalView;
typedef struct _EaCalViewClass              EaCalViewClass;

struct _EaCalView
{
	GtkContainerAccessible parent;
};

GType ea_cal_view_get_type (void);

struct _EaCalViewClass
{
	GtkContainerAccessibleClass parent_class;
};

AtkObject * ea_cal_view_new (GtkWidget *widget);

G_END_DECLS

#endif /* __EA_CAL_VIEW_H__ */
