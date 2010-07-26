/*
 * Borrowed from Moblin-Web-Browser: The web browser for Moblin
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "e-mail-tab-picker.h"

static void mx_droppable_iface_init (MxDroppableIface *iface);
static gint e_mail_tab_picker_find_tab_cb (gconstpointer a, gconstpointer b);

G_DEFINE_TYPE_WITH_CODE (EMailTabPicker,
                         e_mail_tab_picker,
                         MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_DROPPABLE,
                                                mx_droppable_iface_init))

#define TAB_PICKER_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), E_MAIL_TYPE_TAB_PICKER, EMailTabPickerPrivate))

enum
{
  PROP_0,

  PROP_PREVIEW_MODE,
  PROP_DROP_ENABLED,
};

enum
{
  TAB_ACTIVATED,
  CHOOSER_CLICKED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

typedef struct
{
  EMailTab      *tab;
  gfloat       position;
  gfloat       width;
  gboolean     docking;
  gboolean     docked;
} EMailTabPickerProps;

struct _EMailTabPickerPrivate
{
  GList        *tabs;
  gint          n_tabs;
  ClutterActor *chooser_button;
  ClutterActor *close_button;
  gint          current_tab;
  gboolean      preview_mode;
  gboolean      drop_enabled;
  gboolean      in_drag;
  gboolean      drag_preview;

  gint          width;
  gint          total_width;
  gint          max_offset;
  gboolean      docked_tabs;

  ClutterTimeline *scroll_timeline;
  ClutterAlpha    *scroll_alpha;
  gint             scroll_start;
  gint             scroll_end;
  gint             scroll_offset;
  gboolean         keep_current_visible;
  MxAdjustment    *scroll_adjustment;
  ClutterActor    *scroll_bar;

  ClutterTimeline *preview_timeline;
  gfloat           preview_progress;
};

static void
e_mail_tab_picker_over_in (MxDroppable *droppable,
                        MxDraggable *draggable)
{
}

static void
e_mail_tab_picker_over_out (MxDroppable *droppable,
                         MxDraggable *draggable)
{
}

static void
e_mail_tab_picker_drop (MxDroppable         *droppable,
                     MxDraggable         *draggable,
                     gfloat               event_x,
                     gfloat               event_y,
                     gint                 button,
                     ClutterModifierType  modifiers)
{
  GList *t;
  EMailTabPickerProps *tab;
  gint current_position, new_position;

  EMailTabPicker *picker = E_MAIL_TAB_PICKER (droppable);
  EMailTabPickerPrivate *priv = picker->priv;

  /* Make sure this is a valid drop */
  if (!priv->drop_enabled)
    return;

  if (!E_MAIL_IS_TAB (draggable))
    return;

  if (clutter_actor_get_parent (CLUTTER_ACTOR (draggable)) !=
      (ClutterActor *)picker)
    return;

  /* Get current position and property data structure */
  t = g_list_find_custom (priv->tabs, draggable, e_mail_tab_picker_find_tab_cb);
  tab = (EMailTabPickerProps *)t->data;
  if (!tab)
    {
      g_warning ("Tab that's parented to a picker not actually in picker");
      return;
    }
  current_position = g_list_position (priv->tabs, t);

  /* Work out new position */
  for (new_position = 0, t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *props = t->data;

      /* Ignore docked tabs */
      if (!props->docked)
        {
          /* If the tab is beyond the dragged tab and not draggable,
           * we don't want to drag past it.
           */
          if ((event_x >= props->position + priv->scroll_offset) &&
              (tab->position + tab->width <= props->position) &&
              !mx_draggable_is_enabled (MX_DRAGGABLE (props->tab)))
            {
              new_position--;
              break;
            }

          /* The same check for dragging left instead of right */
          if ((event_x < props->position + props->width + priv->scroll_offset)&&
              (tab->position >= props->position) &&
              !mx_draggable_is_enabled (MX_DRAGGABLE (props->tab)))
            break;

          /* If the tab-end position is after the drop position,
           * break - we want to drop before here.
           */
          if (props->position + props->width + priv->scroll_offset > event_x)
            break;
        }

      /* Increment the position */
      new_position++;
    }

  /* Re-order */
  e_mail_tab_picker_reorder (picker, current_position, new_position);
}

static void
mx_droppable_iface_init (MxDroppableIface *iface)
{
  iface->over_in = e_mail_tab_picker_over_in;
  iface->over_out = e_mail_tab_picker_over_out;
  iface->drop = e_mail_tab_picker_drop;
}

