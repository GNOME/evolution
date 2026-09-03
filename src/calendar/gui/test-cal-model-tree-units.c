#include "evolution-config.h"

#include <string.h>

#include "e-cal-model.h"

ECalModelComponent *
		_e_cal_model_test_add_component		(ECalModel *model,
								 const gchar *source_uid,
								 ICalComponent *icalcomp);
gboolean	_e_cal_model_test_modify_component		(ECalModel *model,
								 const gchar *source_uid,
								 ICalComponent *icalcomp);
gboolean	_e_cal_model_test_remove_component		(ECalModel *model,
								 const gchar *source_uid,
								 const gchar *uid,
								 const gchar *rid);
gboolean	_e_cal_model_test_reparent_component		(ECalModel *model,
								 ECalModelComponent *comp_data,
								 ECalModelComponent *new_parent);
guint		_e_cal_model_test_get_row_count			(ECalModel *model);
gchar *		_e_cal_model_test_get_tree_string		(ECalModel *model);

typedef struct _Fixture {
	ECalModel *model;
} Fixture;

static void
fixture_set_up (Fixture *fixture,
		gconstpointer user_data)
{
	fixture->model = g_object_new (E_TYPE_CAL_MODEL, "component-kind", I_CAL_VTODO_COMPONENT, NULL);
}

static void
fixture_tear_down (Fixture *fixture,
		   gconstpointer user_data)
{
	g_clear_object (&fixture->model);
}

static ICalComponent *
new_component (const gchar *uid,
	       const gchar *related_to_uid,
	       ICalParameterReltype reltype)
{
	ICalComponent *icalcomp;
	ICalProperty *prop;
	ICalParameter *param;

	icalcomp = i_cal_component_new (I_CAL_VTODO_COMPONENT);
	i_cal_component_set_uid (icalcomp, uid);

	if (related_to_uid) {
		prop = i_cal_property_new_relatedto (related_to_uid);

		if (reltype != I_CAL_RELTYPE_PARENT) {
			param = i_cal_parameter_new_reltype (reltype);
			i_cal_property_take_parameter (prop, param);
		}

		i_cal_component_take_property (icalcomp, prop);
	}

	return icalcomp;
}

static ECalModelComponent *
add_child (ECalModel *model,
	  const gchar *source_uid,
	  const gchar *uid,
	  const gchar *related_to_uid)
{
	return _e_cal_model_test_add_component (model, source_uid, new_component (uid, related_to_uid, I_CAL_RELTYPE_PARENT));
}

static void
test_two_level_tree (Fixture *fixture,
		     gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "child1", "parent");
	add_child (fixture->model, "cal-a", "child2", "parent");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent(child1,child2)");
	g_free (tree);
}

static void
test_three_level_chain_top_down (Fixture *fixture,
				 gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "grandparent", NULL);
	add_child (fixture->model, "cal-a", "parent", "grandparent");
	add_child (fixture->model, "cal-a", "child", "parent");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "grandparent(parent(child))");
	g_free (tree);
}

static void
test_three_level_chain_grandparent_last (Fixture *fixture,
					 gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "child", "parent");
	add_child (fixture->model, "cal-a", "parent", "grandparent");
	add_child (fixture->model, "cal-a", "grandparent", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "grandparent(parent(child))");
	g_free (tree);
}

static void
test_four_level_chain_scrambled (Fixture *fixture,
				 gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "child", "parent");
	add_child (fixture->model, "cal-a", "great-grandparent", NULL);
	add_child (fixture->model, "cal-a", "grandparent", "great-grandparent");
	add_child (fixture->model, "cal-a", "parent", "grandparent");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "great-grandparent(grandparent(parent(child)))");
	g_free (tree);
}

static void
test_multiple_children_out_of_order (Fixture *fixture,
				     gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "child2", "parent");
	add_child (fixture->model, "cal-a", "child1", "parent");
	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "child3", "parent");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent(child1,child2,child3)");
	g_free (tree);
}

static void
test_direct_cycle (Fixture *fixture,
		   gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "a", "b");
	add_child (fixture->model, "cal-a", "b", "a");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "a,b");
	g_free (tree);
}

static void
test_longer_cycle (Fixture *fixture,
		   gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "a", "b");
	add_child (fixture->model, "cal-a", "b", "c");
	add_child (fixture->model, "cal-a", "c", "a");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "b(a),c");
	g_free (tree);
}

