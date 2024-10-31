/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_ESTIMATED_DURATION_ENTRY_H
#define E_ESTIMATED_DURATION_ENTRY_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

/* Standard GObject macros */
#define E_TYPE_ESTIMATED_DURATION_ENTRY \
	(e_estimated_duration_entry_get_type ())
#define E_ESTIMATED_DURATION_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ESTIMATED_DURATION_ENTRY, EEstimatedDurationEntry))
#define E_ESTIMATED_DURATION_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ESTIMATED_DURATION_ENTRY, EEstimatedDurationEntryClass))
#define E_IS_ESTIMATED_DURATION_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ESTIMATED_DURATION_ENTRY))
#define E_IS_ESTIMATED_DURATION_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ESTIMATED_DURATION_ENTRY))
#define E_IS_ESTIMATED_DURATION_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ESTIMATED_DURATION_ENTRY, EEstimatedDurationEntryClass))

G_BEGIN_DECLS

typedef struct _EEstimatedDurationEntry EEstimatedDurationEntry;
typedef struct _EEstimatedDurationEntryClass EEstimatedDurationEntryClass;
typedef struct _EEstimatedDurationEntryPrivate EEstimatedDurationEntryPrivate;

struct _EEstimatedDurationEntry {
	GtkBox parent;
	EEstimatedDurationEntryPrivate *priv;
};

struct _EEstimatedDurationEntryClass {
	GtkBoxClass parent_class;

	void		(*changed)		(EEstimatedDurationEntry *estimated_duration_entry);
};

GType		e_estimated_duration_entry_get_type	(void);
GtkWidget *	e_estimated_duration_entry_new		(void);
ICalDuration *	e_estimated_duration_entry_get_value	(EEstimatedDurationEntry *self);
void		e_estimated_duration_entry_set_value	(EEstimatedDurationEntry *self,
							 const ICalDuration *value);

G_END_DECLS

#endif /* E_ESTIMATED_DURATION_ENTRY_H */