static void
e_mail_tab_picker_get_property (GObject *object, guint property_id,
                             GValue *value, GParamSpec *pspec)
{
  EMailTabPicker *tab_picker = E_MAIL_TAB_PICKER (object);
  EMailTabPickerPrivate *priv = tab_picker->priv;

  switch (property_id)
    {
    case PROP_PREVIEW_MODE:
      g_value_set_boolean (value, e_mail_tab_picker_get_preview_mode (tab_picker));
      break;

    case PROP_DROP_ENABLED:
      g_value_set_boolean (value, priv->drop_enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
e_mail_tab_picker_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  EMailTabPicker *tab_picker = E_MAIL_TAB_PICKER (object);
  EMailTabPickerPrivate *priv = tab_picker->priv;

  switch (property_id)
    {
    case PROP_PREVIEW_MODE:
      e_mail_tab_picker_set_preview_mode (tab_picker, g_value_get_boolean (value));
      break;

    case PROP_DROP_ENABLED:
      priv->drop_enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
e_mail_tab_picker_dispose (GObject *object)
{
  EMailTabPicker *picker = E_MAIL_TAB_PICKER (object);
  EMailTabPickerPrivate *priv = picker->priv;

  if (priv->scroll_bar)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->scroll_bar));
      priv->scroll_bar = NULL;
    }

  if (priv->scroll_timeline)
    {
      clutter_timeline_stop (priv->scroll_timeline);
      g_object_unref (priv->scroll_alpha);
      g_object_unref (priv->scroll_timeline);
      priv->scroll_timeline = NULL;
      priv->scroll_alpha = NULL;
    }

  if (priv->preview_timeline)
    {
      clutter_timeline_stop (priv->preview_timeline);
      g_object_unref (priv->preview_timeline);
      priv->preview_timeline = NULL;
    }

  if (priv->chooser_button)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->chooser_button));
      priv->chooser_button = NULL;
    }

  if (priv->close_button)
    {
      clutter_actor_unparent (CLUTTER_ACTOR (priv->close_button));
      priv->close_button = NULL;
    }

  while (priv->tabs)
    {
      EMailTabPickerProps *props = priv->tabs->data;
      e_mail_tab_picker_remove_tab (picker, props->tab);
    }

  G_OBJECT_CLASS (e_mail_tab_picker_parent_class)->dispose (object);
}

static void
e_mail_tab_picker_finalize (GObject *object)
{
  G_OBJECT_CLASS (e_mail_tab_picker_parent_class)->finalize (object);
}

static void
e_mail_tab_picker_paint (ClutterActor *actor)
{
  GList *t;
  gfloat width, height, offset;

  EMailTabPickerPrivate *priv = E_MAIL_TAB_PICKER (actor)->priv;

  CLUTTER_ACTOR_CLASS (e_mail_tab_picker_parent_class)->paint (actor);

  clutter_actor_get_size (actor, &width, &height);

  cogl_clip_push_rectangle (0, 0, width, height);

  offset = priv->scroll_offset;
  cogl_translate (-priv->scroll_offset, 0, 0);

  /* Draw normal tabs */
  for (t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *props = t->data;

      if (props->docked)
        continue;
      if (props->position + props->width < offset)
        continue;
      if (props->position > width + offset)
        break;

      if (CLUTTER_ACTOR_IS_MAPPED (props->tab))
        clutter_actor_paint (CLUTTER_ACTOR (props->tab));
    }

  cogl_translate (priv->scroll_offset, 0, 0);

  /* Draw docked tabs */
  if (priv->docked_tabs)
    {
      for (t = priv->tabs; t; t = t->next)
        {
          EMailTabPickerProps *props = t->data;

          if (!props->docked)
            continue;

          if (CLUTTER_ACTOR_IS_MAPPED (props->tab))
            clutter_actor_paint (CLUTTER_ACTOR (props->tab));
        }
    }

  cogl_clip_pop ();

  /* Draw tab chooser button */
  if (CLUTTER_ACTOR_IS_MAPPED (priv->chooser_button))
    clutter_actor_paint (CLUTTER_ACTOR (priv->chooser_button));

  /* Draw scrollbar */
  if (CLUTTER_ACTOR_IS_MAPPED (priv->scroll_bar))
    {
      gfloat height;
      clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->close_button),
                                          -1, NULL, &height);
      height *= priv->preview_progress;
      if (height >= 1.0)
        {
          cogl_clip_push_rectangle (0, 0, width, height);
          if (CLUTTER_ACTOR_IS_MAPPED (priv->close_button))
            clutter_actor_paint (CLUTTER_ACTOR (priv->close_button));
          clutter_actor_paint (CLUTTER_ACTOR (priv->scroll_bar));
          cogl_clip_pop ();
        }
    }
}

static void
e_mail_tab_picker_pick (ClutterActor       *actor,
                     const ClutterColor *color)
{
  EMailTabPickerPrivate *priv = E_MAIL_TAB_PICKER (actor)->priv;

  /* Chain up to paint background */
  CLUTTER_ACTOR_CLASS (e_mail_tab_picker_parent_class)->pick (actor, color);

  if (!priv->in_drag)
    e_mail_tab_picker_paint (actor);
}

static void
e_mail_tab_picker_get_preferred_width (ClutterActor *actor,
                                    gfloat        for_height,
                                    gfloat       *min_width_p,
                                    gfloat       *natural_width_p)
{
  GList *t;
  MxPadding padding;

  EMailTabPickerPrivate *priv = E_MAIL_TAB_PICKER (actor)->priv;

  clutter_actor_get_preferred_width (CLUTTER_ACTOR (priv->chooser_button),
                                     for_height, min_width_p, natural_width_p);

  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (min_width_p)
    *min_width_p += padding.left + padding.right;
  if (natural_width_p)
    *natural_width_p += padding.left + padding.right;

  for (t = priv->tabs; t; t = t->next)
    {
      gfloat min_width, natural_width;

      EMailTabPickerProps *props = t->data;

      clutter_actor_get_preferred_width (CLUTTER_ACTOR (props->tab), for_height,
                                         &min_width, &natural_width);

      if (min_width_p && !t->prev)
        *min_width_p += min_width;
      if (natural_width_p)
        *natural_width_p += natural_width;
    }
}