static void
test_dangling_related_to (Fixture *fixture,
			  gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "orphan", "does-not-exist");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "orphan");
	g_free (tree);

	add_child (fixture->model, "cal-a", "does-not-exist", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "does-not-exist(orphan)");
	g_free (tree);
}

static void
test_cross_source_no_link (Fixture *fixture,
			   gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-b", "child", "parent");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "child,parent");
	g_free (tree);
}

static void
test_reltype_scope (Fixture *fixture,
		    gconstpointer user_data)
{
	gchar *tree;

	_e_cal_model_test_add_component (fixture->model, "cal-a", new_component ("parent", NULL, I_CAL_RELTYPE_PARENT));
	_e_cal_model_test_add_component (fixture->model, "cal-a", new_component ("not-a-child", "parent", I_CAL_RELTYPE_CHILD));
	_e_cal_model_test_add_component (fixture->model, "cal-a", new_component ("not-a-sibling", "parent", I_CAL_RELTYPE_SIBLING));
	_e_cal_model_test_add_component (fixture->model, "cal-a", new_component ("real-child", "parent", I_CAL_RELTYPE_PARENT));

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "not-a-child,not-a-sibling,parent(real-child)");
	g_free (tree);
}

static void
test_remove_promotes_children (Fixture *fixture,
			       gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "child1", "parent");
	add_child (fixture->model, "cal-a", "child2", "parent");

	_e_cal_model_test_remove_component (fixture->model, "cal-a", "parent", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "child1,child2");
	g_free (tree);
}

static void
test_remove_leaf_promotes_nothing (Fixture *fixture,
				   gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "child", "parent");

	_e_cal_model_test_remove_component (fixture->model, "cal-a", "child", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent");
	g_free (tree);
}

static void
test_remove_middle_promotes_only_direct_children (Fixture *fixture,
						  gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "grandparent", NULL);
	add_child (fixture->model, "cal-a", "parent", "grandparent");
	add_child (fixture->model, "cal-a", "child", "parent");

	_e_cal_model_test_remove_component (fixture->model, "cal-a", "parent", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "child,grandparent");
	g_free (tree);
}

static void
test_remove_then_readd_reclaims_children (Fixture *fixture,
					  gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "child", "parent");

	_e_cal_model_test_remove_component (fixture->model, "cal-a", "parent", NULL);
	add_child (fixture->model, "cal-a", "parent", NULL);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent(child)");
	g_free (tree);
}

static void
test_runtime_relinking (Fixture *fixture,
			gconstpointer user_data)
{
	gchar *tree;

	add_child (fixture->model, "cal-a", "parent1", NULL);
	add_child (fixture->model, "cal-a", "parent2", NULL);
	add_child (fixture->model, "cal-a", "child", "parent1");

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent1(child),parent2");
	g_free (tree);

	_e_cal_model_test_modify_component (fixture->model, "cal-a", new_component ("child", "parent2", I_CAL_RELTYPE_PARENT));

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "parent1,parent2(child)");
	g_free (tree);

	_e_cal_model_test_modify_component (fixture->model, "cal-a", new_component ("child", NULL, I_CAL_RELTYPE_PARENT));

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "child,parent1,parent2");
	g_free (tree);
}

static void
test_recurrence_exception_targets_master (Fixture *fixture,
					  gconstpointer user_data)
{
	ICalComponent *icalcomp;
	ICalTime *recurid;
	gchar *tree;

	add_child (fixture->model, "cal-a", "master", NULL);

	icalcomp = i_cal_component_new (I_CAL_VTODO_COMPONENT);
	i_cal_component_set_uid (icalcomp, "master");
	recurid = i_cal_time_new_from_string ("20260101T000000Z");
	i_cal_component_set_recurrenceid (icalcomp, recurid);
	i_cal_component_take_property (icalcomp, i_cal_property_new_relatedto ("master"));
	g_object_unref (recurid);

	_e_cal_model_test_add_component (fixture->model, "cal-a", icalcomp);

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "master(master)");
	g_free (tree);
}

