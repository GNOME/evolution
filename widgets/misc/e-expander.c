/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 */

#include <config.h>

#include "e-expander.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkcontainer.h>
#include <gdk/gdkkeysyms.h>

#define DEFAULT_EXPANDER_SIZE 10
#define DEFAULT_EXPANDER_SPACING 2

/* ESTUFF */
#ifndef _
#define _(x) (x)
#endif
#define E_EXPANDER_GET_PRIVATE(expander) ((EExpanderPrivate *)g_object_get_data (G_OBJECT (expander), "e-expander-priv"))

enum {
  PROP_0,
  PROP_EXPANDED,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_PADDING,
  PROP_LABEL_WIDGET
};

typedef struct {
  GtkWidget        *label_widget;
  gint              spacing;

  GtkExpanderStyle  expander_style;
  guint             animation_timeout;

  guint      expanded : 1;
  guint      use_underline : 1;
  guint      button_down : 1;
} EExpanderPrivate;

static void e_expander_class_init (EExpanderClass *klass);
static void e_expander_init       (EExpander      *expander);

static void e_expander_set_property (GObject          *object,
				       guint             prop_id,
				       const GValue     *value,
				       GParamSpec       *pspec);
static void e_expander_get_property (GObject          *object,
				       guint             prop_id,
				       GValue           *value,
				       GParamSpec       *pspec);

static void e_expander_destroy (GtkObject *object);

static void     e_expander_realize        (GtkWidget        *widget);
static void     e_expander_size_request   (GtkWidget        *widget,
					     GtkRequisition   *requisition);
static void     e_expander_size_allocate  (GtkWidget        *widget,
					     GtkAllocation    *allocation);
static void     e_expander_map            (GtkWidget        *widget);
static gboolean e_expander_expose         (GtkWidget        *widget,
					     GdkEventExpose   *event);
static gboolean e_expander_button_press   (GtkWidget        *widget,
					     GdkEventButton   *event);
static gboolean e_expander_button_release (GtkWidget        *widget,
					     GdkEventButton   *event);
static gboolean e_expander_motion_notify  (GtkWidget        *widget,
					     GdkEventMotion   *event);
static gboolean e_expander_enter_notify   (GtkWidget        *widget,
					     GdkEventCrossing *event);
static gboolean e_expander_leave_notify   (GtkWidget        *widget,
					     GdkEventCrossing *event);
static gboolean e_expander_focus          (GtkWidget        *widget,
					     GtkDirectionType  direction);

static void e_expander_add    (GtkContainer *container,
				 GtkWidget    *widget);
static void e_expander_remove (GtkContainer *container,
				 GtkWidget    *widget);
static void e_expander_forall (GtkContainer *container,
				 gboolean        include_internals,
				 GtkCallback     callback,
				 gpointer        callback_data);

static void e_expander_activate (EExpander *expander);

static GtkBinClass *parent_class = NULL;

GType
e_expander_get_type (void)
{
  static GType expander_type = 0;
  
  if (!expander_type)
    {
      static const GTypeInfo expander_info =
      {
	sizeof (EExpanderClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) e_expander_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (EExpander),
	0,		/* n_preallocs */
	(GInstanceInitFunc) e_expander_init,
      };
      
      expander_type = g_type_register_static (GTK_TYPE_BIN,
					      "EExpander",
					      &expander_info, 0);
    }
  
  return expander_type;
}

