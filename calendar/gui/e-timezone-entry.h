/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * ETimezoneEntry - a field for setting a timezone. It shows the timezone in
 * a GtkEntry with a '...' button beside it which shows a dialog for changing
 * the timezone. The dialog contains a map of the world with a point for each
 * timezone, and an option menu as an alternative way of selecting the
 * timezone.
 */

#ifndef __E_TIMEZONE_ENTRY_H_
#define __E_TIMEZONE_ENTRY_H_ 

#include <gtk/gtkhbox.h>
#include <libgnome/gnome-defs.h>
#include <cal-client/cal-client.h>
 
BEGIN_GNOME_DECLS


#define E_TYPE_TIMEZONE_ENTRY            (e_timezone_entry_get_type ())
#define E_TIMEZONE_ENTRY(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_TIMEZONE_ENTRY, ETimezoneEntry))
#define E_TIMEZONE_ENTRY_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TIMEZONE_ENTRY, ETimezoneEntryClass))
#define E_IS_TIMEZONE_ENTRY(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_TIMEZONE_ENTRY))
#define E_IS_TIMEZONE_ENTRY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_TIMEZONE_ENTRY))


typedef struct _ETimezoneEntry        ETimezoneEntry;
typedef struct _ETimezoneEntryPrivate ETimezoneEntryPrivate;
typedef struct _ETimezoneEntryClass   ETimezoneEntryClass;

struct _ETimezoneEntry {
	GtkHBox hbox;

	/*< private >*/
	ETimezoneEntryPrivate *priv;
};

struct _ETimezoneEntryClass {
	GtkHBoxClass parent_class;

	void (* changed)      (ETimezoneEntry    *tentry);
};

guint      e_timezone_entry_get_type		(void);
GtkWidget* e_timezone_entry_new			(void);

char*	   e_timezone_entry_get_timezone	(ETimezoneEntry	*tentry);
void	   e_timezone_entry_set_timezone	(ETimezoneEntry	*tentry,
						 char		*timezone);

END_GNOME_DECLS

#endif /* __E_TIMEZONE_ENTRY_H_ */
