/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_SORTER_H_
#define _E_SORTER_H_

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_SORTER_TYPE        (e_sorter_get_type ())
#define E_SORTER(o)          (GTK_CHECK_CAST ((o), E_SORTER_TYPE, ESorter))
#define E_SORTER_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_SORTER_TYPE, ESorterClass))
#define E_IS_SORTER(o)       (GTK_CHECK_TYPE ((o), E_SORTER_TYPE))
#define E_IS_SORTER_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_SORTER_TYPE))

typedef struct {
	GtkObject base;
} ESorter;

typedef struct {
	GtkObjectClass parent_class;
	gint      (*model_to_sorted)            (ESorter    *sorter,
						 int         row);
	gint      (*sorted_to_model)            (ESorter    *sorter,
						 int         row);
	  
	void      (*get_model_to_sorted_array)  (ESorter    *sorter,
						 int       **array,
						 int        *count);
	void      (*get_sorted_to_model_array)  (ESorter    *sorter,
						 int       **array,
						 int        *count);
	  
	gboolean  (*needs_sorting)              (ESorter    *sorter);
} ESorterClass;

GtkType   e_sorter_get_type                   (void);
ESorter  *e_sorter_new                        (void);

gint      e_sorter_model_to_sorted            (ESorter  *sorter,
					       int       row);
gint      e_sorter_sorted_to_model            (ESorter  *sorter,
					       int       row);

void      e_sorter_get_model_to_sorted_array  (ESorter  *sorter,
					       int     **array,
					       int      *count);
void      e_sorter_get_sorted_to_model_array  (ESorter  *sorter,
					       int     **array,
					       int      *count);

gboolean  e_sorter_needs_sorting              (ESorter  *sorter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SORTER_H_ */