void
e_mail_tab_picker_get_preferred_height (EMailTabPicker *tab_picker,
                                     gfloat        for_width,
                                     gfloat       *min_height_p,
                                     gfloat       *natural_height_p,
                                     gboolean      with_previews)
{
  MxPadding padding;

  ClutterActor *actor = CLUTTER_ACTOR (tab_picker);
  EMailTabPickerPrivate *priv = tab_picker->priv;

  clutter_actor_get_preferred_height (CLUTTER_ACTOR (priv->chooser_button),
                                     for_width, min_height_p, natural_height_p);

  if (priv->tabs)
    {
      gfloat min_height, natural_height, scroll_height;

      EMailTabPickerProps *props = priv->tabs->data;

      /* Get the height of the first tab - it's assumed that tabs are
       * fixed height.
       */
      if (with_previews)
        {
          clutter_actor_get_preferred_height (CLUTTER_ACTOR (props->tab),
                                              for_width,
                                              &min_height,
                                              &natural_height);
          if (CLUTTER_ACTOR_IS_VISIBLE (priv->scroll_bar))
            {
              /* Add the height of the scrollbar-section */
              clutter_actor_get_preferred_height (
                CLUTTER_ACTOR (priv->close_button), -1, NULL, &scroll_height);
              scroll_height *= priv->preview_progress;

              min_height += scroll_height;
              natural_height += scroll_height;
            }
        }
      else
        e_mail_tab_get_height_no_preview (props->tab, for_width,
                                       &min_height, &natural_height);

      if (min_height_p && (*min_height_p < min_height))
        *min_height_p = min_height;
      if (natural_height_p && (*natural_height_p < natural_height))
        *natural_height_p = natural_height;
    }

  mx_widget_get_padding (MX_WIDGET (actor), &padding);
  if (min_height_p)
    *min_height_p += padding.top + padding.bottom;
  if (natural_height_p)
    *natural_height_p += padding.top + padding.bottom;
}

static void
_e_mail_tab_picker_get_preferred_height (ClutterActor *actor,
                                      gfloat        for_width,
                                      gfloat       *min_height_p,
                                      gfloat       *natural_height_p)
{
  e_mail_tab_picker_get_preferred_height (E_MAIL_TAB_PICKER (actor), for_width,
                                       min_height_p, natural_height_p, TRUE);
}

static void
e_mail_tab_picker_allocate_docked (EMailTabPicker           *tab_picker,
                                const ClutterActorBox  *picker_box_p,
                                const ClutterActorBox  *chooser_box_p,
                                ClutterAllocationFlags  flags)
{
  GList *t;
  MxPadding padding;
  ClutterActorBox picker_box, chooser_box, child_box;
  gfloat offset, width, left, right, height;

  EMailTabPickerPrivate *priv = tab_picker->priv;

  if (!picker_box_p)
    {
      clutter_actor_get_allocation_box (CLUTTER_ACTOR (tab_picker),
                                        &picker_box);
      picker_box_p = &picker_box;
    }
  if (!chooser_box_p)
    {
      clutter_actor_get_allocation_box (CLUTTER_ACTOR (priv->chooser_button),
                                        &chooser_box);
      chooser_box_p = &chooser_box;
    }

  mx_widget_get_padding (MX_WIDGET (tab_picker), &padding);

  /* Calculate available width and height */
  width = picker_box_p->x2 - picker_box_p->x1 - padding.right;

  e_mail_tab_picker_get_preferred_height (tab_picker, -1, NULL, &height, FALSE);
  child_box.y2 = picker_box_p->y2 - picker_box_p->y1 - padding.bottom;
  child_box.y1 = child_box.y2 - height;

  /* Don't dock over the chooser button */
  width -= chooser_box_p->x2 - chooser_box_p->x1;

  offset = priv->scroll_offset;

  left = 0;
  right = width;
  priv->docked_tabs = FALSE;

  for (t = g_list_last (priv->tabs); t; t = t->prev)
    {
      EMailTabPickerProps *props = t->data;

      props->docked = FALSE;

      if (!props->docking)
        continue;

      if (props->position < offset)
        {
          /* Dock left */
          priv->docked_tabs = TRUE;
          props->docked = TRUE;
          child_box.x1 = left;
          child_box.x2 = child_box.x1 + props->width;
          left += props->width;
        }
      else if (props->position + props->width > width + offset)
        {
          /* Dock right */
          priv->docked_tabs = TRUE;
          props->docked = TRUE;
          child_box.x2 = right;
          child_box.x1 = child_box.x2 - props->width;
          right -= props->width;
        }
      else
        {
          child_box.x1 = props->position;
          child_box.x2 = child_box.x1 + props->width;
        }

      clutter_actor_allocate (CLUTTER_ACTOR (props->tab), &child_box, flags);
    }
}