static void
e_expander_class_init (EExpanderClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *gtkobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class    = (GObjectClass *) klass;
  gtkobject_class  = (GtkObjectClass *) klass;
  widget_class    = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->set_property = e_expander_set_property;
  gobject_class->get_property = e_expander_get_property;

  gtkobject_class->destroy = e_expander_destroy;

  widget_class->realize              = e_expander_realize;
  widget_class->size_request         = e_expander_size_request;
  widget_class->size_allocate        = e_expander_size_allocate;
  widget_class->map                  = e_expander_map;
  widget_class->expose_event         = e_expander_expose;
  widget_class->button_press_event   = e_expander_button_press;
  widget_class->button_release_event = e_expander_button_release;
  widget_class->motion_notify_event  = e_expander_motion_notify;
  widget_class->enter_notify_event   = e_expander_enter_notify;
  widget_class->leave_notify_event   = e_expander_leave_notify;
  widget_class->focus                = e_expander_focus;

  container_class->add    = e_expander_add;
  container_class->remove = e_expander_remove;
  container_class->forall = e_expander_forall;

  klass->activate = e_expander_activate;

  /* ESTUFF  g_type_class_add_private (klass, sizeof (EExpanderPrivate)); */

  g_object_class_install_property (gobject_class,
				   PROP_EXPANDED,
				   g_param_spec_boolean ("expanded",
							 _("Expanded"),
							 _("Whether or not the expander is expanded"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							_("Label"),
							_("Text of the expander's label"),
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_USE_UNDERLINE,
				   g_param_spec_boolean ("use_underline",
							 _("Use underline"),
							 _("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_PADDING,
				   g_param_spec_int ("spacing",
						     _("Spacing"),
						     _("Space to put between the label and the child"),
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_LABEL_WIDGET,
				   g_param_spec_object ("label_widget",
							_("Label widget"),
							_("A widget to display in place of the usual expander label"),
							GTK_TYPE_WIDGET,
							G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander-size",
							     _("Expander Size"),
							     _("Size of the expander arrow"),
							     0,
							     G_MAXINT,
							     DEFAULT_EXPANDER_SIZE,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander-spacing",
							     _("Indicator Spacing"),
							     _("Spacing around expander arrow"),
							     0,
							     G_MAXINT,
							     DEFAULT_EXPANDER_SPACING,
							     G_PARAM_READABLE));

  widget_class->activate_signal =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EExpanderClass, activate),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
e_expander_init (EExpander *expander)
{
  EExpanderPrivate *priv;

  /* ESTUFF */
  priv = g_new0 (EExpanderPrivate, 1);
  g_object_set_data_full (G_OBJECT (expander), "e-expander-priv", priv, g_free);
  
  /* ESTUFF  priv = E_EXPANDER_GET_PRIVATE (expander); */

  GTK_WIDGET_SET_FLAGS (expander, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS (expander, GTK_NO_WINDOW);

  priv->label_widget = 0;
  priv->spacing = 0;

  priv->expander_style = GTK_EXPANDER_COLLAPSED;
  priv->animation_timeout = 0;

  priv->expanded = FALSE;
  priv->use_underline = FALSE;
  priv->button_down = FALSE;

}

static void
e_expander_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  EExpander *expander = E_EXPANDER (object);
                                                                                                             
  switch (prop_id)
    {
    case PROP_EXPANDED:
      e_expander_set_expanded (expander, g_value_get_boolean (value));
      break;
    case PROP_LABEL:
      e_expander_set_label (expander, g_value_get_string (value));
      break;
    case PROP_USE_UNDERLINE:
      e_expander_set_use_underline (expander, g_value_get_boolean (value));
      break;
    case PROP_PADDING:
      e_expander_set_spacing (expander, g_value_get_int (value));
      break;
    case PROP_LABEL_WIDGET:
      e_expander_set_label_widget (expander, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
e_expander_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
  EExpander *expander = E_EXPANDER (object);
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (expander);
                                                                                                             
  switch (prop_id)
    {
    case PROP_EXPANDED:
      g_value_set_boolean (value, priv->expanded);
      break;
    case PROP_LABEL:
      g_value_set_string (value, e_expander_get_label (expander));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_PADDING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
			  priv->label_widget ?
			  G_OBJECT (priv->label_widget) : NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
e_expander_destroy (GtkObject *object)
{
  EExpander *expander = E_EXPANDER (object);
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (priv->animation_timeout)
    g_source_remove (priv->animation_timeout);
  priv->animation_timeout = 0;

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
e_expander_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;
                                                                                                             
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - 2 * border_width;
  attributes.height = widget->allocation.height - 2 * border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget)     |
				GDK_POINTER_MOTION_MASK      |
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_BUTTON_PRESS_MASK        |
				GDK_BUTTON_RELEASE_MASK      |
				GDK_EXPOSURE_MASK            |
				GDK_ENTER_NOTIFY_MASK        |
				GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}
                                                                                                             
static void
e_expander_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  EExpander *expander;
  GtkBin *bin;
  EExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;

  expander = E_EXPANDER (widget);
  bin = GTK_BIN (widget);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  requisition->width = expander_size + 2 * expander_spacing +
		       2 * focus_width + 2 * focus_pad;
  requisition->height = interior_focus ? (2 * focus_width + 2 * focus_pad) : 0;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkRequisition label_requisition;

      gtk_widget_size_request (priv->label_widget, &label_requisition);

      requisition->width  += label_requisition.width;
      requisition->height += label_requisition.height;
    }

  requisition->height = MAX (expander_size + 2 * expander_spacing, requisition->height);

  if (!interior_focus)
    requisition->height += 2 * focus_width + 2 * focus_pad;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_requisition;

      gtk_widget_size_request (bin->child, &child_requisition);

      if (!interior_focus)
	child_requisition.width += 2 * focus_width + 2 * focus_pad;

      requisition->width = MAX (requisition->width, child_requisition.width);
      requisition->height += child_requisition.height + priv->spacing;
    }

  requisition->width  += 2 * border_width;
  requisition->height += 2 * border_width + 2 * priv->spacing;
}

static void
e_expander_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  EExpander *expander;
  GtkBin *bin;
  EExpanderPrivate *priv;
  GtkRequisition child_requisition;
  gboolean child_visible = FALSE;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gint label_height;

  expander = E_EXPANDER (widget);
  bin = GTK_BIN (widget);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  child_requisition.width = 0;
  child_requisition.height = 0;
  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      child_visible = TRUE;
      gtk_widget_get_child_requisition (bin->child, &child_requisition);
    }

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x + border_width,
			    allocation->y + border_width,
                            MAX (allocation->width - 2 * border_width, 0),
                            MAX (allocation->height - 2 * border_width, 0));

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkAllocation label_allocation;
      GtkRequisition label_requisition;
      gboolean ltr;

      gtk_widget_get_child_requisition (priv->label_widget, &label_requisition);

      ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

      label_allocation.x = focus_width + focus_pad;
      if (ltr)
	label_allocation.x += expander_size + 2 * expander_spacing;
      label_allocation.y = priv->spacing + focus_width + focus_pad;

      label_allocation.width = MIN (label_requisition.width,
				    allocation->width - 2 * border_width -
				    expander_size - 2 * expander_spacing -
				    2 * focus_width - 2 * focus_pad);
      label_allocation.width = MAX (label_allocation.width, 1);

      label_allocation.height = MIN (label_requisition.height,
				     allocation->height - 2 * border_width -
				     2 * priv->spacing -
				     2 * focus_width - 2 * focus_pad -
				     child_requisition.height -
				     (child_visible ? priv->spacing : 0));
      label_allocation.height = MAX (label_allocation.height, 1);

      gtk_widget_size_allocate (priv->label_widget, &label_allocation);

      label_height = label_allocation.height;
    }
  else
    {
      label_height = 0;
    }

  if (child_visible)
    {
      GtkAllocation child_allocation;
      gint top_height;

      top_height = MAX (2 * expander_spacing + expander_size,
			label_height +
			(interior_focus ? 2 * focus_width + 2 * focus_pad : 0));

      child_allocation.x = 0;
      child_allocation.y = 2 * priv->spacing + top_height;

      if (!interior_focus)
	{
	  child_allocation.x += focus_width + focus_pad;
	  child_allocation.y += focus_width + focus_pad;
	}

      child_allocation.width = allocation->width - 2 * border_width -
			       (!interior_focus ? 2 * focus_width + 2 * focus_pad : 0);
      child_allocation.width = MAX (child_allocation.width, 1);

      child_allocation.height = allocation->height - top_height -
				2 * border_width -
				3 * priv->spacing -
				(!interior_focus ? 2 * focus_width + 2 * focus_pad : 0);
      child_allocation.height = MAX (child_allocation.height, 1);

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
e_expander_map (GtkWidget *widget)
{
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (widget);

  if (priv->label_widget)
    gtk_widget_map (priv->label_widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);
}

static GdkRectangle
get_expander_bounds (EExpander *expander)
{
  GtkWidget *widget;
  GtkBin *bin;
  EExpanderPrivate *priv;
  GdkRectangle bounds;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gboolean ltr;

  widget = GTK_WIDGET (expander);
  bin = GTK_BIN (expander);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  border_width = GTK_CONTAINER (expander)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;


  if (ltr)
    bounds.x = expander_spacing;
  else
    bounds.x = widget->allocation.width - 2 * border_width -
	       expander_spacing - expander_size;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkAllocation label_allocation;

      label_allocation = priv->label_widget->allocation;

      if (expander_size < label_allocation.height)
	bounds.y = label_allocation.y + (label_allocation.height - expander_size) / 2;
      else
	bounds.y = priv->spacing + expander_spacing;
    }
  else
    {
      bounds.y = priv->spacing + expander_spacing;
    }

  if (!interior_focus)
    {
      if (ltr)
	bounds.x += focus_width + focus_pad;
      else
	bounds.x -= focus_width + focus_pad;
      bounds.y += focus_width + focus_pad;
    }

  bounds.width = bounds.height = expander_size;

  return bounds;
}

static void
e_expander_paint (EExpander *expander)
{
  GtkWidget *widget;
  EExpanderPrivate *priv;
  gint x, y;
  GtkStateType state;
  GdkRectangle clip;

  widget = GTK_WIDGET (expander);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  clip = get_expander_bounds (expander);

  x = clip.x + clip.width / 2;
  y = clip.y + clip.height / 2;

  state = widget->state;
  if (state != GTK_STATE_PRELIGHT)
    state = GTK_STATE_NORMAL;

  gtk_paint_expander (widget->style,
		      widget->window,
		      state,
		      &clip,
		      widget,
		      "expander",
		      x,
		      y,
		      priv->expander_style);
}

static void
e_expander_paint_focus (EExpander  *expander,
			  GdkRectangle *area)
{
  GtkWidget *widget;
  EExpanderPrivate *priv;
  gint x, y, width, height;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gint expander_size;
  gint expander_spacing;
  gboolean ltr;

  widget = GTK_WIDGET (expander);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  if (interior_focus)
    {
      if (ltr)
	x = expander_spacing * 2 + expander_size;
      else
	x = 0;
      y = priv->spacing;

      width = height = 2 * focus_pad + 2 * focus_width;

      if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
	{
	  GtkAllocation label_allocation = priv->label_widget->allocation;

	  width  += label_allocation.width;
	  height += label_allocation.height;
	}
    }
  else
    {
      x = y = 0;
      width = widget->allocation.width - 2 * GTK_CONTAINER (widget)->border_width;
      height = widget->allocation.height - 2 * GTK_CONTAINER (widget)->border_width;
    }

  gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget),
		   area, widget, "expander",
		   x, y, width, height);
}