static void
test_reparent_by_dnd (Fixture *fixture,
		      gconstpointer user_data)
{
	ECalModelComponent *comp_a, *comp_b, *comp_child;
	gchar *tree;

	comp_a = add_child (fixture->model, "cal-a", "a", NULL);
	comp_b = add_child (fixture->model, "cal-a", "b", NULL);
	comp_child = add_child (fixture->model, "cal-a", "child", "a");

	g_assert_false (e_cal_model_can_reparent_component (fixture->model, comp_child, comp_b));

	e_cal_model_set_reparent_by_dnd (fixture->model, TRUE);

	g_assert_true (e_cal_model_can_reparent_component (fixture->model, comp_child, comp_b));
	g_assert_false (e_cal_model_can_reparent_component (fixture->model, comp_child, comp_child));
	g_assert_false (e_cal_model_can_reparent_component (fixture->model, comp_a, comp_child));

	g_assert_true (_e_cal_model_test_reparent_component (fixture->model, comp_child, comp_b));

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "a,b(child)");
	g_free (tree);

	g_assert_true (_e_cal_model_test_reparent_component (fixture->model, comp_child, NULL));

	tree = _e_cal_model_test_get_tree_string (fixture->model);
	g_assert_cmpstr (tree, ==, "a,b,child");
	g_free (tree);
}

static ECalModelComponent *
add_from_ics (ECalModel *model,
	     const gchar *ics_text)
{
	return _e_cal_model_test_add_component (model, "cal-a", i_cal_component_new_from_string (ics_text));
}

static void
test_field_baseline (Fixture *fixture,
		     gconstpointer user_data)
{
	ECalModelComponent *comp_data;
	EDateEditValue *created, *lastmodified;
	ICalTime *tt, *expected_tt;
	gchar *categories, *summary;
	const gchar *uid;

	comp_data = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\n"
		"UID:task-1\r\n"
		"SUMMARY:Buy milk\r\n"
		"CATEGORIES:Errands,Personal\r\n"
		"CREATED:20260101T000000Z\r\n"
		"LAST-MODIFIED:20260102T000000Z\r\n"
		"END:VTODO\r\n");

	uid = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_UID);
	g_assert_cmpstr (uid, ==, "task-1");

	summary = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_SUMMARY);
	g_assert_cmpstr (summary, ==, "Buy milk");
	g_free (summary);

	categories = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_CATEGORIES);
	g_assert_cmpstr (categories, ==, "Errands,Personal");
	g_free (categories);

	created = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_CREATED);
	g_assert_nonnull (created);
	tt = e_date_edit_value_get_time (created);
	expected_tt = i_cal_time_new_from_string ("20260101T000000Z");
	g_assert_cmpint (i_cal_time_compare (tt, expected_tt), ==, 0);
	g_object_unref (expected_tt);
	e_date_edit_value_free (created);

	lastmodified = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_LASTMODIFIED);
	g_assert_nonnull (lastmodified);
	tt = e_date_edit_value_get_time (lastmodified);
	expected_tt = i_cal_time_new_from_string ("20260102T000000Z");
	g_assert_cmpint (i_cal_time_compare (tt, expected_tt), ==, 0);
	g_object_unref (expected_tt);
	e_date_edit_value_free (lastmodified);
}

static void
test_field_transparency (Fixture *fixture,
			 gconstpointer user_data)
{
	ECalModelComponent *comp_transparent, *comp_opaque, *comp_absent;
	gchar *value;

	comp_transparent = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-free\r\nTRANSP:TRANSPARENT\r\nEND:VEVENT\r\n");
	comp_opaque = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-busy\r\nTRANSP:OPAQUE\r\nEND:VEVENT\r\n");
	comp_absent = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-notransp\r\nEND:VEVENT\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_transparent, E_CAL_MODEL_FIELD_TRANSPARENCY);
	g_assert_cmpstr (value, ==, "Free");

	value = e_cal_model_get_field_value (fixture->model, comp_opaque, E_CAL_MODEL_FIELD_TRANSPARENCY);
	g_assert_cmpstr (value, ==, "Busy");

	value = e_cal_model_get_field_value (fixture->model, comp_absent, E_CAL_MODEL_FIELD_TRANSPARENCY);
	g_assert_null (value);
}

static void
test_field_location (Fixture *fixture,
		     gconstpointer user_data)
{
	ECalModelComponent *comp_event, *comp_task, *comp_no_location;
	gchar *value;

	comp_event = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-loc\r\nLOCATION:Room 42\r\nEND:VEVENT\r\n");
	comp_task = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-loc\r\nLOCATION:Home Office\r\nEND:VTODO\r\n");
	comp_no_location = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-noloc\r\nEND:VEVENT\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_event, E_CAL_MODEL_FIELD_LOCATION);
	g_assert_cmpstr (value, ==, "Room 42");

	value = e_cal_model_get_field_value (fixture->model, comp_task, E_CAL_MODEL_FIELD_LOCATION);
	g_assert_cmpstr (value, ==, "Home Office");

	value = e_cal_model_get_field_value (fixture->model, comp_no_location, E_CAL_MODEL_FIELD_LOCATION);
	g_assert_cmpstr (value, ==, "");
}

