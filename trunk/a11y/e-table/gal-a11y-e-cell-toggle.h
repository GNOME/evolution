#ifndef __GAL_A11Y_E_CELL_TOGGLE_H__
#define __GAL_A11Y_E_CELL_TOGGLE_H__

#include <atk/atk.h>
#include "gal-a11y-e-cell.h"
#include "gal-a11y-e-cell-toggle.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GAL_A11Y_TYPE_E_CELL_TOGGLE            (gal_a11y_e_cell_toggle_get_type ())
#define GAL_A11Y_E_CELL_TOGGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE, GalA11yECellToggle))
#define GAL_A11Y_E_CELL_TOGGLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAL_A11Y_E_CELL_TOGGLE, GalA11yECellToggleClass))
#define GAL_A11Y_IS_E_CELL_TOGGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE))
#define GAL_A11Y_IS_E_CELL_TOGGLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAL_A11Y_TYPE_E_CELL_TOGGLE))
#define GAL_A11Y_E_CELL_TOGGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAL_A11Y_TYPE_E_CELL_TOGGLE, GalA11yECellToggleClass))

typedef struct _GalA11yECellToggle                  GalA11yECellToggle;
typedef struct _GalA11yECellToggleClass             GalA11yECellToggleClass;

struct _GalA11yECellToggle
{
  GalA11yECell parent;
  gint         model_id;
};

GType gal_a11y_e_cell_toggle_get_type (void);

struct _GalA11yECellToggleClass
{
  GalA11yECellClass parent_class;
};

AtkObject *gal_a11y_e_cell_toggle_new  (ETableItem *item,
                                        ECellView  *cell_view,
                                        AtkObject  *parent,
                                        int         model_col,
                                        int         view_col,
                                        int         row);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GAL_A11Y_E_CELL_TOGGLE_H__ */
