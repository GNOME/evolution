/* Evolution calendar - Recurrence page of the calendar component dialogs
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef RECURRENCE_PAGE_H
#define RECURRENCE_PAGE_H

#include "comp-editor-page.h"

BEGIN_GNOME_DECLS



#define TYPE_RECURRENCE_PAGE            (recurrence_page_get_type ())
#define RECURRENCE_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_RECURRENCE_PAGE, RecurrencePage))
#define RECURRENCE_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_RECURRENCE_PAGE, RecurrencePageClass))
#define IS_RECURRENCE_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_RECURRENCE_PAGE))
#define IS_RECURRENCE_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_RECURRENCE_PAGE))

typedef struct _RecurrencePagePrivate RecurrencePagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	RecurrencePagePrivate *priv;
} RecurrencePage;

typedef struct {
	CompEditorPageClass parent_class;
} RecurrencePageClass;


GtkType         recurrence_page_get_type  (void);
RecurrencePage *recurrence_page_construct (RecurrencePage *rpage);
RecurrencePage *recurrence_page_new       (void);



END_GNOME_DECLS

#endif
