

/*
 * DO NOT replace type casting of WebKit types by GLib macros unless
 * you know what you are doing (I do).
 *
 * Probably due to bugs in WebKitGtk+ DOM bindings these macros will
 * produce runtime warnings, but the objects and class hierarchy ARE VALID.
 *
 * This mostly affects only WebKitDOMText, which is subclass
 * of WebKitDOMNode, but the text nodes are rarely created as instances of
 * WebKitDOMText. To make sure that you really can cast WebKitDOMNode to
 * WebKitDOMText, check whether webkit_dom_node_get_node_type() == 3
 * (3 is "text" node type). WebKitDOMNode is just a thin wrapper around
 * WebKit's internal WebCore objects. Using get_node_type() is evaluated
 * against properties of these internal object.
 */



static void
normalize (WebKitDOMNode *node)
{
	WebKitDOMNodeList *children;
	gulong ii;

	/* Standard normalization */
	webkit_dom_node_normalize (node);

	children = webkit_dom_node_get_child_nodes (node);

	ii = 0;
	while (ii < webkit_dom_node_list_get_length (children)) {
		WebKitDOMNode *child, *sibling;
		gchar *tag_name, *sibling_tag_name;

		child = webkit_dom_node_list_item (children, ii);

		/* We are interested only in nodes representing HTML
		 * elements */
		if (webkit_dom_node_get_node_type (child) != 1) {
			ii++;
			continue;
		}

		sibling = webkit_dom_node_get_next_sibling (child);

		/* If sibling node is not an element, then skip the current
		 * element and the sibling node as well */
		if (webkit_dom_node_get_node_type (sibling) != 1) {
			ii += 2;
			continue;
		}

		/* Recursively normalize the child element */
		normalize (child);

		tag_name = webkit_dom_element_get_tag_name (
				WEBKIT_DOM_ELEMENT (child));
		sibling_tag_name = webkit_dom_element_get_tag_name (
				WEBKIT_DOM_ELEMENT (sibling));

		if (g_strcmp0 (tag_name, sibling_tag_name) == 0) {
			gchar *str1, *str2, *inner_html;

			str1 = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (child));
			str2 = webkit_dom_html_element_get_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (sibling));
			inner_html = g_strconcat (str1, str2, NULL);
			webkit_dom_html_element_set_inner_html (
				WEBKIT_DOM_HTML_ELEMENT (child), inner_html, NULL);

			g_free (str1);
			g_free (str2);
			g_free (inner_html);

			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (sibling),
				sibling, NULL);
		}

		ii++;
	}
}

static void
remove_format (EEditorSelection *selection,
	       const gchar *format_tag)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMNode *start_node, *end_node;
	WebKitDOMElement *common_ancestor;

	document = webkit_web_view_get_dom_document (selection->priv->webview);
	range = editor_selection_get_current_range (selection);

	start_node = webkit_dom_range_get_start_container (range, NULL);
	end_node = webkit_dom_range_get_end_container (range, NULL);

	common_ancestor = webkit_dom_node_get_parent_element (
				webkit_dom_node_get_parent_node (
					webkit_dom_range_get_common_ancestor_container (
						range, NULL)));

	/* Cool! The selection is all within one node */
	if (start_node == end_node) {
		WebKitDOMElement *element;
		WebKitDOMNode *node = start_node;
		WebKitDOMNodeList *children;
		gchar *wrapper_tag_name;

		if (webkit_dom_node_get_node_type (start_node) != 3) {
			/* XXX Is it possible for selection to start somewhere
			 * else then in a text node? If yes, what should we
			 * do about it? */
			return;
		}

		/* Split <b>|blabla SELECTED TEXT bla|</b> to
		 * <b>|blabla |SELECTED TEXT| bla|</b> (| indicates node) */
		node = (WebKitDOMNode *) webkit_dom_text_split_text (
				(WebKitDOMText *) (node),
			webkit_dom_range_get_start_offset (range, NULL), NULL);
		webkit_dom_text_split_text  (
				(WebKitDOMText *) node,
				webkit_dom_range_get_end_offset (range, NULL),
				NULL);

		element = webkit_dom_node_get_parent_element (node);
		children = webkit_dom_node_get_child_nodes (WEBKIT_DOM_NODE (element));
		wrapper_tag_name = webkit_dom_element_get_tag_name (element);

		while (webkit_dom_node_list_get_length (children) > 0) {
			WebKitDOMNode *child;

			child = webkit_dom_node_list_item (children, 0);

			if (child != node) {
				WebKitDOMElement *wrapper;
				wrapper = webkit_dom_document_create_element (
					document, wrapper_tag_name, NULL);

				webkit_dom_node_append_child (
					WEBKIT_DOM_NODE (wrapper), child, NULL);

				child = WEBKIT_DOM_NODE (wrapper);
			}

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (element)),
				child, WEBKIT_DOM_NODE (element), NULL);
		}

		/* Remove the now empty container */
		/*
		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (
				WEBKIT_DOM_NODE (element)),
			WEBKIT_DOM_NODE (element), NULL);
		*/

		g_free (wrapper_tag_name);
	}

	normalize (WEBKIT_DOM_NODE (common_ancestor));
}