static void
e_mail_tab_picker_scroll_new_frame_cb (ClutterTimeline *timeline,
                                    guint            msecs,
                                    EMailTabPicker    *tab_picker)
{
  EMailTabPickerPrivate *priv = tab_picker->priv;
  gdouble alpha = clutter_alpha_get_alpha (priv->scroll_alpha);

  priv->scroll_offset = (priv->scroll_start * (1.0 - alpha)) +
                        (priv->scroll_end * alpha);
  mx_adjustment_set_value (priv->scroll_adjustment, priv->scroll_offset);
  e_mail_tab_picker_allocate_docked (tab_picker, NULL, NULL, 0);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (tab_picker));
}

static void
e_mail_tab_picker_scroll_completed_cb (ClutterTimeline *timeline,
                                    EMailTabPicker    *tab_picker)
{
  EMailTabPickerPrivate *priv = tab_picker->priv;

  priv->scroll_offset = priv->scroll_end;
  mx_adjustment_set_value (priv->scroll_adjustment, priv->scroll_offset);
  e_mail_tab_picker_allocate_docked (tab_picker, NULL, NULL, 0);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (tab_picker));

  g_object_unref (priv->scroll_alpha);
  g_object_unref (priv->scroll_timeline);
  priv->scroll_alpha = NULL;
  priv->scroll_timeline = NULL;
}

static void
e_mail_tab_picker_scroll_to (EMailTabPicker *tab_picker,
                          gint          destination,
                          guint         duration)
{
  EMailTabPickerPrivate *priv = tab_picker->priv;

  priv->scroll_start = priv->scroll_offset;
  priv->scroll_end = CLAMP (destination, 0, priv->max_offset);

  if (priv->scroll_timeline)
    {
      clutter_timeline_stop (priv->scroll_timeline);
      clutter_timeline_rewind (priv->scroll_timeline);
      clutter_timeline_set_duration (priv->scroll_timeline, duration);
    }
  else
    {
      if (priv->scroll_end == priv->scroll_offset)
        return;

      priv->scroll_timeline = clutter_timeline_new (duration);
      priv->scroll_alpha = clutter_alpha_new_full (priv->scroll_timeline,
                                                   CLUTTER_EASE_OUT_QUAD);
      g_signal_connect (priv->scroll_timeline, "new_frame",
                        G_CALLBACK (e_mail_tab_picker_scroll_new_frame_cb),
                        tab_picker);
      g_signal_connect (priv->scroll_timeline, "completed",
                        G_CALLBACK (e_mail_tab_picker_scroll_completed_cb),
                        tab_picker);
    }

  clutter_timeline_start (priv->scroll_timeline);
}

static void
e_mail_tab_picker_allocate (ClutterActor           *actor,
                         const ClutterActorBox  *box,
                         ClutterAllocationFlags  flags)
{
  GList *t;
  MxPadding padding;
  gint old_max_offset, old_scroll_offset;
  ClutterActorBox child_box, scroll_box;
  gfloat width, total_width, height;

  EMailTabPicker *tab_picker = E_MAIL_TAB_PICKER (actor);
  EMailTabPickerPrivate *priv = tab_picker->priv;

  mx_widget_get_padding (MX_WIDGET (actor), &padding);

  /* Allocate for scroll-bar and close button */
  clutter_actor_get_preferred_size (CLUTTER_ACTOR (priv->close_button),
                                    NULL, NULL, &width, &height);
  child_box.x1 = 0;
  child_box.x2 = box->x2 - box->x1 - padding.right;
  child_box.y1 = 0;
  child_box.y2 = child_box.y1 + height;
  clutter_actor_allocate (CLUTTER_ACTOR (priv->close_button),
                          &child_box,
                          flags);

  /* FIXME: Make this a property */
#define SPACING 4.0
  /* Work out allocation for scroll-bar, but allocate it later */
  scroll_box = child_box;
  scroll_box.x2 -= width + SPACING;
  scroll_box.x1 += SPACING;
  scroll_box.y1 += SPACING;
  scroll_box.y2 -= SPACING;

  child_box.y1 += (height * priv->preview_progress) + padding.top;

  /* Allocate for tabs */
  total_width = 0;
  child_box.x1 = padding.left;
  e_mail_tab_picker_get_preferred_height (tab_picker, -1, NULL, &height, FALSE);
  for (t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *props = t->data;
      ClutterActor *actor = CLUTTER_ACTOR (props->tab);

      clutter_actor_get_preferred_width (actor, child_box.y2, NULL, &width);

      /* Fill out data - note it's ok to fill out docking here as when it
       * changes, the tab queues a relayout.
       */
      props->docking = e_mail_tab_get_docking (props->tab);
      props->position = child_box.x1;
      props->width = width;

      total_width += width;

      /* Don't stretch tabs without a preview to fit tabs with a preview */
      if (e_mail_tab_get_preview_actor (props->tab))
        child_box.y2 = box->y2 - box->y1 - padding.bottom;
      else
        child_box.y2 = child_box.y1 + height;

      child_box.x2 = child_box.x1 + width;
      clutter_actor_allocate (actor, &child_box, flags);

      child_box.x1 = child_box.x2;
    }

  /* Allocate for the chooser button */
  clutter_actor_get_preferred_width (CLUTTER_ACTOR (priv->chooser_button),
                                     box->y2 - box->y1, NULL, &width);

  child_box.x2 = box->x2 - box->x1 - padding.right;
  child_box.x1 = child_box.x2 - width;
  child_box.y1 = 0;
  child_box.y2 = child_box.y1 + height;
  clutter_actor_allocate (CLUTTER_ACTOR (priv->chooser_button),
                          &child_box, flags);

  /* Cache some useful size values */
  priv->width = (gint)(box->x2 - box->x1);

  priv->total_width = (gint)(total_width + padding.left + padding.right);

  old_max_offset = priv->max_offset;
  priv->max_offset = priv->total_width - priv->width +
                     (gint)(child_box.x2 - child_box.x1);
  if (priv->max_offset < 0)
    priv->max_offset = 0;

  /* Allocate for tab picker */
  old_scroll_offset = priv->scroll_offset;
  priv->scroll_offset = CLAMP (priv->scroll_offset, 0, priv->max_offset);
  e_mail_tab_picker_allocate_docked (tab_picker, box, &child_box, flags);

  /* Chain up (store box) */
  CLUTTER_ACTOR_CLASS (e_mail_tab_picker_parent_class)->
    allocate (actor, box, flags);

  /* Sync up the scroll-bar properties */
  g_object_set (G_OBJECT (priv->scroll_adjustment),
                "page-increment", (gdouble)(box->x2 - box->x1),
                "page-size", (gdouble)(box->x2 - box->x1),
                "upper", (gdouble)total_width,
                NULL);

  if ((priv->max_offset != old_max_offset) ||
      (priv->scroll_offset != old_scroll_offset))
    mx_adjustment_set_value (priv->scroll_adjustment,
                             (gdouble)priv->scroll_offset);

  /* Allocate for scroll-bar */
  clutter_actor_allocate (CLUTTER_ACTOR (priv->scroll_bar), &scroll_box, flags);

  /* Keep current tab visible */
  if (priv->keep_current_visible)
    {
      EMailTabPickerProps *current =
        g_list_nth_data (priv->tabs, priv->current_tab);

      if ((current->position < priv->scroll_offset) ||
          (current->position + current->width >= priv->max_offset))
        e_mail_tab_picker_scroll_to (tab_picker, current->position, 150);
    }
}

