/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_URL_ENTRY_H
#define E_URL_ENTRY_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_URL_ENTRY \
	(e_url_entry_get_type ())
#define E_URL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_URL_ENTRY, EUrlEntry))
#define E_URL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_URL_ENTRY, EUrlEntryClass))
#define E_IS_URL_ENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_URL_ENTRY))
#define E_IS_URL_ENTRY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_URL_ENTRY))
#define E_URL_ENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_URL_ENTRY, EUrlEntryClass))

G_BEGIN_DECLS

typedef struct _EUrlEntry EUrlEntry;
typedef struct _EUrlEntryClass EUrlEntryClass;
typedef struct _EUrlEntryPrivate EUrlEntryPrivate;

struct _EUrlEntry {
	GtkEntry parent;
	EUrlEntryPrivate *priv;
};

struct _EUrlEntryClass {
	GtkEntryClass parent_class;

	/* gboolean	(*open_url)	(EUrlEntry *entry,
					 GtkWindow *parent_window,
					 const gchar *url); */
};

GType		e_url_entry_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_url_entry_new			(void);
void		e_url_entry_set_icon_visible	(EUrlEntry *url_entry,
						 gboolean visible);
gboolean	e_url_entry_get_icon_visible	(EUrlEntry *url_entry);

G_END_DECLS

#endif /* E_URL_ENTRY_H */
