#ifndef __E_SUMMARY_CALLBACKS_H__
#define __E_SUMMARY_CALLBACKS_H__

#include "e-summary.h"

void embed_service (GtkWidget *widget,
		    ESummary *esummary);
void new_mail (GtkWidget *widget,
	       ESummary *esummary);
void configure_summary (GtkWidget *widget,
			ESummary *esummary);
#endif