static gboolean
e_expander_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      EExpander *expander = E_EXPANDER (widget);
      EExpanderPrivate *priv;

      priv = E_EXPANDER_GET_PRIVATE (expander);

      e_expander_paint (expander);

      if (GTK_WIDGET_HAS_FOCUS (expander))
	e_expander_paint_focus (expander, &event->area);

      GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static gboolean
is_in_expander_panel (EExpander *expander,
		      GdkWindow   *window,
		      gint         x,
		      gint         y)
{
  GtkWidget *widget;
  GtkBin *bin;
  GdkRectangle area;
  gint border_width;

  widget = GTK_WIDGET (expander);
  bin = GTK_BIN (expander);

  border_width = GTK_CONTAINER (expander)->border_width;

  area = get_expander_bounds (expander);

  area.x = 0;
  area.width = widget->allocation.width;

  if (widget->window == window)
    {
      if (x >= area.x && x <= (area.x + area.width) &&
	  y >= area.y && y <= (area.y + area.height))
	return TRUE;
    }

  return FALSE;
}

static gboolean
e_expander_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
  EExpander *expander = E_EXPANDER (widget);
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (event->button == 1 && !priv->button_down)
    {
      if (is_in_expander_panel (expander, event->window, event->x, event->y))
	{
	  priv->button_down = TRUE;
	  return TRUE;
	}
    }

  return FALSE;
}

