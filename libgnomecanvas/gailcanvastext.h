/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GAIL_CANVAS_TEXT_H__
#define __GAIL_CANVAS_TEXT_H__

#include <libgnomecanvas/libgnomecanvas.h>
#include <atk/atk.h>
#include "gailcanvasitem.h"
#include <libgail-util/gailtextutil.h>

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS_TEXT                  (gail_canvas_text_get_type ())
#define GAIL_CANVAS_TEXT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS_TEXT, GailCanvasText))
#define GAIL_CANVAS_TEXT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS_TEXT, GailCanvasTextClass))
#define GAIL_IS_CANVAS_TEXT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS_TEXT))
#define GAIL_IS_CANVAS_TEXT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS_TEXT))
#define GAIL_CANVAS_TEXT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS_TEXT, GailCanvasTextClass))

typedef struct _GailCanvasText                 GailCanvasText;
typedef struct _GailCanvasTextClass            GailCanvasTextClass;

GType
gail_canvas_text_get_type (void);

struct _GailCanvasTextClass
{
  GailCanvasItemClass parent_class;
};

AtkObject * gail_canvas_text_new (GObject *obj);

G_END_DECLS

#endif /* __GAIL_CANVAS_TEXT_H__ */
