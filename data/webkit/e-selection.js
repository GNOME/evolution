/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2019 Red Hat (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

'use strict';

/* semi-convention: private functions start with lower-case letter,
   public functions start with upper-case letter. */

var EvoSelection = {
};

/* The node path is described as an array of child indexes between parent
   and the childNode (in this order). */
EvoSelection.GetChildPath = function(parent, childNode)
{
	if (!childNode) {
		return null;
	}

	var array = [], node;

	if (childNode.nodeType == childNode.TEXT_NODE) {
		childNode = childNode.parentElement;
	}

	for (node = childNode; node && !(node === parent); node = node.parentElement) {
		var child, index = 0;

		for (child = node.previousElementSibling; child; child = child.previousElementSibling) {
			index++;
		}

		array[array.length] = index;
	}

	return array.reverse();
}

/* Finds the element (not node) referenced by the 'path', which had been created
   by EvoSelection.GetChildPath(). There should be used the same 'parent' element
   in both calls. */
EvoSelection.FindElementByPath = function(parent, path)
{
	if (!parent || !path) {
		return null;
	}

	var ii, child = parent;

	for (ii = 0; ii < path.length; ii++) {
		var idx = path[ii];

		if (idx < 0 || idx >= child.children.length) {
			throw "EvoSelection.FindElementByPath:: Index '" + idx + "' out of range '" + child.children.length + "'";
		}

		child = child.children.item(idx);
	}

	return child;
}

/* This is when the text nodes are split, then the text length of
   the previous text node influences offset of the next node. */
EvoSelection.GetOverallTextOffset = function(node)
{
	if (!node) {
		return 0;
	}

	var text_offset = 0, sibling;

	for (sibling = node.previousSibling; sibling; sibling = sibling.previousSibling) {
		if (sibling.nodeType == sibling.TEXT_NODE) {
			text_offset += sibling.textContent.length;
		}
	}

	return text_offset;
}

/* Traverses direct text nodes under element until it reaches the first within
   the textOffset. */
EvoSelection.GetTextOffsetNode = function(element, textOffset)
{
	if (!element) {
		return null;
	}

	var node, adept = null;

	for (node = element.firstChild; node; node = node.nextSibling) {
		if (node.nodeType == node.TEXT_NODE) {
			var txt_len = node.textContent.length;

			if (textOffset > txt_len) {
				textOffset -= txt_len;
				adept = node;
			} else {
				break;
			}
		}
	}

	return node ? node : (adept ? adept : element);
}

/* Returns an object, where the current selection in the doc is stored */
EvoSelection.Store = function(doc)
{
	if (!doc || !doc.getSelection()) {
		return null;
	}

	var selection = {}, sel = doc.getSelection();

	selection.anchorElem = sel.anchorNode ? EvoSelection.GetChildPath(doc.body, sel.anchorNode) : [];
	selection.anchorOffset = sel.anchorOffset + EvoSelection.GetOverallTextOffset(sel.anchorNode);

	if (sel.anchorNode && sel.anchorNode.nodeType == sel.anchorNode.ELEMENT_NODE) {
		selection.anchorIsElement = true;
	}

	if (!sel.isCollapsed) {
		selection.focusElem = EvoSelection.GetChildPath(doc.body, sel.focusNode);
		selection.focusOffset = sel.focusOffset + EvoSelection.GetOverallTextOffset(sel.focusNode);

		if (sel.focusNode && sel.focusNode.nodeType == sel.focusNode.ELEMENT_NODE) {
			selection.focusIsElement = true;
		}
	}

	return selection;
}

/* Restores selection in the doc according to the information stored in 'selection',
   obtained by EvoSelection.Store(). */
EvoSelection.Restore = function(doc, selection)
{
	if (!doc || !selection || !doc.getSelection()) {
		return;
	}

	var anchorNode, anchorOffset, focusNode, focusOffset;

	anchorNode = EvoSelection.FindElementByPath(doc.body, selection.anchorElem);
	anchorOffset = selection.anchorOffset;

	if (!anchorNode) {
		return;
	}

	if (!anchorOffset) {
		anchorOffset = 0;
	}

	if (!selection.anchorIsElement) {
		anchorNode = EvoSelection.GetTextOffsetNode(anchorNode, anchorOffset);
		anchorOffset -= EvoSelection.GetOverallTextOffset(anchorNode);
	}

	focusNode = EvoSelection.FindElementByPath(doc.body, selection.focusElem);
	focusOffset = selection.focusOffset;

	if (focusNode) {
		if (!selection.focusIsElement) {
			focusNode = EvoSelection.GetTextOffsetNode(focusNode, focusOffset);
			focusOffset -= EvoSelection.GetOverallTextOffset(focusNode);
		}
	}

	if (focusNode)
		doc.getSelection().setBaseAndExtent(anchorNode, anchorOffset, focusNode, focusOffset);
	else
		doc.getSelection().setPosition(anchorNode, anchorOffset);
}

/* Encodes selection information to a string */
EvoSelection.ToString = function(selection)
{
	if (!selection) {
		return "";
	}

	var utils = {
		arrayToString : function(array) {
			var ii, str = "[";

			if (!array) {
				return str + "]";
			}

			for (ii = 0; ii < array.length; ii++) {
				if (ii) {
					str += ",";
				}
				str += array[ii];
			}

			return str + "]";
		}
	};

	var str = "", anchorElem, anchorOffset, focusElem, focusOffset;

	anchorElem = selection.anchorElem;
	anchorOffset = selection.anchorOffset;
	focusElem = selection.focusElem;
	focusOffset = selection.focusOffset;

	str += "anchorElem=" + utils.arrayToString(anchorElem);
	str += " anchorOffset=" + (anchorOffset ? anchorOffset : 0);

	if (selection.anchorIsElement) {
		str += " anchorIsElement=1";
	}

	if (focusElem) {
		str += " focusElem=" + utils.arrayToString(focusElem);
		str += " focusOffset=" + (focusOffset ? focusOffset : 0);

		if (selection.focusIsElement) {
			str += " focusIsElement=1";
		}
	}

	return str;
}

/* Decodes selection information from a string */
EvoSelection.FromString = function(str)
{
	if (!str) {
		return null;
	}

	var utils = {
		arrayFromString : function(str) {
			if (!str || !str.startsWith("[") || !str.endsWith("]")) {
				return null;
			}

			var ii, array;

			array = str.substr(1, str.length - 2).split(",");

			if (!array) {
				return null;
			}

			if (array.length == 1 && array[0] == "") {
				array.length = 0;
			} else {
				for (ii = 0; ii < array.length; ii++) {
					array[ii] = parseInt(array[ii], 10);

					if (!Number.isInteger(array[ii])) {
						return null;
					}
				}
			}

			return array;
		}
	};

	var selection = {}, ii, split_str;

	split_str = str.split(" ");

	if (!split_str || !split_str.length) {
		return null;
	}

	for (ii = 0; ii < split_str.length; ii++) {
		var name;

		name = "anchorElem";
		if (split_str[ii].startsWith(name + "=")) {
			selection[name] = utils.arrayFromString(split_str[ii].slice(name.length + 1));
			continue;
		}

		name = "anchorOffset";
		if (split_str[ii].startsWith(name + "=")) {
			var value;

			value = parseInt(split_str[ii].slice(name.length + 1), 10);
			if (Number.isInteger(value)) {
				selection[name] = value;
			}
			continue;
		}

		name = "anchorIsElement";
		if (split_str[ii].startsWith(name + "=")) {
			var value;

			value = parseInt(split_str[ii].slice(name.length + 1), 10);
			if (Number.isInteger(value) && value == 1) {
				selection[name] = true;
			}
			continue;
		}

		name = "focusElem";
		if (split_str[ii].startsWith(name + "=")) {
			selection[name] = utils.arrayFromString(split_str[ii].slice(name.length + 1));
			continue;
		}

		name = "focusOffset";
		if (split_str[ii].startsWith(name + "=")) {
			var value;

			value = parseInt(split_str[ii].slice(name.length + 1), 10);
			if (Number.isInteger(value)) {
				selection[name] = value;
			}
		}

		name = "focusIsElement";
		if (split_str[ii].startsWith(name + "=")) {
			var value;

			value = parseInt(split_str[ii].slice(name.length + 1), 10);
			if (Number.isInteger(value) && value == 1) {
				selection[name] = true;
			}
			continue;
		}
	}

	/* The "anchorElem" is required, the rest is optional */
	if (!selection.anchorElem)
		return null;

	return selection;
}

/* The so-called updater object has several methods to work with when removing
   elements from the structure, which tries to preserve selection in the new
   document structure. The methods are:

   beforeRemove(node) - called before going to remove the 'node'
   afterRemove(newNode) - with what the 'node' from beforeRemove() had been replaced
   restore() - called at the end, to restore the selection
 */
EvoSelection.CreateUpdaterObject = function()
{
	var obj = {
		selectionBefore : null,
		selectionAnchorNode : null,
		selectionAnchorOffset : -1,
		selectionFocusNode : null,
		selectionFocusOffset : -1,
		changeAnchor : false,
		changeFocus : false,

		beforeRemove : function(node) {
			this.changeAnchor = false;
			this.changeFocus = false;

			if (this.selectionAnchorNode) {
				this.changeAnchor = node === this.selectionAnchorNode ||
					(this.selectionAnchorNode.noteType == this.selectionAnchorNode.TEXT_NODE &&
					 this.selectionAnchorNode.parentElement === node);
			}

			if (this.selectionFocusNode) {
				this.changeFocus = node === this.selectionFocusNode ||
					(this.selectionFocusNode.noteType == this.selectionFocusNode.TEXT_NODE &&
					 this.selectionFocusNode.parentElement === node);
			}
		},

		afterRemove : function(newNode) {
			if (this.changeAnchor) {
				this.selectionAnchorNode = newNode;
				this.selectionAnchorOffset += EvoSelection.GetOverallTextOffset(newNode);
			}

			if (this.changeFocus) {
				this.selectionFocusNode = newNode;
				this.selectionFocusOffset += EvoSelection.GetOverallTextOffset(newNode);
			}

			this.changeAnchor = false;
			this.changeFocus = false;
		},

		restore : function() {
			if (this.selectionAnchorNode && this.selectionAnchorNode.parentElement) {
				var selection = {
					anchorElem : EvoSelection.GetChildPath(document.body, this.selectionAnchorNode),
					anchorOffset : this.selectionAnchorOffset
				};

				if (this.selectionFocusNode) {
					selection.focusElem = EvoSelection.GetChildPath(document.body, this.selectionFocusNode);
					selection.focusOffset = this.selectionFocusOffset;
				}

				EvoSelection.Restore(document, selection);
			} else {
				EvoSelection.Restore(document, this.selectionBefore);
			}
		}
	};

	obj.selectionBefore = EvoSelection.Store(document);
	obj.selectionAnchorNode = document.getSelection().anchorNode;
	obj.selectionAnchorOffset = document.getSelection().anchorOffset + EvoSelection.GetOverallTextOffset(obj.selectionAnchorNode);

	if (!document.getSelection().isCollapsed) {
		obj.selectionFocusNode = document.getSelection().focusNode;
		obj.selectionFocusOffset = document.getSelection().focusOffset + EvoSelection.GetOverallTextOffset(obj.selectionFocusNode);
	}

	return obj;
}