static gboolean
e_expander_button_release (GtkWidget      *widget,
                           GdkEventButton *event)
{
  EExpander *expander = E_EXPANDER (widget);
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (event->button == 1 && priv->button_down)
    {
      g_signal_emit_by_name (expander, "activate");
      
      priv->button_down = FALSE;
      return TRUE;
    }

  return FALSE;
}

static void
e_expander_maybe_prelight (EExpander *expander)
{
  GtkWidget *widget;
  EExpanderPrivate *priv;
  GtkStateType state = GTK_STATE_NORMAL;

  widget = GTK_WIDGET (expander);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (!priv->button_down)
    {
      gint x, y;

      gdk_window_get_pointer (widget->window, &x, &y, NULL);

      if (is_in_expander_panel (expander, widget->window, x, y))
	state = GTK_STATE_PRELIGHT;
    }

  gtk_widget_set_state (widget, state);
}

static gboolean
e_expander_motion_notify (GtkWidget      *widget,
			    GdkEventMotion *event)
{
  e_expander_maybe_prelight (E_EXPANDER (widget));

  return FALSE;
}

static gboolean
e_expander_enter_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  e_expander_maybe_prelight (E_EXPANDER (widget));

  return FALSE;
}

static gboolean
e_expander_leave_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  gtk_widget_set_state (widget, GTK_STATE_NORMAL);

  return FALSE;
}