static void
e_mail_tab_picker_map (ClutterActor *actor)
{
  GList *t;
  EMailTabPickerPrivate *priv = E_MAIL_TAB_PICKER (actor)->priv;

  CLUTTER_ACTOR_CLASS (e_mail_tab_picker_parent_class)->map (actor);

  clutter_actor_map (CLUTTER_ACTOR (priv->chooser_button));
  clutter_actor_map (CLUTTER_ACTOR (priv->close_button));
  clutter_actor_map (CLUTTER_ACTOR (priv->scroll_bar));

  for (t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *props = t->data;
      clutter_actor_map (CLUTTER_ACTOR (props->tab));
    }
}

static void
e_mail_tab_picker_unmap (ClutterActor *actor)
{
  GList *t;
  EMailTabPickerPrivate *priv = E_MAIL_TAB_PICKER (actor)->priv;

  CLUTTER_ACTOR_CLASS (e_mail_tab_picker_parent_class)->unmap (actor);

  clutter_actor_unmap (CLUTTER_ACTOR (priv->chooser_button));
  clutter_actor_unmap (CLUTTER_ACTOR (priv->close_button));
  clutter_actor_unmap (CLUTTER_ACTOR (priv->scroll_bar));

  for (t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *props = t->data;
      clutter_actor_unmap (CLUTTER_ACTOR (props->tab));
    }
}

