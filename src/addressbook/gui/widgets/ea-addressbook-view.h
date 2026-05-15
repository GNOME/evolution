/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Leon Zhang <leon.zhang@sun.com>
 */

#ifndef __EA_ADDRESSBOOK_VIEW_H__
#define __EA_ADDRESSBOOK_VIEW_H__

#include <gtk/gtk.h>
#include "e-addressbook-view.h"

G_BEGIN_DECLS

#define EA_TYPE_AB_VIEW			(ea_ab_view_get_type ())
#define EA_AB_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_AB_VIEW, EaABView))
#define EA_AB_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_AB_VIEW, EaABViewClass))
#define EA_IS_AB_VIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_AB_VIEW))
#define EA_IS_AB_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EA_TYPE_AB_VIEW))

typedef struct _EaABView       EaABView;
typedef struct _EaABViewClass  EaABViewClass;

struct _EaABView
{
	GtkAccessible parent;
};

struct _EaABViewClass
{
	GtkAccessibleClass parent_class;
};

GType ea_ab_view_get_type (void);
AtkObject * ea_ab_view_new (GObject *obj);

G_END_DECLS

#endif /* __EA_ADDRESSBOOK_VIEW_H__ */
