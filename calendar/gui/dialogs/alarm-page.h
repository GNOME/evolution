/* Evolution calendar - Alarm page of the calendar component dialogs
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

#ifndef ALARM_PAGE_H
#define ALARM_PAGE_H

#include "comp-editor-page.h"

BEGIN_GNOME_DECLS



#define TYPE_ALARM_PAGE            (alarm_page_get_type ())
#define ALARM_PAGE(obj)            (GTK_CHECK_CAST ((obj), TYPE_ALARM_PAGE, AlarmPage))
#define ALARM_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TYPE_ALARM_PAGE, AlarmPageClass))
#define IS_ALARM_PAGE(obj)         (GTK_CHECK_TYPE ((obj), TYPE_ALARM_PAGE))
#define IS_ALARM_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), TYPE_ALARM_PAGE))

typedef struct _AlarmPagePrivate AlarmPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	AlarmPagePrivate *priv;
} AlarmPage;

typedef struct {
	CompEditorPageClass parent_class;
} AlarmPageClass;


GtkType    alarm_page_get_type  (void);
AlarmPage *alarm_page_construct (AlarmPage *apage);
AlarmPage *alarm_page_new       (void);



END_GNOME_DECLS

#endif
