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

#ifndef __GAIL_CANVAS_ITEM_H__
#define __GAIL_CANVAS_ITEM_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <atk/atk.h>

G_BEGIN_DECLS

#define GAIL_TYPE_CANVAS_ITEM                  (gail_canvas_item_get_type ())
#define GAIL_CANVAS_ITEM(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CANVAS_ITEM, GailCanvasItem))
#define GAIL_CANVAS_ITEM_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CANVAS_ITEM, GailCanvasItemClass))
#define GAIL_IS_CANVAS_ITEM(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CANVAS_ITEM))
#define GAIL_IS_CANVAS_ITEM_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CANVAS_ITEM))
#define GAIL_CANVAS_ITEM_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CANVAS_ITEM, GailCanvasItemClass))

typedef struct _GailCanvasItem                 GailCanvasItem;
typedef struct _GailCanvasItemClass            GailCanvasItemClass;

struct _GailCanvasItem
{
  AtkGObjectAccessible parent;
};

GType gail_canvas_item_get_type (void);

struct _GailCanvasItemClass
{
  AtkGObjectAccessibleClass parent_class;
};

AtkObject * gail_canvas_item_new (GObject *obj);

G_END_DECLS

#endif /* __GAIL_CANVAS_ITEM_H__ */