static void
test_field_status_events (Fixture *fixture,
			  gconstpointer user_data)
{
	ECalModelComponent *comp_confirmed, *comp_tentative, *comp_cancelled;
	gchar *value;

	comp_confirmed = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-confirmed\r\nSTATUS:CONFIRMED\r\nEND:VEVENT\r\n");
	comp_tentative = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-tentative\r\nSTATUS:TENTATIVE\r\nEND:VEVENT\r\n");
	comp_cancelled = add_from_ics (fixture->model,
		"BEGIN:VEVENT\r\nUID:event-cancelled\r\nSTATUS:CANCELLED\r\nEND:VEVENT\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_confirmed, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Confirmed");

	value = e_cal_model_get_field_value (fixture->model, comp_tentative, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Tentative");

	value = e_cal_model_get_field_value (fixture->model, comp_cancelled, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Cancelled");
}

static void
test_field_status_tasks (Fixture *fixture,
			 gconstpointer user_data)
{
	ECalModelComponent *comp_needs_action, *comp_in_process, *comp_completed, *comp_cancelled;
	gchar *value;

	comp_needs_action = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-needsaction\r\nSTATUS:NEEDS-ACTION\r\nEND:VTODO\r\n");
	comp_in_process = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-inprocess\r\nSTATUS:IN-PROCESS\r\nEND:VTODO\r\n");
	comp_completed = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-completed\r\nSTATUS:COMPLETED\r\nEND:VTODO\r\n");
	comp_cancelled = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-cancelled\r\nSTATUS:CANCELLED\r\nEND:VTODO\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_needs_action, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Needs Action");

	value = e_cal_model_get_field_value (fixture->model, comp_in_process, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "In Progress");

	value = e_cal_model_get_field_value (fixture->model, comp_completed, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Completed");

	value = e_cal_model_get_field_value (fixture->model, comp_cancelled, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Cancelled");
}

static void
test_field_status_journals (Fixture *fixture,
			    gconstpointer user_data)
{
	ECalModelComponent *comp_draft, *comp_final, *comp_cancelled;
	gchar *value;

	comp_draft = add_from_ics (fixture->model,
		"BEGIN:VJOURNAL\r\nUID:memo-draft\r\nSTATUS:DRAFT\r\nEND:VJOURNAL\r\n");
	comp_final = add_from_ics (fixture->model,
		"BEGIN:VJOURNAL\r\nUID:memo-final\r\nSTATUS:FINAL\r\nEND:VJOURNAL\r\n");
	comp_cancelled = add_from_ics (fixture->model,
		"BEGIN:VJOURNAL\r\nUID:memo-cancelled\r\nSTATUS:CANCELLED\r\nEND:VJOURNAL\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_draft, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Draft");

	value = e_cal_model_get_field_value (fixture->model, comp_final, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Final");

	value = e_cal_model_get_field_value (fixture->model, comp_cancelled, E_CAL_MODEL_FIELD_STATUS);
	g_assert_cmpstr (value, ==, "Cancelled");
}

static void
test_field_geo (Fixture *fixture,
	       gconstpointer user_data)
{
	ECalModelComponent *comp_data;
	gchar *value;

	comp_data = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-geo\r\nGEO:45.436845;-125.862501\r\nEND:VTODO\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_data, E_CAL_MODEL_FIELD_GEO);
	g_assert_cmpstr (value, ==, "45.4368 N, 125.863 W");
}

static void
test_field_priority (Fixture *fixture,
		     gconstpointer user_data)
{
	ECalModelComponent *comp_high, *comp_normal, *comp_low, *comp_none;
	gchar *value;

	comp_high = add_from_ics (fixture->model, "BEGIN:VTODO\r\nUID:task-high\r\nPRIORITY:1\r\nEND:VTODO\r\n");
	comp_normal = add_from_ics (fixture->model, "BEGIN:VTODO\r\nUID:task-normal\r\nPRIORITY:5\r\nEND:VTODO\r\n");
	comp_low = add_from_ics (fixture->model, "BEGIN:VTODO\r\nUID:task-low\r\nPRIORITY:9\r\nEND:VTODO\r\n");
	comp_none = add_from_ics (fixture->model, "BEGIN:VTODO\r\nUID:task-nopriority\r\nEND:VTODO\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_high, E_CAL_MODEL_FIELD_PRIORITY);
	g_assert_cmpstr (value, ==, "High");

	value = e_cal_model_get_field_value (fixture->model, comp_normal, E_CAL_MODEL_FIELD_PRIORITY);
	g_assert_cmpstr (value, ==, "Normal");

	value = e_cal_model_get_field_value (fixture->model, comp_low, E_CAL_MODEL_FIELD_PRIORITY);
	g_assert_cmpstr (value, ==, "Low");

	value = e_cal_model_get_field_value (fixture->model, comp_none, E_CAL_MODEL_FIELD_PRIORITY);
	g_assert_cmpstr (value, ==, "");
}

static void
test_field_strikeout (Fixture *fixture,
		      gconstpointer user_data)
{
	ECalModelComponent *comp_complete, *comp_incomplete;
	gpointer value;

	comp_complete = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-strike-complete\r\nSTATUS:COMPLETED\r\nEND:VTODO\r\n");
	comp_incomplete = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-strike-incomplete\r\nSTATUS:NEEDS-ACTION\r\nEND:VTODO\r\n");

	value = e_cal_model_get_field_value (fixture->model, comp_complete, E_CAL_MODEL_FIELD_STRIKEOUT);
	g_assert_true (GPOINTER_TO_INT (value));

	value = e_cal_model_get_field_value (fixture->model, comp_incomplete, E_CAL_MODEL_FIELD_STRIKEOUT);
	g_assert_false (GPOINTER_TO_INT (value));
}

static void
test_field_color_coding (Fixture *fixture,
			 gconstpointer user_data)
{
	ECalModelComponent *comp_overdue;
	const gchar *color;

	e_cal_model_set_color_overdue (fixture->model, "#ff0000");
	e_cal_model_set_color_due_today (fixture->model, "#1e90ff");

	comp_overdue = add_from_ics (fixture->model,
		"BEGIN:VTODO\r\nUID:task-overdue\r\nDUE:20200101T000000Z\r\nEND:VTODO\r\n");

	color = e_cal_model_get_color_for_component (fixture->model, comp_overdue);
	g_assert_cmpstr (color, ==, "#ff0000");
}

static void
before_rebuild_capture_row3_uid_cb (EVirtualTreeModel *vtree_model,
				    gpointer user_data)
{
	gchar **out_uid = user_data;
	GObject *row_obj;

	g_clear_pointer (out_uid, g_free);

	row_obj = e_virtual_tree_model_dup_row (vtree_model, 3);
	if (row_obj) {
		*out_uid = g_strdup (i_cal_component_get_uid (E_CAL_MODEL_COMPONENT (row_obj)->icalcomp));
		g_object_unref (row_obj);
	}
}

static void
test_sort_before_rebuild_sees_old_order (Fixture *fixture,
					 gconstpointer user_data)
{
	static const gchar *summaries[] = { "10", "11", "12", "13", "14" };
	ECalModelSortColumn sort_columns[1];
	gchar *captured_uid = NULL;
	GObject *row_obj;
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (summaries); ii++) {
		ICalComponent *icalcomp = new_component (summaries[ii], NULL, I_CAL_RELTYPE_PARENT);
		i_cal_component_set_summary (icalcomp, summaries[ii]);
		_e_cal_model_test_add_component (fixture->model, "cal-a", icalcomp);
	}

	sort_columns[0].field = E_CAL_MODEL_FIELD_SUMMARY;
	sort_columns[0].order = GTK_SORT_ASCENDING;
	e_cal_model_set_sort_columns (fixture->model, sort_columns, 1);

	/* Ascending order is 10,11,12,13,14 -- row 3 is "13". */
	row_obj = e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (fixture->model), 3);
	g_assert_nonnull (row_obj);
	g_assert_cmpstr (i_cal_component_get_uid (E_CAL_MODEL_COMPONENT (row_obj)->icalcomp), ==, "13");
	g_object_unref (row_obj);

	g_signal_connect (fixture->model, "before-rebuild",
		G_CALLBACK (before_rebuild_capture_row3_uid_cb), &captured_uid);

	sort_columns[0].order = GTK_SORT_DESCENDING;
	e_cal_model_set_sort_columns (fixture->model, sort_columns, 1);

	g_signal_handlers_disconnect_by_func (fixture->model, before_rebuild_capture_row3_uid_cb, &captured_uid);

	/* The before-rebuild signal must still see the pre-sort row at index 3
	 * ("13"), not the already-resorted data. */
	g_assert_cmpstr (captured_uid, ==, "13");
	g_free (captured_uid);

	/* Descending order is 14,13,12,11,10 -- "13" is now at row 1. */
	row_obj = e_virtual_tree_model_dup_row (E_VIRTUAL_TREE_MODEL (fixture->model), 1);
	g_assert_nonnull (row_obj);
	g_assert_cmpstr (i_cal_component_get_uid (E_CAL_MODEL_COMPONENT (row_obj)->icalcomp), ==, "13");
	g_object_unref (row_obj);
}

gint
main (gint argc,
     gchar **argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add ("/ECalModel/Tree/TwoLevelTree", Fixture, NULL, fixture_set_up, test_two_level_tree, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/ThreeLevelChainTopDown", Fixture, NULL, fixture_set_up, test_three_level_chain_top_down, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/ThreeLevelChainGrandparentLast", Fixture, NULL, fixture_set_up, test_three_level_chain_grandparent_last, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/FourLevelChainScrambled", Fixture, NULL, fixture_set_up, test_four_level_chain_scrambled, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/MultipleChildrenOutOfOrder", Fixture, NULL, fixture_set_up, test_multiple_children_out_of_order, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/DirectCycle", Fixture, NULL, fixture_set_up, test_direct_cycle, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/LongerCycle", Fixture, NULL, fixture_set_up, test_longer_cycle, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/DanglingRelatedTo", Fixture, NULL, fixture_set_up, test_dangling_related_to, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/CrossSourceNoLink", Fixture, NULL, fixture_set_up, test_cross_source_no_link, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/ReltypeScope", Fixture, NULL, fixture_set_up, test_reltype_scope, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RemovePromotesChildren", Fixture, NULL, fixture_set_up, test_remove_promotes_children, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RemoveLeafPromotesNothing", Fixture, NULL, fixture_set_up, test_remove_leaf_promotes_nothing, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RemoveMiddlePromotesOnlyDirectChildren", Fixture, NULL, fixture_set_up, test_remove_middle_promotes_only_direct_children, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RemoveThenReaddReclaimsChildren", Fixture, NULL, fixture_set_up, test_remove_then_readd_reclaims_children, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RuntimeRelinking", Fixture, NULL, fixture_set_up, test_runtime_relinking, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/RecurrenceExceptionTargetsMaster", Fixture, NULL, fixture_set_up, test_recurrence_exception_targets_master, fixture_tear_down);
	g_test_add ("/ECalModel/Tree/ReparentByDnd", Fixture, NULL, fixture_set_up, test_reparent_by_dnd, fixture_tear_down);

	g_test_add ("/ECalModel/Field/Baseline", Fixture, NULL, fixture_set_up, test_field_baseline, fixture_tear_down);
	g_test_add ("/ECalModel/Field/Transparency", Fixture, NULL, fixture_set_up, test_field_transparency, fixture_tear_down);
	g_test_add ("/ECalModel/Field/Location", Fixture, NULL, fixture_set_up, test_field_location, fixture_tear_down);
	g_test_add ("/ECalModel/Field/StatusEvents", Fixture, NULL, fixture_set_up, test_field_status_events, fixture_tear_down);
	g_test_add ("/ECalModel/Field/StatusTasks", Fixture, NULL, fixture_set_up, test_field_status_tasks, fixture_tear_down);
	g_test_add ("/ECalModel/Field/StatusJournals", Fixture, NULL, fixture_set_up, test_field_status_journals, fixture_tear_down);
	g_test_add ("/ECalModel/Field/Geo", Fixture, NULL, fixture_set_up, test_field_geo, fixture_tear_down);
	g_test_add ("/ECalModel/Field/Priority", Fixture, NULL, fixture_set_up, test_field_priority, fixture_tear_down);
	g_test_add ("/ECalModel/Field/Strikeout", Fixture, NULL, fixture_set_up, test_field_strikeout, fixture_tear_down);
	g_test_add ("/ECalModel/Field/ColorCoding", Fixture, NULL, fixture_set_up, test_field_color_coding, fixture_tear_down);

	g_test_add ("/ECalModel/Sort/BeforeRebuildSeesOldOrder", Fixture, NULL, fixture_set_up, test_sort_before_rebuild_sees_old_order, fixture_tear_down);

	return g_test_run ();
}