static void
e_mail_tab_picker_class_init (EMailTabPickerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EMailTabPickerPrivate));

  object_class->get_property = e_mail_tab_picker_get_property;
  object_class->set_property = e_mail_tab_picker_set_property;
  object_class->dispose = e_mail_tab_picker_dispose;
  object_class->finalize = e_mail_tab_picker_finalize;

  actor_class->paint = e_mail_tab_picker_paint;
  actor_class->pick = e_mail_tab_picker_pick;
  actor_class->get_preferred_width = e_mail_tab_picker_get_preferred_width;
  actor_class->get_preferred_height = _e_mail_tab_picker_get_preferred_height;
  actor_class->allocate = e_mail_tab_picker_allocate;
  actor_class->map = e_mail_tab_picker_map;
  actor_class->unmap = e_mail_tab_picker_unmap;

  g_object_class_install_property (object_class,
                                   PROP_PREVIEW_MODE,
                                   g_param_spec_boolean ("preview-mode",
                                                         "Preview mode",
                                                         "Whether to display "
                                                         "in preview mode.",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_NICK |
                                                         G_PARAM_STATIC_BLURB));

  g_object_class_override_property (object_class,
                                    PROP_DROP_ENABLED,
                                    "drop-enabled");

  signals[TAB_ACTIVATED] =
    g_signal_new ("tab-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EMailTabPickerClass, tab_activated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, E_MAIL_TYPE_TAB);

  signals[CHOOSER_CLICKED] =
    g_signal_new ("chooser-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (EMailTabPickerClass, chooser_clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
e_mail_tab_picker_chooser_clicked_cb (ClutterActor *button,
                                   EMailTabPicker *self)
{
  g_signal_emit (self, signals[CHOOSER_CLICKED], 0);
}

static gboolean
e_mail_tab_picker_scroll_event_cb (ClutterActor       *actor,
                                ClutterScrollEvent *event,
                                gpointer            user_data)
{
  EMailTabPicker *self = E_MAIL_TAB_PICKER (actor);
  EMailTabPickerPrivate *priv = self->priv;

  priv->keep_current_visible = FALSE;

  switch (event->direction)
    {
    case CLUTTER_SCROLL_UP :
    case CLUTTER_SCROLL_LEFT :
      e_mail_tab_picker_scroll_to (self, priv->scroll_end - 200, 150);
      break;

    case CLUTTER_SCROLL_DOWN :
    case CLUTTER_SCROLL_RIGHT :
      e_mail_tab_picker_scroll_to (self, priv->scroll_end + 200, 150);
      break;
    }

  return TRUE;
}

static void
e_mail_tab_picker_scroll_value_cb (MxAdjustment   *adjustment,
                                GParamSpec     *pspec,
                                EMailTabPicker   *picker)
{
  EMailTabPickerPrivate *priv = picker->priv;
  gdouble value = mx_adjustment_get_value (adjustment);

  if ((gint)value != priv->scroll_offset)
    {
      priv->keep_current_visible = FALSE;
      priv->scroll_offset = (gint)value;
      clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
    }
}

static void
e_mail_tab_picker_init (EMailTabPicker *self)
{
  EMailTabPickerPrivate *priv = self->priv = TAB_PICKER_PRIVATE (self);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  priv->chooser_button = mx_button_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (priv->chooser_button),
                          "chooser-button");
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->chooser_button),
                            CLUTTER_ACTOR (self));

  priv->close_button = mx_button_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (priv->close_button),
                          "chooser-close-button");
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->close_button),
                            CLUTTER_ACTOR (self));
  clutter_actor_hide (CLUTTER_ACTOR (priv->close_button));

  priv->scroll_adjustment = mx_adjustment_new_with_values (0, 0, 0, 100, 200, 200);
  priv->scroll_bar = mx_scroll_bar_new_with_adjustment (priv->scroll_adjustment);
  g_object_unref (priv->scroll_adjustment);
  clutter_actor_set_parent (CLUTTER_ACTOR (priv->scroll_bar),
                            CLUTTER_ACTOR (self));
  clutter_actor_hide (CLUTTER_ACTOR (priv->scroll_bar));

  g_signal_connect (priv->chooser_button, "clicked",
                    G_CALLBACK (e_mail_tab_picker_chooser_clicked_cb), self);
  g_signal_connect (priv->close_button, "clicked",
                    G_CALLBACK (e_mail_tab_picker_chooser_clicked_cb), self);
  g_signal_connect (self, "scroll-event",
                    G_CALLBACK (e_mail_tab_picker_scroll_event_cb), NULL);
}

static gint
e_mail_tab_picker_find_tab_cb (gconstpointer a, gconstpointer b)
{
  EMailTabPickerProps *props = (EMailTabPickerProps *)a;
  EMailTab *tab = (EMailTab *)b;

  if (props->tab == tab)
    return 0;
  else
    return -1;
}

static void
e_mail_tab_picker_tab_clicked_cb (EMailTab *tab, EMailTabPicker *self)
{
  EMailTabPickerPrivate *priv = self->priv;
  EMailTab *old_tab =
    ((EMailTabPickerProps *)g_list_nth_data (priv->tabs, priv->current_tab))->tab;
  GList *new_tab_link = g_list_find_custom (priv->tabs, tab,
                                            e_mail_tab_picker_find_tab_cb);

  if (!new_tab_link)
    return;

  priv->keep_current_visible = TRUE;

  /* If the same tab is clicked, make sure we remain active and return */
  if (tab == old_tab)
    {
      e_mail_tab_set_active (tab, TRUE);
      if (priv->preview_mode)
        g_signal_emit (self, signals[TAB_ACTIVATED], 0, tab);
      return;
    }

  /* Deselect old tab */
  e_mail_tab_set_active (old_tab, FALSE);

  /* Set new tab */
  priv->current_tab = g_list_position (priv->tabs, new_tab_link);
  g_signal_emit (self, signals[TAB_ACTIVATED], 0, tab);
}

ClutterActor *
e_mail_tab_picker_new (void)
{
  return g_object_new (E_MAIL_TYPE_TAB_PICKER, NULL);
}

static void
e_mail_tab_picker_tab_drag_begin_cb (MxDraggable         *draggable,
                                  gfloat               event_x,
                                  gfloat               event_y,
                                  gint                 event_button,
                                  ClutterModifierType  modifiers,
                                  EMailTabPicker        *picker)
{
  EMailTabPickerPrivate *priv = picker->priv;
  priv->in_drag = TRUE;

  if (!priv->preview_mode)
    {
      e_mail_tab_picker_set_preview_mode (picker, TRUE);
      priv->drag_preview = TRUE;
    }
}

static void
e_mail_tab_picker_tab_drag_end_cb (MxDraggable  *draggable,
                                gfloat        event_x,
                                gfloat        event_y,
                                EMailTabPicker *picker)
{
  EMailTabPickerPrivate *priv = picker->priv;
  priv->in_drag = FALSE;

  if (priv->drag_preview)
    {
      e_mail_tab_picker_set_preview_mode (picker, FALSE);
      priv->drag_preview = FALSE;
    }
}

