/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TEST_MODEL_H_
#define _E_TEST_MODEL_H_

#include "e-table-model.h"
#include <e-book.h>
#include <e-book-view.h>
#include <e-card.h>

#define E_TEST_MODEL_TYPE        (e_test_model_get_type ())
#define E_TEST_MODEL(o)          (GTK_CHECK_CAST ((o), E_TEST_MODEL_TYPE, ETestModel))
#define E_TEST_MODEL_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TEST_MODEL_TYPE, ETestModelClass))
#define E_IS_TEST_MODEL(o)       (GTK_CHECK_TYPE ((o), E_TEST_MODEL_TYPE))
#define E_IS_TEST_MODEL_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TEST_MODEL_TYPE))

/* Virtual Column list:
   0   Email
   1   Full Name
   2   Street
   3   Phone
*/
typedef struct _Address Address;
typedef enum _Rows Rows;

struct _Address {
	gchar *email;
	gchar *full_name;
	gchar *street;
	gchar *phone;
};

enum _Rows {
	EMAIL,
	FULL_NAME,
	STREET,
	PHONE,
	LAST_COL
};

typedef struct {
	ETableModel parent;

	EBook *book;

	EBookView *book_view;

	ECard **data;
	int data_count;

	char *uri;
} ETestModel;


typedef struct {
	ETableModelClass parent_class;
} ETestModelClass;


GtkType e_test_model_get_type (void);
ETableModel *e_test_model_new (char *uri);

void e_test_model_add_column (ETestModel *model, Address *newadd);

#endif /* _E_TEST_MODEL_H_ */
