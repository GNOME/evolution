/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CELL_ESTIMATED_DURATION_H
#define E_CELL_ESTIMATED_DURATION_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CELL_ESTIMATED_DURATION \
	(e_cell_estimated_duration_get_type ())
#define E_CELL_ESTIMATED_DURATION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CELL_ESTIMATED_DURATION, ECellEstimatedDuration))
#define E_CELL_ESTIMATED_DURATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CELL_ESTIMATED_DURATION, ECellEstimatedDurationClass))
#define E_IS_CELL_ESTIMATED_DURATION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CELL_ESTIMATED_DURATION))
#define E_IS_CELL_ESTIMATED_DURATION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CELL_ESTIMATED_DURATION))
#define E_CELL_ESTIMATED_DURATION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CELL_ESTIMATED_DURATION, ECellEstimatedDurationClass))

G_BEGIN_DECLS

typedef struct _ECellEstimatedDuration ECellEstimatedDuration;
typedef struct _ECellEstimatedDurationClass ECellEstimatedDurationClass;

struct _ECellEstimatedDuration {
	ECellText parent;
};

struct _ECellEstimatedDurationClass {
	ECellTextClass parent_class;
};

GType		e_cell_estimated_duration_get_type	(void) G_GNUC_CONST;
ECell *		e_cell_estimated_duration_new		(const gchar *fontname,
							 GtkJustification justify);

G_END_DECLS

#endif /* E_CELL_ESTIMATED_DURATION_H */