void
e_mail_tab_picker_add_tab (EMailTabPicker *picker, EMailTab *tab, gint position)
{
  EMailTabPickerProps *props;
  EMailTabPickerPrivate *priv = picker->priv;

  if (priv->tabs && (priv->current_tab >= position))
    priv->current_tab++;

  props = g_slice_new (EMailTabPickerProps);
  props->tab = tab;
  priv->tabs = g_list_insert (priv->tabs, props, position);
  priv->n_tabs++;

  clutter_actor_set_parent (CLUTTER_ACTOR (tab), CLUTTER_ACTOR (picker));
  mx_draggable_set_axis (MX_DRAGGABLE (tab), MX_DRAG_AXIS_X);

  g_signal_connect_after (tab, "clicked",
                          G_CALLBACK (e_mail_tab_picker_tab_clicked_cb), picker);
  g_signal_connect (tab, "drag-begin",
                    G_CALLBACK (e_mail_tab_picker_tab_drag_begin_cb), picker);
  g_signal_connect (tab, "drag-end",
                    G_CALLBACK (e_mail_tab_picker_tab_drag_end_cb), picker);

  e_mail_tab_set_preview_mode (tab, priv->preview_mode);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
}

void
e_mail_tab_picker_remove_tab (EMailTabPicker *picker, EMailTab *tab)
{
  GList *tab_link;
  EMailTabPickerPrivate *priv = picker->priv;

  tab_link = g_list_find_custom (priv->tabs, tab, e_mail_tab_picker_find_tab_cb);

  if (!tab_link)
    return;

  g_signal_handlers_disconnect_by_func (tab,
                                        e_mail_tab_picker_tab_clicked_cb,
                                        picker);
  g_signal_handlers_disconnect_by_func (tab,
                                        e_mail_tab_picker_tab_drag_begin_cb,
                                        picker);
  g_signal_handlers_disconnect_by_func (tab,
                                        e_mail_tab_picker_tab_drag_end_cb,
                                        picker);

  /* We don't want to do this during dispose, checking if chooser_button
   * exists is a way of checking if we're in dispose without keeping an
   * extra variable around.
   */
  if (priv->chooser_button)
    {
      gint position = g_list_position (priv->tabs, tab_link);
      if (priv->current_tab)
        {
          if (priv->current_tab > position)
            priv->current_tab--;
          else if (priv->current_tab == position)
            e_mail_tab_picker_set_current_tab (picker, priv->current_tab - 1);
        }
      else if (priv->tabs->next && (position == 0))
        {
          e_mail_tab_picker_set_current_tab (picker, priv->current_tab + 1);
          priv->current_tab--;
        }
    }

  g_slice_free (EMailTabPickerProps, tab_link->data);
  priv->tabs = g_list_delete_link (priv->tabs, tab_link);
  clutter_actor_unparent (CLUTTER_ACTOR (tab));
  priv->n_tabs--;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
}

GList *
e_mail_tab_picker_get_tabs (EMailTabPicker *picker)
{
  GList *tab_list, *t;

  EMailTabPickerPrivate *priv = picker->priv;

  tab_list = NULL;
  for (t = g_list_last (priv->tabs); t; t = t->prev)
    {
      EMailTabPickerProps *props = t->data;
      tab_list = g_list_prepend (tab_list, props->tab);
    }

  return tab_list;
}

EMailTab *
e_mail_tab_picker_get_tab (EMailTabPicker *picker, gint tab)
{
  EMailTabPickerProps *props = g_list_nth_data (picker->priv->tabs, tab);
  return props->tab;
}

gint
e_mail_tab_picker_get_tab_no (EMailTabPicker *picker, EMailTab *tab)
{
  GList *tab_link = g_list_find_custom (picker->priv->tabs, tab,
                                        e_mail_tab_picker_find_tab_cb);
  return g_list_position (picker->priv->tabs, tab_link);
}

gint
e_mail_tab_picker_get_current_tab (EMailTabPicker *picker)
{
  return picker->priv->current_tab;
}

void
e_mail_tab_picker_set_current_tab (EMailTabPicker *picker, gint tab_no)
{
  EMailTabPickerPrivate *priv = picker->priv;
  EMailTabPickerProps *props;

  printf("OLD %d new %d\n", priv->current_tab, tab_no);
  if (priv->n_tabs == 0)
    return;

  if (ABS (tab_no) >= priv->n_tabs)
    return;

  if (tab_no < 0)
    tab_no = priv->n_tabs + tab_no;

  props = g_list_nth_data (priv->tabs, (guint)tab_no);

  if (props)
    {
      e_mail_tab_picker_tab_clicked_cb (props->tab, picker);
      e_mail_tab_set_active (props->tab, TRUE);
    }
}