static void
apply_format (EEditorSelection *editor_selection,
	      const gchar *format_tag)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	WebKitDOMNode *selection, *node;
	WebKitDOMNode *new_parent;
	gint format_tag_len = strlen (format_tag);
	gboolean prev_sibling_match, next_sibling_match;

	prev_sibling_match = FALSE;
	next_sibling_match = FALSE;

	document = webkit_web_view_get_dom_document (editor_selection->priv->webview);
	range = editor_selection_get_current_range (editor_selection);

	if (webkit_dom_range_get_start_offset (range, NULL) != 0) {
		node = webkit_dom_range_get_start_container (range, NULL);
		selection = (WebKitDOMNode*) webkit_dom_text_split_text (
			(WebKitDOMText *) node,
			webkit_dom_range_get_start_offset (range, NULL), NULL);
	} else {
		selection = webkit_dom_range_get_start_container (range, NULL);
	}

	webkit_dom_text_split_text ((WebKitDOMText *) selection,
			webkit_dom_range_get_end_offset (range, NULL), NULL);

	/* The split above might have produced an empty text node
	 * (for example splitting "TEXT" on offset 4 will produce
	 * "TEXT" and "" nodes), so remove it */
	node = webkit_dom_node_get_next_sibling (selection);
	if (webkit_dom_node_get_node_type (node) == 3) {
		gchar *content;

		content = webkit_dom_node_get_text_content (node);
		if (!content || (strlen (content) == 0)) {
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (node),
				node, NULL);
		}

		g_free (content);
	}

	/* Check whether previous sibling is an element and whether it is <format_tag> */
	node = webkit_dom_node_get_previous_sibling (selection);
	if (node && (webkit_dom_node_get_node_type (node) == 1)) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (
				(WebKitDOMElement *) node);
		prev_sibling_match = ((format_tag_len == strlen (tag_name)) &&
				      (g_ascii_strncasecmp (
						format_tag, tag_name,
						format_tag_len) == 0));
		g_free (tag_name);
	}

	/* Check whether next sibling is an element and whether it is <format_tag> */
	node = webkit_dom_node_get_next_sibling (selection);
	if (node && (webkit_dom_node_get_node_type (node) == 1)) {
		gchar *tag_name;

		tag_name = webkit_dom_element_get_tag_name (
				(WebKitDOMElement *) node);
		next_sibling_match = ((format_tag_len == strlen (tag_name)) &&
				      (g_ascii_strncasecmp (
					      	format_tag, tag_name,
						format_tag_len) == 0));
		g_free (tag_name);
	}

	/* Merge selection and next sibling to the orevious sibling */
	if (prev_sibling_match && next_sibling_match) {
		WebKitDOMNode *next_sibling, *child;

		new_parent = webkit_dom_node_get_previous_sibling (selection);
		next_sibling = webkit_dom_node_get_next_sibling (selection);

		/* Append selection to the new parent */
		webkit_dom_node_append_child (new_parent, selection, NULL);

		/* Append all children of next sibling to the new parent */
		while ((child = webkit_dom_node_get_first_child (next_sibling)) != NULL) {
			webkit_dom_node_append_child (new_parent, child, NULL);
		}
		webkit_dom_node_remove_child (
			webkit_dom_node_get_parent_node (next_sibling),
			next_sibling, NULL);

	/* Merge selection to the previous sibling */
	} else if (prev_sibling_match && !next_sibling_match) {
		new_parent = webkit_dom_node_get_previous_sibling (selection);
		webkit_dom_node_append_child (new_parent, selection, NULL);

	/* Merge selection to the next sibling */
	} else if (!prev_sibling_match && next_sibling_match) {
		new_parent = webkit_dom_node_get_next_sibling (selection);
		webkit_dom_node_insert_before (
			new_parent, selection,
			webkit_dom_node_get_first_child (new_parent), NULL);

	/* Just wrap the selection to <tag_name> */
	} else {
		new_parent = (WebKitDOMNode *)
				webkit_dom_document_create_element (
					document, format_tag, NULL);
		webkit_dom_range_surround_contents (range, new_parent, NULL);
	}

	webkit_dom_node_normalize (
		(WebKitDOMNode *) webkit_dom_node_get_parent_element (new_parent));
}


 