static gboolean
focus_child_in (GtkWidget        *widget,
		GtkDirectionType  direction)
{
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

  if (!child)
    return FALSE;

  return gtk_widget_child_focus (child, direction);
}

static gboolean
e_expander_focus (GtkWidget        *widget,
		    GtkDirectionType  direction)
{
  EExpanderPrivate *priv;
  GtkWidget *old_focus_child;
  gboolean widget_is_focus;
  gboolean label_can_focus;

  priv = E_EXPANDER_GET_PRIVATE (widget);

  widget_is_focus = gtk_widget_is_focus (widget);
  old_focus_child = GTK_CONTAINER (widget)->focus_child;
  label_can_focus = priv->label_widget && GTK_WIDGET_CAN_FOCUS (priv->label_widget);

  if (old_focus_child && old_focus_child == priv->label_widget)
    {
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
	  gtk_widget_grab_focus (widget);
	  return TRUE;
	case GTK_DIR_DOWN:
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_RIGHT:
	  return focus_child_in (widget, direction);
	}
    }
  else if (old_focus_child)
    {
      if (gtk_widget_child_focus (old_focus_child, direction))
	return TRUE;

      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
	  if (label_can_focus)
	    gtk_widget_grab_focus (priv->label_widget);
	  else
	    gtk_widget_grab_focus (widget);
	  return TRUE;
	case GTK_DIR_DOWN:
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_RIGHT:
	  return FALSE;
	}
    }
  else if (widget_is_focus)
    {
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
	  return FALSE;
	case GTK_DIR_DOWN:
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_RIGHT:
	  if (label_can_focus)
	    {
	      gtk_widget_grab_focus (priv->label_widget);
	      return TRUE;
	    }

	  return focus_child_in (widget, direction);
	}
    }
  else
    {
      switch (direction)
	{
	case GTK_DIR_DOWN:
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_TAB_BACKWARD:
	  gtk_widget_grab_focus (widget);
	  return TRUE;
	case GTK_DIR_UP:
	case GTK_DIR_LEFT:
	case GTK_DIR_RIGHT:
	  if (!focus_child_in (widget, direction))
	    {
	      gtk_widget_grab_focus (widget);
	    }
	  return TRUE;
	}
    }

  g_assert_not_reached ();
  return FALSE;
}

static void
e_expander_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (parent_class)->add (container, widget);

  g_object_set (G_OBJECT (widget),
		"visible", E_EXPANDER_GET_PRIVATE (container)->expanded,
		NULL);
}