void
e_mail_tab_picker_reorder (EMailTabPicker *picker,
                        gint          old_position,
                        gint          new_position)
{
  GList *link;
  gpointer data;

  EMailTabPickerPrivate *priv = picker->priv;

  if (old_position == new_position)
    return;

  if (!(link = g_list_nth (priv->tabs, old_position)))
    return;

  data = link->data;
  priv->tabs = g_list_delete_link (priv->tabs, link);
  priv->tabs = g_list_insert (priv->tabs, data, new_position);

  if (priv->current_tab == old_position)
    {
      if (new_position < 0)
        priv->current_tab = priv->n_tabs - 1;
      else
        priv->current_tab = CLAMP (new_position, 0, priv->n_tabs - 1);
    }
  else if ((priv->current_tab > old_position) &&
           (new_position >= priv->current_tab))
    priv->current_tab--;
  else if ((priv->current_tab < old_position) &&
           (new_position <= priv->current_tab))
    priv->current_tab++;

  clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
}

gint
e_mail_tab_picker_get_n_tabs (EMailTabPicker *picker)
{
  return picker->priv->n_tabs;
}

static void
preview_new_frame_cb (ClutterTimeline *timeline,
                      guint            msecs,
                      EMailTabPicker    *picker)
{
  picker->priv->preview_progress = clutter_timeline_get_progress (timeline);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
}

static void
preview_completed_cb (ClutterTimeline *timeline,
                      EMailTabPicker    *picker)
{
  EMailTabPickerPrivate *priv = picker->priv;

  if (priv->preview_timeline)
    {
      g_object_unref (priv->preview_timeline);
      priv->preview_timeline = NULL;

      if (priv->preview_mode)
        {
          priv->preview_progress = 1.0;
          clutter_actor_hide (CLUTTER_ACTOR (priv->chooser_button));
        }
      else
        {
          priv->preview_progress = 0.0;
          clutter_actor_hide (CLUTTER_ACTOR (priv->scroll_bar));
          clutter_actor_hide (CLUTTER_ACTOR (priv->close_button));
        }
      clutter_actor_queue_relayout (CLUTTER_ACTOR (picker));
    }
}

void
e_mail_tab_picker_set_preview_mode (EMailTabPicker *picker, gboolean preview)
{
  GList *t;

  EMailTabPickerPrivate *priv = picker->priv;

  if (priv->preview_mode == preview)
    return;

  priv->preview_mode = preview;

  /* Put all tabs in preview mode */
  for (t = priv->tabs; t; t = t->next)
    {
      EMailTabPickerProps *prop = t->data;
      e_mail_tab_set_preview_mode (prop->tab, preview);
    }

  /* Slide in the scroll-bar */
  if (!priv->preview_timeline)
    {
      if (preview)
        clutter_actor_show (CLUTTER_ACTOR (priv->scroll_bar));

      priv->preview_timeline = clutter_timeline_new (150);
      g_signal_connect (priv->preview_timeline, "new-frame",
                        G_CALLBACK (preview_new_frame_cb), picker);
      g_signal_connect (priv->preview_timeline, "completed",
                        G_CALLBACK (preview_completed_cb), picker);
      clutter_timeline_start (priv->preview_timeline);
    }
  clutter_timeline_set_direction (priv->preview_timeline,
                                  preview ? CLUTTER_TIMELINE_FORWARD :
                                            CLUTTER_TIMELINE_BACKWARD);

  /* Connect/disconnect the scrollbar */
  if (preview)
    g_signal_connect (priv->scroll_adjustment, "notify::value",
                      G_CALLBACK (e_mail_tab_picker_scroll_value_cb), picker);
  else
    g_signal_handlers_disconnect_by_func (priv->scroll_adjustment,
                                          e_mail_tab_picker_scroll_value_cb,
                                          picker);

  if (preview)
    {
      /* Fade out the chooser button show close button */
      clutter_actor_animate (CLUTTER_ACTOR (priv->chooser_button),
                             CLUTTER_EASE_IN_OUT_QUAD, 150,
                             "opacity", 0x00,
                             NULL);
      clutter_actor_show (CLUTTER_ACTOR (priv->close_button));
    }
  else
    {
      /* Fade in the chooser button */
      clutter_actor_show (CLUTTER_ACTOR (priv->chooser_button));
      clutter_actor_animate (CLUTTER_ACTOR (priv->chooser_button),
                             CLUTTER_EASE_IN_OUT_QUAD, 150,
                             "opacity", 0xff,
                             NULL);
    }
  clutter_actor_set_reactive (CLUTTER_ACTOR (priv->chooser_button), !preview);

  /* Remove the hover state, which likely got stuck when we clicked it */
  if (!preview)
    mx_stylable_set_style_pseudo_class (MX_STYLABLE (priv->chooser_button),
                                        NULL);

  g_object_notify (G_OBJECT (picker), "preview-mode");
}

gboolean
e_mail_tab_picker_get_preview_mode (EMailTabPicker *picker)
{
  EMailTabPickerPrivate *priv = picker->priv;
  return priv->preview_mode;
}

void
e_mail_tab_picker_enable_drop (EMailTabPicker *picker, gboolean enable)
{
  EMailTabPickerPrivate *priv = picker->priv;

  if (priv->drop_enabled == enable)
    return;

  priv->drop_enabled = enable;
  if (enable)
    mx_droppable_enable (MX_DROPPABLE (picker));
  else
    mx_droppable_disable (MX_DROPPABLE (picker));

  g_object_notify (G_OBJECT (picker), "enabled");
}

