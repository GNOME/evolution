/* Evolution calendar - Main page of the task editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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

#ifndef MEETING_PAGE_H
#define MEETING_PAGE_H

#include "../e-meeting-store.h"
#include "../itip-utils.h"
#include "comp-editor-page.h"

G_BEGIN_DECLS



#define TYPE_MEETING_PAGE            (meeting_page_get_type ())
#define MEETING_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MEETING_PAGE, MeetingPage))
#define MEETING_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MEETING_PAGE, MeetingPageClass))
#define IS_MEETING_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MEETING_PAGE))
#define IS_MEETING_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_MEETING_PAGE))

typedef struct _MeetingPagePrivate MeetingPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	MeetingPagePrivate *priv;
} MeetingPage;

typedef struct {
	CompEditorPageClass parent_class;
} MeetingPageClass;


GtkType       meeting_page_get_type          (void);
MeetingPage  *meeting_page_construct         (MeetingPage   *mpage,
					      EMeetingStore *ems,
					      ECal     *client);
MeetingPage  *meeting_page_new               (EMeetingStore *ems,
					      ECal     *client);
ECalComponent *meeting_page_get_cancel_comp   (MeetingPage   *mpage);



G_END_DECLS

#endif