static void
e_expander_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  EExpander *expander = E_EXPANDER (container);

  if (E_EXPANDER_GET_PRIVATE (expander)->label_widget == widget)
    e_expander_set_label_widget (expander, NULL);
  else
    GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
e_expander_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (container);

  if (bin->child)
    (* callback) (bin->child, callback_data);

  if (priv->label_widget)
    (* callback) (priv->label_widget, callback_data);
}

static void
e_expander_activate (EExpander *expander)
{
  e_expander_set_expanded (expander,
			     !E_EXPANDER_GET_PRIVATE (expander)->expanded);
}

GtkWidget *
e_expander_new (const gchar *label)
{
  return g_object_new (E_TYPE_EXPANDER, "label", label, NULL);
}

GtkWidget *
e_expander_new_with_mnemonic (const gchar *label)
{
  return g_object_new (E_TYPE_EXPANDER,
		       "label", label,
		       "use_underline", TRUE,
		       NULL);
}

static gboolean
e_expander_animation_timeout (EExpander *expander)
{
  EExpanderPrivate *priv;
  GdkRectangle area;
  gboolean finish = FALSE;

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (GTK_WIDGET_REALIZED (expander))
    {
      area = get_expander_bounds (expander);
      gdk_window_invalidate_rect (GTK_WIDGET (expander)->window, &area, TRUE);
    }

  if (priv->expanded)
    {
      if (priv->expander_style == GTK_EXPANDER_COLLAPSED)
	{
	  priv->expander_style = GTK_EXPANDER_SEMI_EXPANDED;
	}
      else
	{
	  priv->expander_style = GTK_EXPANDER_EXPANDED;
	  finish = TRUE;
	}
    }
  else
    {
      if (priv->expander_style == GTK_EXPANDER_EXPANDED)
	{
	  priv->expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
	}
      else
	{
	  priv->expander_style = GTK_EXPANDER_COLLAPSED;
	  finish = TRUE;
	}
    }

  if (finish)
    {
      priv->animation_timeout = 0;
      g_object_set (G_OBJECT (GTK_BIN (expander)->child),
		    "visible", priv->expanded,
		    NULL);
    }

  return !finish;
}

static void
e_expander_start_animation (EExpander *expander)
{
  EExpanderPrivate *priv;

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (priv->animation_timeout)
    g_source_remove (priv->animation_timeout);

  priv->animation_timeout =
		g_timeout_add (50,
			       (GSourceFunc) e_expander_animation_timeout,
			       expander);
}

void
e_expander_set_expanded (EExpander *expander,
			   gboolean     expanded)
{
  EExpanderPrivate *priv;

  g_return_if_fail (E_IS_EXPANDER (expander));

  priv = E_EXPANDER_GET_PRIVATE (expander);

  expanded = expanded != FALSE;

  if (priv->expanded != expanded)
    {
      priv->expanded = expanded;

      if (GTK_WIDGET_VISIBLE (expander))
	e_expander_start_animation (expander);

      else if (GTK_BIN (expander)->child)
	{
	  priv->expander_style = expanded ? GTK_EXPANDER_EXPANDED :
					    GTK_EXPANDER_COLLAPSED;
	  g_object_set (G_OBJECT (GTK_BIN (expander)->child),
			"visible", priv->expanded,
			NULL);
	}

      gtk_widget_queue_resize (GTK_WIDGET (expander));

      g_object_notify (G_OBJECT (expander), "expanded");
    }
}

gboolean
e_expander_get_expanded (EExpander *expander)
{
  g_return_val_if_fail (E_IS_EXPANDER (expander), FALSE);

  return E_EXPANDER_GET_PRIVATE (expander)->expanded;
}

void
e_expander_set_spacing (EExpander *expander,
			  gint         spacing)
{
  EExpanderPrivate *priv;

  g_return_if_fail (E_IS_EXPANDER (expander));
  g_return_if_fail (spacing >= 0);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (expander));

      g_object_notify (G_OBJECT (expander), "spacing");
    }
}

