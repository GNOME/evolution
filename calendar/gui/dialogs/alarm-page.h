/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

G_BEGIN_DECLS



#define TYPE_ALARM_PAGE            (alarm_page_get_type ())
#define ALARM_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ALARM_PAGE, AlarmPage))
#define ALARM_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_ALARM_PAGE, AlarmPageClass))
#define IS_ALARM_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ALARM_PAGE))
#define IS_ALARM_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_ALARM_PAGE))

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



G_END_DECLS

#endif