gint
e_expander_get_spacing (EExpander *expander)
{
  g_return_val_if_fail (E_IS_EXPANDER (expander), 0);

  return E_EXPANDER_GET_PRIVATE (expander)->spacing;
}

void
e_expander_set_label (EExpander *expander,
			const gchar *label)
{
  g_return_if_fail (E_IS_EXPANDER (expander));

  if (!label)
    {
      e_expander_set_label_widget (expander, NULL);
    }
  else
    {
      GtkWidget *child;

      child = gtk_label_new (label);
      gtk_label_set_use_underline (GTK_LABEL (child),
				   E_EXPANDER_GET_PRIVATE (expander)->use_underline);
      gtk_widget_show (child);

      e_expander_set_label_widget (expander, child);
    }

  g_object_notify (G_OBJECT (expander), "label");
}

/**
 * e_expander_get_label:
 * @expander: a #EExpander
 *
 * If the expander's label widget is a #GtkLabel, return the
 * text in the label widget. (The frame will have a #GtkLabel
 * for the label widget if a non-%NULL argument was passed
 * to e_expander_new().)
 *
 * Return value: the text in the label, or %NULL if there
 *               was no label widget or the lable widget was not
 *               a #GtkLabel. This string is owned by GTK+ and
 *               must not be modified or freed.
 **/
G_CONST_RETURN char *
e_expander_get_label (EExpander *expander)
{
  EExpanderPrivate *priv;

  g_return_val_if_fail (E_IS_EXPANDER (expander), NULL);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (priv->label_widget && GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_text (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

void
e_expander_set_use_underline (EExpander *expander,
				gboolean     use_underline)
{
  EExpanderPrivate *priv;

  g_return_if_fail (E_IS_EXPANDER (expander));

  priv = E_EXPANDER_GET_PRIVATE (expander);

  use_underline = use_underline != FALSE;

  if (priv->use_underline != use_underline)
    {
      priv->use_underline = use_underline;

      if (priv->label_widget && GTK_IS_LABEL (priv->label_widget))
	gtk_label_set_use_underline (GTK_LABEL (priv->label_widget), use_underline);

      g_object_notify (G_OBJECT (expander), "use_underline");
    }
}

gboolean
e_expander_get_use_underline (EExpander *expander)
{
  g_return_val_if_fail (E_IS_EXPANDER (expander), FALSE);

  return E_EXPANDER_GET_PRIVATE (expander)->use_underline;
}

/**
 * e_expander_set_label_widget:
 * @expander: a #EExpander
 * @label_widget: the new label widget
 *
 * Set the label widget for the expander. This is the widget
 * that will appear embedded alongside the expander arrow.
 **/
void
e_expander_set_label_widget (EExpander *expander,
			       GtkWidget   *label_widget)
{
  EExpanderPrivate *priv;
  gboolean need_resize = FALSE;

  g_return_if_fail (E_IS_EXPANDER (expander));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || label_widget->parent == NULL);

  priv = E_EXPANDER_GET_PRIVATE (expander);

  if (priv->label_widget == label_widget)
    return;

  if (priv->label_widget)
    {
      need_resize = GTK_WIDGET_VISIBLE (priv->label_widget);
      gtk_widget_unparent (priv->label_widget);
    }

  priv->label_widget = label_widget;

  if (label_widget)
    {
      priv->label_widget = label_widget;
      gtk_widget_set_parent (label_widget, GTK_WIDGET (expander));
      need_resize |= GTK_WIDGET_VISIBLE (label_widget);
    }

  if (GTK_WIDGET_VISIBLE (expander) && need_resize)
    gtk_widget_queue_resize (GTK_WIDGET (expander));

  g_object_notify (G_OBJECT (expander), "label_widget");
}

/**
 * e_expander_get_label_widget:
 * @expander: a #EExpander
 *
 * Retrieves the label widget for the frame. See
 * e_expander_set_label_widget().
 *
 * Return value: the label widget, or %NULL if there is none.
 **/
GtkWidget *
e_expander_get_label_widget (EExpander *expander)
{
  g_return_val_if_fail (E_IS_EXPANDER (expander), NULL);

  return E_EXPANDER_GET_PRIVATE (expander)->label_widget;
}
