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

var Evo = {
	hasSelection : false,
	blockquoteStyle : "margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex",
	magicSpacebarState: -1,
	markCitationColor : null,
	plugins : null
};

Evo.RegisterPlugin = function(plugin)
{
	if (plugin == null)
		return;

	if (plugin.name === undefined) {
		console.error("Evo.RegisterPlugin: Plugin '" + plugin + "' has missing 'name' member");
		return;
	}

	if (plugin.setup === undefined) {
		console.error("Evo.RegisterPlugin: Plugin '" + plugin.name + "' has missing 'setup' function");
		return;
	}

	if (Evo.plugins == null)
		Evo.plugins = [];

	Evo.plugins.push(plugin);
}

Evo.setupPlugins = function(doc)
{
	if (Evo.plugins == null)
		return;

	var ii;

	for (ii = 0; ii < Evo.plugins.length; ii++) {
		try {
			if (Evo.plugins[ii] != null)
				Evo.plugins[ii].setup(doc);
		} catch (err) {
			console.error("Failed to setup plugin '" + Evo.plugins[ii].name + "': " + err.name + ": " + err.message);
			Evo.plugins[ii] = null;
		}
	}
}

/* The 'traversar_obj' is an object, which implements a callback function:
   boolean exec(doc, iframe_id, level);
   and it returns whether continue the traversar */
Evo.foreachIFrameDocument = function(doc, traversar_obj, call_also_for_doc, level)
{
	if (!doc)
		return false;

	if (call_also_for_doc && !traversar_obj.exec(doc, doc.defaultView.frameElement ? doc.defaultView.frameElement.id : "", level))
		return false;

	var iframes, ii;

	iframes = doc.getElementsByTagName("iframe");

	for (ii = 0; ii < iframes.length; ii++) {
		if (!iframes[ii].contentDocument)
			continue;

		if (!traversar_obj.exec(iframes[ii].contentDocument, iframes[ii].id, level + 1))
			return false;

		if (!Evo.foreachIFrameDocument(iframes[ii].contentDocument, traversar_obj, false, level + 1))
			return false;
	}

	return true;
}

Evo.findIFrame = function(iframe_id)
{
	if (iframe_id == "")
		return null;

	var traversar = {
		iframe_id : iframe_id,
		iframe : null,
		exec : function(doc, iframe_id, level) {
			if (iframe_id == this.iframe_id) {
				this.iframe = doc.defaultView.frameElement;
				return false;
			}
			return true;
		}
	};

	Evo.foreachIFrameDocument(document, traversar, true, 0);

	return traversar.iframe;
}

Evo.findIFrameDocument = function(iframe_id)
{
	if (iframe_id == "")
		return document;

	var iframe = Evo.findIFrame(iframe_id);

	if (iframe)
		return iframe.contentDocument;

	return null;
}

Evo.runTraversarForIFrameId = function(iframe_id, traversar_obj)
{
	if (iframe_id == "*") {
		Evo.foreachIFrameDocument(document, traversar_obj, true, 0);
	} else {
		var doc = Evo.findIFrameDocument(iframe_id);

		if (doc) {
			var level = 0, parent;

			for (parent = doc.defaultView.frameElement; parent; parent = parent.ownerDocument.defaultView.frameElement) {
				level++;
			}

			traversar_obj.exec(doc, iframe_id, level);
		}
	}
}

Evo.findElementInDocumentById = function(doc, element_id)
{
	if (!doc)
		return null;

	if (element_id == "*html")
		return doc.documentElement;

	if (element_id == "*head")
		return doc.head;

	if (element_id == "*body")
		return doc.body;

	return doc.getElementById(element_id);
}

Evo.FindElement = function(iframe_id, element_id)
{
	var traversar = {
		element_id : element_id,
		res : null,
		exec : function(doc, iframe_id, level) {
			this.res = Evo.findElementInDocumentById(doc, this.element_id);
			if (this.res)
				return false;
			return true;
		}
	};

	Evo.runTraversarForIFrameId(iframe_id, traversar);

	return traversar.res;
}

Evo.SetElementHidden = function(iframe_id, element_id, value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem)
		elem.hidden = value;
}

Evo.SetElementDisabled = function(iframe_id, element_id, value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem)
		elem.disabled = value;
}

Evo.SetElementChecked = function(iframe_id, element_id, value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem)
		elem.checked = value;
}

Evo.SetElementStyleProperty = function(iframe_id, element_id, property_name, value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		if (value != null && value != "")
			elem.style.setProperty(property_name, value);
		else
			elem.style.removeProperty(property_name);
	}
}

Evo.SetElementAttribute = function(iframe_id, element_id, namespace_uri, qualified_name, value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		if (value != null && value != "")
			elem.setAttributeNS(namespace_uri, qualified_name, value);
		else
			elem.removeAttributeNS(namespace_uri, qualified_name);
	}
}

Evo.createStyleSheet = function(doc, style_sheet_id, content)
{
	var node;

	node = doc.createElement("style");
	node.id = style_sheet_id;
	node.media = "screen";
	node.innerText = content;

	doc.head.append(node);

	return node;
}

Evo.CreateStyleSheet = function(iframe_id, style_sheet_id, content)
{
	var doc = Evo.findIFrameDocument(iframe_id);

	if (!doc)
		return;

	var styles = doc.head.getElementsByTagName("style"), ii;

	for (ii = 0; ii < styles.length; ii++) {
		if (styles[ii].id == style_sheet_id) {
			styles[ii].innerText = content;
			return;
		}
	}

	Evo.createStyleSheet(doc, style_sheet_id, content);
}

Evo.RemoveStyleSheet = function(iframe_id, style_sheet_id)
{
	var traversar = {
		style_sheet_id : style_sheet_id,
		exec : function(doc, iframe_id, level) {
			if (doc && doc.head) {
				var ii, styles = doc.head.getElementsByTagName("style");

				for (ii = styles.length - 1; ii >= 0; ii--) {
					if (this.style_sheet_id == "*" || styles[ii].id == this.style_sheet_id) {
						doc.head.removeChild(styles[ii]);
					}
				}
			}

			return true;
		}
	};

	Evo.runTraversarForIFrameId(iframe_id, traversar);
}

Evo.addRuleIntoStyleSheetDocument = function(doc, style_sheet_id, selector, style)
{
	var styles = doc.head.getElementsByTagName("style"), ii, styleobj = null;

	for (ii = 0; ii < styles.length; ii++) {
		if (styles[ii].id == style_sheet_id) {
			styleobj = styles[ii];
			break;
		}
	}

	if (!styleobj) {
		Evo.createStyleSheet(doc, style_sheet_id, selector + " { " + style + " }");
		return;
	}

	for (ii = 0; ii < styleobj.sheet.cssRules.length; ii++) {
		if (styleobj.sheet.cssRules[ii].selectorText == selector) {
			styleobj.sheet.cssRules[ii].style.cssText = style;
			return;
		}
	}

	styleobj.sheet.addRule(selector, style);
}

Evo.AddRuleIntoStyleSheet = function(iframe_id, style_sheet_id, selector, style)
{
	var traversar = {
		style_sheet_id : style_sheet_id,
		selector : selector,
		style : style,
		is_unicode_bidi : style.indexOf("unicode-bidi:") >= 0,
		exec : function(doc, iframe_id, level) {
			if (doc && doc.head && (!this.is_unicode_bidi || iframe_id.indexOf(".text_html") < 0)) {
				Evo.addRuleIntoStyleSheetDocument(doc, this.style_sheet_id, this.selector, this.style);
			}

			return true;
		}
	};

	Evo.runTraversarForIFrameId(iframe_id, traversar);
}

Evo.SetDocumentContent = function(content)
{
	document.documentElement.innerHTML = content;

	Evo.initialize(null);
	window.webkit.messageHandlers.contentLoaded.postMessage("");
}

Evo.SetIFrameSrc = function(iframe_id, src_uri)
{
	var iframe = Evo.findIFrame(iframe_id);

	if (iframe)
		iframe.src = src_uri;
}

Evo.SetIFrameContent = function(iframe_id, content)
{
	var iframe = Evo.findIFrame(iframe_id);

	if (iframe) {
		iframe.contentDocument.documentElement.innerHTML = content;

		Evo.initialize(iframe);
		window.webkit.messageHandlers.contentLoaded.postMessage(iframe_id);
	}
}

Evo.elementClicked = function(elem)
{
	var with_parents_left, with_parents_top, scroll_x = 0, scroll_y = 0, offset_parent, dom_window;
	var parent_iframe_id = "";

	if (elem.ownerDocument.defaultView.frameElement)
		parent_iframe_id = elem.ownerDocument.defaultView.frameElement.id;

	with_parents_left = elem.offsetLeft;
	with_parents_top = elem.offsetTop;

	offset_parent = elem;
	while (offset_parent = offset_parent.offsetParent, offset_parent) {
		with_parents_left += offset_parent.offsetLeft;
		with_parents_top += offset_parent.offsetTop;
	}

	dom_window = elem.ownerDocument.defaultView;
	while (dom_window instanceof Window) {
		var parent_dom_window, iframe_elem;

		parent_dom_window = dom_window.parent;
		iframe_elem = dom_window.frameElement;

		while (iframe_elem) {
			with_parents_left += iframe_elem.offsetLeft;
			with_parents_top += iframe_elem.offsetTop;

			iframe_elem = iframe_elem.offsetParent;
		}

		scroll_x += dom_window.scrollX;
		scroll_y += dom_window.scrollY;

		if (parent_dom_window == dom_window)
			break;

		dom_window = parent_dom_window;
	}

	var res = {};

	res["iframe-id"] = parent_iframe_id;
	res["elem-id"] = elem.id;
	res["elem-class"] = elem.className;
	res["elem-value"] = elem.getAttribute("value");
	res["left"] = with_parents_left - scroll_x;
	res["top"] = with_parents_top - scroll_y;
	res["width"] = elem.offsetWidth;
	res["height"] = elem.offsetHeight;

	window.webkit.messageHandlers.elementClicked.postMessage(res);
}

Evo.RegisterElementClicked = function(iframe_id, elem_classes_str)
{
	var traversar = {
		elem_classes : elem_classes_str.split("\n"),
		exec : function(doc, iframe_id, level) {
			if (doc && this.elem_classes) {
				var ii;

				for (ii = 0; ii < this.elem_classes.length; ii++) {
					if (this.elem_classes[ii] != "") {
						var jj, elems;

						elems = doc.getElementsByClassName(this.elem_classes[ii]);

						for (jj = 0; jj < elems.length; jj++) {
							elems[jj].onclick = function() { Evo.elementClicked(this); };
						}
					}
				}
			}

			return true;
		}
	};

	Evo.runTraversarForIFrameId(iframe_id, traversar);
}

Evo.checkAnyParentIsPre = function(node)
{
	if (!node)
		return false;

	while (node = node.parentElement, node) {
		if (/* node instanceof HTMLPreElement */ node.tagName == "PRE")
			return true;
		if (/* node instanceof HTMLIFrameElement */ node.tagName == "IFRAME")
			break;
	}

	return false;
}

Evo.getElementContent = function(node, format, useOuterHTML)
{
	if (!node)
		return null;

	var data;

	if (format == 1) {
		data = EvoConvert.ToPlainText(node);
	} else if (format == 2) {
		data = useOuterHTML ? node.outerHTML : node.innerHTML;
	} else if (format == 3) {
		data = {};
		data["plain"] = EvoConvert.ToPlainText(node);
		data["html"] = useOuterHTML ? node.outerHTML : node.innerHTML;
	}

	return data;
}

Evo.checkHasSelection = function(content)
{
	var traversar = {
		content : content,
		has : false,
		exec : function(doc, iframe_id, level) {
			if (doc && !doc.defaultView.getSelection().isCollapsed) {
				if (content) {
					var fragment, node, inpre;

					fragment = doc.defaultView.getSelection().getRangeAt(0).cloneContents();
					inpre = Evo.checkAnyParentIsPre(doc.defaultView.getSelection().getRangeAt(0).startContainer);
					node = doc.createElement(inpre ? "PRE" : "DIV");
					node.appendChild(fragment);

					content.data = Evo.getElementContent(node, content.format, inpre);
				}

				this.has = true;

				return false;
			}

			return true;
		}
	};

	Evo.foreachIFrameDocument(document, traversar, true, 0);

	return traversar.has;
}

Evo.selectionChanged = function()
{

	var has;

	has = Evo.checkHasSelection(null);

	if (has != Evo.hasSelection) {
		Evo.hasSelection = has;
		window.webkit.messageHandlers.hasSelection.postMessage(has);
	}
}

Evo.GetSelection = function(format)
{
	var content = { format: 0, data: null };

	content.format = format;

	if (!Evo.checkHasSelection(content))
		return null;

	return content.data;
}

Evo.GetDocumentContent = function(iframe_id, format)
{
	var doc;

	if (iframe_id == "") {
		doc = document;
	} else {
		var iframe = Evo.findIFrame(iframe_id);

		if (!iframe)
			return null;

		doc = iframe.contentDocument;
	}

	return Evo.getElementContent(doc.documentElement, format, true);
}

Evo.GetElementContent = function(iframe_id, element_id, format, use_outer_html)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (!elem)
		return null;

	return Evo.getElementContent(elem, format, use_outer_html);
}

Evo.findElementFromPoint = function(doc, xx, yy, parent_elem)
{
	var elem;

	if (!parent_elem) {
		elem = doc.elementFromPoint(xx, yy);
	} else {
		var left_offset = 0, top_offset = 0, offset_parent, use_parent;

		for (use_parent = parent_elem; use_parent; use_parent = use_parent.ownerDocument.defaultView.frameElement) {
			offset_parent = use_parent;
			do {
				left_offset += offset_parent.offsetLeft - offset_parent.scrollLeft;
				top_offset += offset_parent.offsetTop - offset_parent.scrollTop;

				offset_parent = offset_parent.offsetParent;
			/* Stop on body, because it sometimes have the same offset/scroll values as its iframe parent and sometimes not. */
			} while (offset_parent && !(/* offset_parent instanceof HTMLBodyElement */ offset_parent.tagName == "BODY"));
		}

		elem = doc.elementFromPoint(xx - left_offset + window.scrollX, yy - top_offset + window.scrollY);
	}

	if (!elem) {
		return parent_elem;
	}

	if (/* !(elem instanceof HTMLIFrameElement) */ elem.tagName != "IFRAME") {
		return elem;
	}

	if (parent_elem && parent_elem === elem) {
		return parent_elem;
	}

	var iframedoc = elem.contentDocument;

	if (!iframedoc) {
		return parent_elem;
	}

	return Evo.findElementFromPoint(iframedoc, xx, yy, elem);
}

Evo.GetElementFromPoint = function(xx, yy)
{
	var elem;

	if (xx == -1 && yy == -1)
		elem = document.activeElement;
	else
		elem = Evo.findElementFromPoint(document, xx, yy, null);

	if (!elem)
		return null;

	var res = {}, iframe;

	iframe = elem.ownerDocument.defaultView.frameElement;

	res["iframe-src"] = iframe ? iframe.src : document.documentURI;
	res["iframe-id"] = iframe ? iframe.id : "";
	res["elem-id"] = elem.id;

	return res;
}

Evo.AddTooltipToLinks = function(iframe_id)
{
	var doc = Evo.findIFrameDocument(iframe_id);

	if (!doc)
		return;

	var elements, ii;

	elements = doc.getElementsByTagName("A");

	for (ii = 0; ii < elements.length; ii++) {
		var elem = elements[ii];

		if (elem.href && !elem.title) {
			var tooltip = Evo.getUriTooltip(elem.href);

			if (tooltip)
				elem.title = tooltip;
		}
	}
}

Evo.initialize = function(elem)
{
	var doc, elems, ii;

	if (elem && /*elem instanceof HTMLIFrameElement*/ elem.tagName == "IFRAME" && elem.contentDocument) {
		elem.onload = function() { Evo.initializeAndPostContentLoaded(this); };
		doc = elem.contentDocument;
	} else
		doc = document;

	Evo.setupPlugins(doc);

	elems = doc.getElementsByTagName("iframe");

	for (ii = 0; ii < elems.length; ii++) {
		var iframe = elems[ii];

		iframe.onload = function() { Evo.initializeAndPostContentLoaded(this); };

		if (iframe.contentDocument && iframe.contentDocument.body && iframe.contentDocument.body.childElementCount > 0)
			Evo.initializeAndPostContentLoaded(iframe);
		else if (iframe.contentDocument && iframe.contentDocument.body)
			iframe.contentDocument.body.onload = function() { Evo.initializeAndPostContentLoaded(this); };
	}

	/* Ensure selection, used for the caret mode */
	if (!doc.getSelection().anchorNode && doc.body.firstChild) {
		if (doc.body) {
			doc.getSelection().setPosition(doc.body.firstChild, 0);
		}

		if (doc.defaultView && !doc.defaultView.frameElement) {
			var iframe = doc.getElementsByTagName('IFRAME')[0];

			if (iframe && iframe.contentDocument && iframe.contentDocument.body && iframe.contentDocument.body.firstChild) {
				iframe.focus();
				iframe.contentDocument.getSelection().setPosition(iframe.contentDocument.body.firstChild, 0);
			}
		}
	}

	if (doc.defaultView && !doc.defaultView.frameElement && !doc.body.hasAttribute("class"))
		doc.body.className = "-e-web-view-background-color -e-web-view-text-color";
	elems = doc.querySelectorAll("input, textarea, select, button, label");

	for (ii = 0; ii < elems.length; ii++) {
		elems[ii].onfocus = function() { window.webkit.messageHandlers.needInputChanged.postMessage(true); };
		elems[ii].onblur = function() { window.webkit.messageHandlers.needInputChanged.postMessage(false); };
	}

	elems = doc.querySelectorAll("img[src^=\"file://\"]");

	for (ii = 0; ii < elems.length; ii++) {
		elems[ii].src = "evo-" + elems[ii].src;
	}

	if (doc.body && doc.querySelector("[data-evo-signature-plain-text-mode]")) {
		doc.body.setAttribute("style", "font-family: Monospace;");
	}

	doc.onselectionchange = Evo.selectionChanged;
}

Evo.initializeAndPostContentLoaded = function(elem)
{
	var iframe_id = "";

	if (elem && /*elem instanceof HTMLIFrameElement*/ elem.tagName == "IFRAME")
		iframe_id = elem.id;
	if (elem && elem.ownerDocument && elem.ownerDocument.defaultView.frameElement)
		iframe_id = elem.ownerDocument.defaultView.frameElement.id;
	else if (window.frameElement)
		iframe_id = window.frameElement.id;

	Evo.initialize(elem);

	/* Skip, when its content is not loaded yet */
	if (iframe_id != "" && elem && elem.tagName == "IFRAME" && elem.contentDocument &&
	    (!elem.contentDocument.body || !elem.contentDocument.body.childElementCount)) {
		if (elem.contentDocument.body) {
			elem.contentDocument.body.onload = function() { Evo.initializeAndPostContentLoaded(this); };
		}
	} else {
		window.webkit.messageHandlers.contentLoaded.postMessage(iframe_id);
	}

	if (window.webkit.messageHandlers.mailDisplayMagicSpacebarStateChanged)
		Evo.mailDisplayUpdateMagicSpacebarState();

	Evo.AddTooltipToLinks(iframe_id);

	var traversar = {
		exec : function(doc, ifrm_id, level) {
			if (doc.body && doc.body.firstElementChild) {
				window.webkit.messageHandlers.contentLoaded.postMessage(ifrm_id);
			}
			return true;
		}
	};

	Evo.foreachIFrameDocument(document, traversar, false, 0);
}

Evo.EnsureMainDocumentInitialized = function()
{
	Evo.initializeAndPostContentLoaded(null);
}

Evo.mailDisplayGetScrollbarHeight = function()
{
	if (Evo.mailDisplayCachedScrollbarHeight != undefined)
		return Evo.mailDisplayCachedScrollbarHeight;

	var el = document.createElement("div");
	el.style.cssText = "overflow:scroll; visibility:hidden; position:absolute;";
	document.body.appendChild(el);
	Evo.mailDisplayCachedScrollbarHeight = el.offsetHeight - el.clientHeight
	el.remove();

	return Evo.mailDisplayCachedScrollbarHeight;
}

Evo.mailDisplayUpdateIFramesHeightRecursive = function(doc)
{
	if (!doc)
		return;

	var ii, iframes;

	iframes = doc.getElementsByTagName("iframe");

	/* Update from bottom to top */
	for (ii = 0; ii < iframes.length; ii++) {
		Evo.mailDisplayUpdateIFramesHeightRecursive(iframes[ii].contentDocument);
	}

	if (!doc.scrollingElement || !doc.defaultView || !doc.defaultView.frameElement ||
	    doc.defaultView.frameElement.hasAttribute("evo-skip-iframe-auto-height"))
		return;

	if (doc.defaultView.frameElement.height == doc.scrollingElement.scrollHeight)
		doc.defaultView.frameElement.height = 10;

	doc.defaultView.frameElement.height = doc.scrollingElement.scrollHeight + 2 +
		(doc.scrollingElement.scrollWidth > doc.scrollingElement.clientWidth ? Evo.mailDisplayGetScrollbarHeight() : 0);
}

Evo.mailDisplayUpdateIFramesHeightCB = function(timeStamp)
{
	if (Evo.mailDisplayRecalcHeightTimeStamp === timeStamp)
		return;

	Evo.mailDisplayRecalcHeightTimeStamp = timeStamp;

	var scrollx = document.defaultView ? document.defaultView.scrollX : -1;
	var scrolly = document.defaultView ? document.defaultView.scrollY : -1;

	Evo.mailDisplayUpdateIFramesHeightRecursive(document);

	if (scrollx != -1 && scrolly != -1 && (
	    document.defaultView.scrollX != scrollx ||
	    document.defaultView.scrollY != scrolly))
		document.defaultView.scrollTo(scrollx, scrolly);

	Evo.mailDisplayResizeContentToPreviewWidth();
	EvoItip.resizeAgendaFramesRecursive(document);
	Evo.mailDisplayUpdateMagicSpacebarState();
}

Evo.MailDisplayUpdateIFramesHeight = function()
{
	window.requestAnimationFrame(Evo.mailDisplayUpdateIFramesHeightCB);
}

if (this instanceof Window && this.document) {
	this.document.onload = function() { Evo.initializeAndPostContentLoaded(this); };

	if (this.document.body && this.document.body.firstChild)
		Evo.initializeAndPostContentLoaded(this.document);
}

Evo.replaceImgSource = function(src, requireFirst, first, second)
{
	if (requireFirst)
		return src.replace(second, first);

	return src.replace(first, second);
}

Evo.vCardCollapseContactList = function(elem)
{
	if (elem && elem.id && elem.id != "" && elem.ownerDocument) {
		var list;

		list = elem.ownerDocument.getElementById("list-" + elem.id);
		if (list) {
			var child;

			list.hidden = !list.hidden;

			for (child = elem.firstElementChild; child; child = child.nextElementSibling) {
				if (/*child instanceof HTMLImageElement*/ child.tagName == "IMG") {
					child.src = Evo.replaceImgSource(child.src, list.hidden,
						"gtk-stock://x-evolution-pan-end", "gtk-stock://x-evolution-pan-down");
				}
			}
		}
	}
}

Evo.vCardBindInDocument = function(doc)
{
	if (!doc)
		return;

	var elems, ii;

	elems = doc.querySelectorAll("._evo_vcard_collapse_button");
	for (ii = 0; ii < elems.length; ii++) {
		elems[ii].onclick = function() { Evo.vCardCollapseContactList(this); };
	}
}

Evo.VCardBind = function(iframe_id)
{
	var traversar = {
		exec : function(doc, iframe_id, level) {
			Evo.vCardBindInDocument(doc);
		}
	};

	Evo.runTraversarForIFrameId(iframe_id, traversar);
}

Evo.mailDisplayResizeContentToPreviewWidthCB = function(timeStamp)
{
	if (Evo.mailDisplayPreviewWidthTimeStamp === timeStamp)
		return;

	Evo.mailDisplayPreviewWidthTimeStamp = timeStamp;

	if (!document || !document.documentElement ||
	    document.documentElement.scrollWidth < document.documentElement.clientWidth) {
		return;
	}

	var traversar = {
		can_force_width_on_iframe : function(iframe) {
			if (!iframe || !iframe.contentDocument)
				return false;

			/* itip-view's alternative HTML iframe is managed by other means */
			if (iframe.parentElement.id.indexOf("itip-view-alternative-html") >= 0)
				return false;
			if (iframe.id.endsWith(".itip"))
				return false;

			/* We can force the width on every message that was not formatted
			 * by text-highlight module. */
			if (iframe.id.indexOf("text-highlight") < 0)
				return true;

			/* If the message was formatted with text-highlight we can adjust the
			 * width just for the messages that were formatted as plain text. */
			return iframe.src.indexOf("__formatas=txt") >= 0;
		},

		set_iframe_and_body_width : function(doc, width, original_width, level) {
			if (!doc)
				return;

			var ii, iframes, local_width = width;

			ii = doc.getElementById("itip-agenda-column");
			if (ii && ii.clientWidth < local_width) {
				local_width -= ii.clientWidth;
			}

			iframes = doc.getElementsByTagName("iframe");

			if (level == 0) {
				local_width -= 2; /* 1 + 1 frame borders */
			} else if (!iframes.length) {
				/* Message main body */
				local_width -= level * 20; /* 10 + 10 margins of body without iframes */
				local_width -= 4;

				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", "body", "width: " + local_width + "px;");
				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".part-container", "width: " + local_width + "px;");
			} else if (level == 1) {
				local_width -= 20; /* 10 + 10 margins of body with iframes */

				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", "body",
					"width: " + local_width + "px;");

				local_width -= 4; /* 2 + 2 frame borders */

				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".part-container-nostyle iframe",
					"width: " + local_width + "px;");

				/* We need to subtract another 10 pixels from the iframe width to
				 * have the iframe's borders on the correct place. We can't subtract
				 * it from local_width as we don't want to propagate this change
				 * further. */
				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".part-container iframe",
					"width: " + (local_width - 10) + "px;");
			} else {
				local_width -= (level - 1) * 20; /* 10 + 10 margins of body with iframes */
				local_width -= 4; /* 2 + 2 frame borders */
				local_width -= 10; /* attachment margin */

				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".part-container-nostyle iframe",
					"width: " + local_width + "px;");

				Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", "body > .part-container-nostyle iframe",
					"width: " + local_width + "px;");
			}

			/* Add rules to every sub document */
			for (ii = 0; ii < iframes.length; ii++) {
				var jj, frmdoc = iframes[ii].contentDocument;
				for (jj = 0; frmdoc && jj < frmdoc.images.length; jj++) {
					var img = frmdoc.images[jj];
					if (frmdoc.defaultView && !img.hasAttribute("width") && !img.hasAttribute("height")) {
						var can1 = img.hasAttribute("x-evo-width-modified"), can2 = false;
						if (!can1)
							can2 = img.style.width == "" && img.style.height == "";
						if (can1 || can2) {
							var expected_width;
							if (can1) {
								expected_width = parseInt(img.getAttribute("x-evo-width-modified"));
							} else {
								var tmp = frmdoc.defaultView.getComputedStyle(img).width;
								if (tmp && tmp.endsWith("px"))
									expected_width = parseInt(tmp.slice(0, -2));
								else
									expected_width = tmp;
								if (expected_width > 0)
									img.setAttribute("x-evo-width-modified", expected_width);
							}
							if (expected_width > 0) {
								if (expected_width < local_width)
									img.style.width = expected_width + "px";
								else
									img.style.width = local_width + "px";
							}
						}
					}
				}

				if (!this.can_force_width_on_iframe (iframes[ii]))
					continue;

				var tmp_local_width = local_width;

				if (level == 0) {
					tmp_local_width -= 10; /* attachment's margin */

					Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".attachment-wrapper iframe:not([src*=\"__formatas=\"])",
						"width: " + tmp_local_width + "px;");

					Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", ".attachment-wrapper iframe[src*=\"__formatas=txt\"]",
						"width: " + tmp_local_width + "px;");

					Evo.addRuleIntoStyleSheetDocument(doc, "-e-mail-formatter-style-sheet", "body > .part-container-nostyle iframe",
						"width: " + tmp_local_width + "px;");
				}

				this.set_iframe_and_body_width (iframes[ii].contentDocument, tmp_local_width, original_width, level + 1);
			}
		}
	};

	var width = document.documentElement.clientWidth;

	width -= 20; /* 10 + 10 margins of body */

	traversar.set_iframe_and_body_width(document, width, width, 0);

	if (!Evo.isItip && document.documentElement.clientWidth - 20 > width)
		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
}

Evo.mailDisplayResizeContentToPreviewWidth = function()
{
	window.requestAnimationFrame(Evo.mailDisplayResizeContentToPreviewWidthCB);
}

Evo.mailDisplayUpdateMagicSpacebarState = function()
{
	var new_state = 0;

	if (document && document.defaultView && document.documentElement && document.documentElement.scrollHeight) {
		// add gap of 7 pixels, which are not needed to be scrolled to the very bottom of the page
		if (document.defaultView.scrollY + document.defaultView.innerHeight + 7 < document.documentElement.scrollHeight)
			new_state |= (1 << 0); /* E_MAGIC_SPACEBAR_CAN_GO_BOTTOM */

		if (document.defaultView.scrollY > 0)
			new_state |= (1 << 1); /* E_MAGIC_SPACEBAR_CAN_GO_TOP */
	}

	if (new_state != Evo.magicSpacebarState) {
		Evo.magicSpacebarState = new_state;
		window.webkit.messageHandlers.mailDisplayMagicSpacebarStateChanged.postMessage(Evo.magicSpacebarState);
	}
}

Evo.mailDisplayResized = function()
{
	Evo.mailDisplayResizeContentToPreviewWidth();
	Evo.mailDisplayUpdateMagicSpacebarState();
}

Evo.mailDisplayToggleHeadersVisibility = function(elem)
{
	if (!elem || !elem.ownerDocument)
		return;

	var short_headers, full_headers;

	short_headers = elem.ownerDocument.getElementById("__evo-short-headers");
	full_headers = elem.ownerDocument.getElementById("__evo-full-headers");

	if (!short_headers || !full_headers)
		return;

	var expanded = full_headers.style.getPropertyValue("display") == "table";

	full_headers.style.setProperty("display", expanded ? "none" : "table");
	short_headers.style.setProperty("display", expanded ? "table" : "none");

	var child;

	for (child = elem.firstElementChild; child; child = child.nextElementSibling) {
		if (/*child instanceof HTMLImageElement*/ child.tagName == "IMG")
			child.src = Evo.replaceImgSource(child.src, expanded,
				"gtk-stock://x-evolution-pan-end", "gtk-stock://x-evolution-pan-down");
	}

	window.webkit.messageHandlers.mailDisplayHeadersCollapsed.postMessage(expanded);
}

Evo.mailDisplayToggleAddressVisibility = function(elem)
{
	if (!elem || !elem.ownerDocument)
		return;

	var parent, img;

	/* get img and parent depending on which element the click came from (button/ellipsis) */
	if (/* elem instanceof HTMLButtonElement */ elem.tagName == "BUTTON") {
		parent = elem.parentElement.parentElement;
		img = elem.firstElementChild;
	} else {
		var button;

		parent = elem.parentElement;
		button = parent.parentElement.querySelector("#__evo-moreaddr-button");
		img = button.firstElementChild;
	}

	var full_addr, ellipsis;

	full_addr = parent.querySelector("#__evo-moreaddr");
	ellipsis = parent.querySelector("#__evo-moreaddr-ellipsis");

	if (full_addr && ellipsis) {
		var expanded;

		expanded = full_addr.style.getPropertyValue("display") == "inline";

		full_addr.style.setProperty("display", expanded ? "none" : "inline");
		ellipsis.style.setProperty("display", expanded ? "inline" : "none");
		img.src = Evo.replaceImgSource(img.src, expanded,
			"gtk-stock://x-evolution-pan-end", "gtk-stock://x-evolution-pan-down");
	}
}

Evo.mailDisplayVCardModeButtonClicked = function(elem)
{
	if (!elem || !elem.parentElement)
		return;

	var normal_btn = null, compact_btn = null, iframe_elem = null, child;

	for (child = elem.parentElement.firstElementChild; child; child = child.nextElementSibling) {
		if (!iframe_elem && /* child instanceof HTMLImageElement */ child.tagName == "IFRAME") {
			iframe_elem = child;

			if (normal_btn && compact_btn)
				break;

			continue;
		}

		var name = child.getAttribute("name");

		if (name) {
			if (!normal_btn && name == "set-display-mode-normal")
				normal_btn = child;
			else if (!compact_btn && name == "set-display-mode-compact")
				compact_btn = child;

			if (normal_btn && compact_btn && iframe_elem)
				break;
		}
	}

	if (normal_btn && compact_btn && iframe_elem) {
		normal_btn.hidden = normal_btn === elem;
		compact_btn.hidden = !normal_btn.hidden;
		iframe_elem.src = elem.getAttribute("evo-iframe-uri");
	}
}

Evo.unsetHTMLColors = function(doc)
{
	var ii, isz = doc.styleSheets.length;

	// to change only iframe-s, which are marked as such
	if (!doc.defaultView.frameElement ||
	    !doc.defaultView.frameElement.hasAttribute("x-e-unset-colors")) {
		return;
	}

	for (ii = 0; ii < isz; ii++) {
		var sheet = doc.styleSheets[ii];

		if (!sheet.cssRules ||
		    sheet.id == "-e-web-view-style-sheet" ||
		    sheet.id == "-e-mail-formatter-style-sheet") {
			continue;
		}

		var jj, jsz = sheet.cssRules.length;

		for (jj = 0; jj < jsz; jj++) {
			var rule = sheet.cssRules[jj];

			if (!rule.style || !rule.selectorText || rule.selectorText.startsWith(".-e-web-view-") || rule.selectorText.startsWith(".-e-mail-formatter-"))
				continue;

			if (rule.style.color)
				rule.style.removeProperty("color");

			if (rule.style.backgroundColor)
				rule.style.removeProperty("background-color");
		}
	}

	var elems = doc.querySelectorAll("[style],[color],[bgcolor]");

	isz = elems.length;

	for (ii = 0; ii < isz; ii++) {
		var elem = elems[ii];

		if (elem.tagName != "HTML" && elem.tagName != "IFRAME" && elem.tagName != "INPUT" && elem.tagName != "BUTTON" && elem.tagName != "IMG") {
			if (elem.style) {
				if (elem.style.color)
					elem.style.removeProperty("color");

				if (elem.style.backgroundColor)
					elem.style.removeProperty("background-color");

				if (!elem.style.length)
					elem.removeAttribute("style");
			}

			elem.removeAttribute("color");
			elem.removeAttribute("bgcolor");
		}
	}

	elems = doc.querySelectorAll("body");

	isz = elems.length;

	for (ii = 0; ii < isz; ii++) {
		var elem = elems[ii];

		elem.removeAttribute("bgcolor");
		elem.removeAttribute("text");
		elem.removeAttribute("link");
		elem.removeAttribute("alink");
		elem.removeAttribute("vlink");

		if (!elem.classList.contains("-e-web-view-text-color"))
			elem.classList.add("-e-web-view-text-color");

		if (!elem.classList.contains("-e-web-view-background-color"))
			elem.classList.add("-e-web-view-background-color");
	}
}

Evo.mailDisplaySetIFrameHeightForDocument = function(doc, minHeight)
{
	if (!doc || !doc.defaultView || !doc.scrollingElement)
		return;

	var iframe = doc.defaultView.frameElement;

	if (!iframe || iframe.hasAttribute("evo-skip-iframe-auto-height"))
		return;

	var value = minHeight;

	iframe.height = 10;

	if (value < doc.scrollingElement.scrollHeight)
		value = doc.scrollingElement.scrollHeight;
	if (doc.scrollingElement.scrollWidth > doc.scrollingElement.clientWidth)
		value += Evo.mailDisplayGetScrollbarHeight();

	// to ignore size change notifications made by itself
	if (Evo.mailDisplayResizeObserver)
		Evo.mailDisplayResizeObserver.expectChange++;

	iframe.height = value;

	// update also parent
	if (iframe.ownerDocument && iframe.ownerDocument.defaultView && iframe.ownerDocument.defaultView.frameElement)
		Evo.mailDisplaySetIFrameHeightForDocument(iframe.ownerDocument, 10);
}

Evo.mailDisplayHandleSizeEntries = function(timeStamp)
{
	if (!Evo.mailDisplaySizeEntries || !Evo.mailDisplaySizeEntries.length)
		return;

	var scrollx = document.defaultView ? document.defaultView.scrollX : -1;
	var scrolly = document.defaultView ? document.defaultView.scrollY : -1;
	var covered = [], ii;

	for (ii = Evo.mailDisplaySizeEntries.length - 1; ii >= 0; ii--) {
		var entries = Evo.mailDisplaySizeEntries[ii];

		for (const entry of entries) {
			if (covered.includes(entry.target))
				continue;
			covered[covered.length] = entry.target;

			if (entry.target.ownerDocument && entry.target.ownerDocument.defaultView &&
			    entry.target.ownerDocument.defaultView.frameElement && entry.borderBoxSize?.length > 0) {
				Evo.mailDisplaySetIFrameHeightForDocument(entry.target.ownerDocument, entry.borderBoxSize[0].blockSize);
			}
		}

		if (scrollx != -1 && scrolly != -1 && (
		    document.defaultView.scrollX != scrollx ||
		    document.defaultView.scrollY != scrolly))
			document.defaultView.scrollTo(scrollx, scrolly);
	}

	Evo.mailDisplaySizeEntries = [];
}

Evo.mailDisplaySizeChanged = function(entries, observer)
{
	if (Evo.mailDisplaySizeEntries === undefined) {
		Evo.mailDisplaySizeEntries = [];
	}
	Evo.mailDisplaySizeEntries[Evo.mailDisplaySizeEntries.length] = entries;
	if (Evo.mailDisplayResizeObserver.expectChange > 0) {
		Evo.mailDisplayResizeObserver.expectChange--;
		return;
	}
	window.requestAnimationFrame(Evo.mailDisplayHandleSizeEntries);
}

Evo.MailDisplayBindDOM = function(iframe_id, markCitationColor)
{
	Evo.markCitationColor = markCitationColor != "" ? markCitationColor : null;
	if (!Evo.mailDisplayResizeObserver) {
		Evo.mailDisplayResizeObserver = new ResizeObserver(Evo.mailDisplaySizeChanged);
		Evo.mailDisplayResizeObserver.expectChange = 0;
	}

	var traversar = {
		unstyleBlockquotes : function(doc) {
			var ii, elems;

			elems = doc.getElementsByTagName("blockquote");
			for (ii = 0; ii < elems.length; ii++) {
				var elem = elems[ii];

				if (elem.hasAttribute("type")) {
					if (elem.getAttribute("type").toLowerCase() == "cite")
						elem.removeAttribute("style");
				} else {
					elem.removeAttribute("style");
					elem.setAttribute("type", "cite");
				}

				if (elem.hasAttribute("style") &&
				    elem.getAttribute("style") == Evo.blockquoteStyle) {
					elem.removeAttribute("style");
				}

				if (Evo.markCitationColor && elem.hasAttribute("type") && elem.getAttribute("type").toLowerCase() == "cite")
					elem.style.color = Evo.markCitationColor;
			}
		},
		textRequiresWrap : function(text) {
			if (!text || text.length <= 80)
				return false;

			var cnt = -1, ii;

			for (ii = 0; ii < text.length; ii++) {
				cnt++;

				var chr = text.charAt(ii);

				if (chr == ' ' || chr == '\t' || chr == '\r' || chr == '\n')
					cnt == -1;
				else if (cnt > 80)
					return true;
			}

			return false;
		},
		wrapLongAchors : function(doc) {
			var ii, elems;

			elems = doc.getElementsByTagName("blockquote");
			for (ii = 0; ii < elems.length; ii++) {
				var elem = elems[ii];

				if (this.textRequiresWrap(elem.innerText))
					elem.classList.add("evo-awrap");
				else
					elem.classList.remove("evo-awrap");
			}
		},
		bind : function(doc) {
			var ii, elems;

			elems = doc.querySelectorAll("#__evo-collapse-headers-img");
			for (ii = 0; ii < elems.length; ii++) {
				elems[ii].onclick = function() { Evo.mailDisplayToggleHeadersVisibility(this); };
			}

			elems = doc.querySelectorAll("#__evo-moreaddr-ellipsis");
			for (ii = 0; ii < elems.length; ii++) {
				elems[ii].onclick = function() { Evo.mailDisplayToggleAddressVisibility(this); };
			}

			elems = doc.querySelectorAll("#__evo-moreaddr-button");
			for (ii = 0; ii < elems.length; ii++) {
				elems[ii].onclick = function() { Evo.mailDisplayToggleAddressVisibility(this); };
			}

			elems = doc.querySelectorAll(".org-gnome-vcard-display-mode-button");
			for (ii = 0; ii < elems.length; ii++) {
				elems[ii].onclick = function() { Evo.mailDisplayVCardModeButtonClicked(this); };
			}

			var elem;

			elem = doc.getElementById("__evo-contact-photo");

			if (elem && elem.hasAttribute("data-mailaddr")) {
				var mail_addr;

				mail_addr = elem.getAttribute("data-mailaddr");
				if (mail_addr != "") {
					elem.src = "mail://contact-photo?mailaddr=" + mail_addr;
				}
			}
		},
		exec : function(doc, iframe_id, level) {
			if (doc) {
				this.unstyleBlockquotes(doc);
				this.wrapLongAchors(doc);
				this.bind(doc);

				Evo.vCardBindInDocument(doc);

				Evo.addRuleIntoStyleSheetDocument(doc,
					"-e-mail-formatter-style-sheet",
					"a.evo-awrap",
					"white-space: normal; word-break: break-all;");
				Evo.unsetHTMLColors(doc);

				if (doc.body) {
					Evo.mailDisplayResizeObserver.observe(doc.body);
					doc.body.onresize = Evo.mailDisplayResized;
				}
			}

			return true;
		}
	};

	if (!iframe_id)
		iframe_id = "*";

	Evo.runTraversarForIFrameId(iframe_id, traversar);

	Evo.mailDisplayResizeContentToPreviewWidth();
	Evo.mailDisplayUpdateMagicSpacebarState();

	if (document.body) {
		Evo.mailDisplayResizeObserver.observe(document.body);
		document.body.onresize = Evo.mailDisplayResized;
		document.body.onscroll = Evo.mailDisplayUpdateMagicSpacebarState;
	}
}

Evo.MailDisplayShowAttachment = function(element_id, show)
{
	var elem;

	elem = Evo.FindElement("*", element_id);

	if (!elem) {
		return;
	}

	elem.hidden = !show;

	if (elem.hasAttribute("inner-html-data")) {
		var html_data = elem.getAttribute("inner-html-data");

		elem.removeAttribute("inner-html-data");

		if (html_data && html_data != "") {
			elem.innerHTML = html_data;

			var iframe;

			iframe = elem.querySelector("iframe");

			if (iframe) {
				Evo.initializeAndPostContentLoaded(iframe);
				Evo.MailDisplayBindDOM(iframe.id, Evo.markCitationColor);
			}

			var iframe_id = "";

			if (elem.ownerDocument.defaultView.frameElement)
				iframe_id = elem.ownerDocument.defaultView.frameElement.id;

			window.webkit.messageHandlers.contentLoaded.postMessage(iframe_id);
			Evo.mailDisplayUpdateMagicSpacebarState();
		}
	} else if (elem.ownerDocument.defaultView.frameElement) {
		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

Evo.MailDisplayProcessMagicSpacebar = function(towards_bottom)
{
	if (document && document.defaultView && document.defaultView.innerHeight) {
		document.defaultView.scrollBy(0, (towards_bottom ? 1 : -1) * document.defaultView.innerHeight);
	}

	Evo.mailDisplayUpdateMagicSpacebarState();
}

Evo.MailDisplayManageInsecureParts = function(iframe_id, part_id_prefix, show, partids)
{
	var ii, elem;

	elem = Evo.FindElement(iframe_id, "show:" + part_id_prefix);
	if (elem) {
		elem.hidden = show
	}

	elem = Evo.FindElement(iframe_id, "hide:" + part_id_prefix);
	if (elem) {
		elem.hidden = !show
	}

	for (ii = 0; ii < partids.length; ii++) {
		elem = Evo.FindElement(iframe_id, partids[ii]);

		if (elem)
			elem.hidden = !show;
	}
}

var EvoItip = {
	SELECT_ESOURCE : "select_esource",
	TEXTAREA_RSVP_COMMENT : "textarea_rsvp_comment",
	CHECKBOX_RSVP : "checkbox_rsvp",
	CHECKBOX_RECUR : "checkbox_recur",
	CHECKBOX_KEEP_ALARM : "checkbox_keep_alarm",
	CHECKBOX_INHERIT_ALARM : "checkbox_inherit_alarm",
	CHECKBOX_UPDATE : "checkbox_update",
	CHECKBOX_FREE_TIME : "checkbox_free_time",
	TABLE_ROW_BUTTONS : "table_row_buttons",
	BUTTON_IMPORT_BARE : "button_import_bare"
};

EvoItip.alarmCheckClickedCb = function(check1)
{
	var check2;

	if (check1.id == EvoItip.CHECKBOX_KEEP_ALARM) {
		check2 = check1.ownerDocument.getElementById(EvoItip.CHECKBOX_INHERIT_ALARM);
	} else {
		check2 = check1.ownerDocument.getElementById(EvoItip.CHECKBOX_KEEP_ALARM);
	}

	if (check2) {
		check2.disabled = check1.hidden && check1.checked;
	}
}

EvoItip.selectedSourceChanged = function(elem)
{
	var data = {};

	data["iframe-id"] = elem.ownerDocument.defaultView.frameElement.id;
	data["source-uid"] = elem.value;

	window.webkit.messageHandlers.itipSourceChanged.postMessage(data);
}

EvoItip.Initialize = function(iframe_id)
{
	var doc = Evo.findIFrameDocument(iframe_id);

	if (!doc) {
		return;
	}

	var elem;

	elem = doc.getElementById(EvoItip.SELECT_ESOURCE);
	if (elem) {
		elem.onchange = function() { EvoItip.selectedSourceChanged(this); };
	}

	elem = doc.getElementById(EvoItip.CHECKBOX_RECUR);
	if (elem) {
		elem.onclick = function() { window.webkit.messageHandlers.itipRecurToggled.postMessage(this.ownerDocument.defaultView.frameElement.id); };
	}

	elem = doc.getElementById(EvoItip.CHECKBOX_RSVP);
	if (elem) {
		elem.onclick = function() {
				var elem = this.ownerDocument.getElementById(EvoItip.TEXTAREA_RSVP_COMMENT);
				if (elem) {
					elem.disabled = !this.checked;
				}
			};
	}

	elem = doc.getElementById(EvoItip.CHECKBOX_INHERIT_ALARM);
	if (elem) {
		elem.onclick = function() { EvoItip.alarmCheckClickedCb(this); };
	}

	elem = doc.getElementById(EvoItip.CHECKBOX_KEEP_ALARM);
	if (elem) {
		elem.onclick = function() { EvoItip.alarmCheckClickedCb(this); };
	}
}

EvoItip.SetElementInnerHTML = function(iframe_id, element_id, html_content)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		elem.innerHTML = html_content;
		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.SetShowCheckbox = function(iframe_id, element_id, show, update_second)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		elem.hidden = !show;

		if (elem.nextElementSibling) {
			elem.nextElementSibling.hidden = !show;
		}

		if (!show) {
			elem.checked = false;
		}

		if (update_second) {
			EvoItip.alarmCheckClickedCb(elem);
		}

		elem = elem.ownerDocument.getElementById("table_row_" + element_id);
		if (elem) {
			elem.hidden = !show;
		}

		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.SetAreaText = function(iframe_id, element_id, text)
{
	var row = Evo.FindElement(iframe_id, element_id);

	if (row) {
		row.hidden = text == "";

		if (row.lastElementChild) {
			row.lastElementChild.innerHTML = text;
		}

		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.UpdateTimes = function(iframe_id, element_id, header, label)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		elem.hidden = false;

		if (elem.firstElementChild) {
			elem.firstElementChild.innerHTML = header;
		}

		if (elem.lastElementChild) {
			elem.lastElementChild.innerHTML = label;
		}

		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.AppendInfoRow = function(iframe_id, table_id, row_id, icon_name, message)
{
	var cell, row, table = Evo.FindElement(iframe_id, table_id);

	if (!table) {
		return;
	}

	row = table.insertRow(-1);
	row.id = row_id;

	cell = row.insertCell(-1);

	if (icon_name && icon_name != "") {
		var img;

		img = table.ownerDocument.createElement("img");
		img.src = "gtk-stock://" + icon_name;

		cell.appendChild(img);
	}

	cell = row.insertCell(-1);
	cell.innerHTML = message;

	window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
}

EvoItip.RemoveInfoRow = function(iframe_id, row_id)
{
	var row = Evo.FindElement(iframe_id, row_id);

	if (row && row.parentNode) {
		row.parentNode.removeChild(row);
		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.RemoveChildNodes = function(iframe_id, element_id)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		while (elem.lastChild) {
			elem.removeChild(elem.lastChild);
		}

		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.AddToSourceList = function(iframe_id, optgroup_id, optgroup_label, option_id, option_label, writable)
{
	var doc, select_elem;

	doc = Evo.findIFrameDocument(iframe_id);
	select_elem = doc ? doc.getElementById(EvoItip.SELECT_ESOURCE) : null;

	if (!select_elem) {
		return;
	}

	var option, optgroup;

	optgroup = doc.getElementById (optgroup_id);

	if (!optgroup) {
		optgroup = doc.createElement("optgroup");
		optgroup.id = optgroup_id;
		optgroup.label = optgroup_label;

		select_elem.appendChild(optgroup);
	}

	option = doc.createElement("option");
	option.value = option_id;
	option.label = option_label;
	option.innerHTML = option_label;
	option.className = "calendar";

	if (!writable) {
		option.disabled = true;
	}

	optgroup.appendChild(option);
}

EvoItip.HideButtons = function(iframe_id, element_id)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		var child;

		for (child = elem.firstElementChild; child; child = child.nextElementSibling) {
			var button = child.firstElementChild;

			if (button)
				button.hidden = true;
		}

		window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
	}
}

EvoItip.SetElementAccessKey = function(iframe_id, element_id, access_key)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		elem.accessKey = access_key;
	}
}

EvoItip.SetSelectSelected = function(iframe_id, element_id, option_value)
{
	var elem = Evo.FindElement(iframe_id, element_id);

	if (elem) {
		var ii;

		for (ii = 0; ii < elem.length; ii++) {
			if (elem.item(ii).value == option_value) {
				elem.item(ii).selected = true;
				break;
			}
		}

		// claim what source is selected when failed to select the requested source
		if (ii >= elem.length)
			EvoItip.selectedSourceChanged(elem);
	}
}

EvoItip.SetButtonsDisabled = function(iframe_id, disabled)
{
	var doc = Evo.findIFrameDocument(iframe_id);

	if (!doc) {
		return;
	}

	var elem, cell;

	elem = doc.getElementById(EvoItip.CHECKBOX_UPDATE);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_RECUR);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_FREE_TIME);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_KEEP_ALARM);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_INHERIT_ALARM);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_RSVP);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.TEXTAREA_RSVP_COMMENT);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.BUTTON_IMPORT_BARE);
	if (elem)
		elem.disabled = disabled;

	elem = doc.getElementById(EvoItip.TABLE_ROW_BUTTONS);
	if (!elem)
		return;

	for (cell = elem.firstElementChild; cell; cell = cell.nextElementSibling) {
		var btn = cell.firstElementChild;

		if (btn && !btn.hidden) {
			btn.disabled = disabled;
		}
	}
}

EvoItip.GetState = function(iframe_id)
{
	var doc;

	doc = Evo.findIFrameDocument(iframe_id);

	if (!doc) {
		return null;
	}

	var elem, res = {};

	elem = doc.getElementById(EvoItip.TEXTAREA_RSVP_COMMENT);
	res["rsvp-comment"] = elem ? elem.value : null;

	elem = doc.getElementById(EvoItip.CHECKBOX_RSVP);
	res["rsvp-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_UPDATE);
	res["update-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_RECUR);
	res["recur-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_FREE_TIME);
	res["free-time-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_KEEP_ALARM);
	res["keep-alarm-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	elem = doc.getElementById(EvoItip.CHECKBOX_INHERIT_ALARM);
	res["inherit-alarm-check"] = elem && elem.checked && !elem.hidden && !elem.disabled;

	return res;
}

EvoItip.FlipAlternativeHTMLPart = function(iframe_id, element_value, img_id, span_id)
{
	var elem = Evo.FindElement(iframe_id, element_value);
	if (elem) {
		elem.hidden = !elem.hidden;
	}
	elem = Evo.FindElement(iframe_id, img_id);
	if (elem) {
		var tmp = elem.src;
		elem.src = elem.getAttribute("othersrc");
		elem.setAttribute("othersrc", tmp);
	}
	elem = Evo.FindElement(iframe_id, img_id + "-dark");
	if (elem) {
		var tmp = elem.src;
		elem.src = elem.getAttribute("othersrc");
		elem.setAttribute("othersrc", tmp);
	}
	elem = Evo.FindElement(iframe_id, span_id);
	if (elem) {
		var tmp = elem.innerText;
		elem.innerText = elem.getAttribute("othertext");
		elem.setAttribute("othertext", tmp);
	}
	window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
}

EvoItip.UpdateAgenda = function(iframe_id, html, width, scrollToTime)
{
	var agendaIframe = Evo.FindElement(iframe_id, "itip-agenda-iframe");

	if (!agendaIframe)
		return;

	Evo.isItip = true;
	agendaIframe.style.width = (width + 40) + "px";

	var innerDoc = agendaIframe.contentDocument;

	if (!innerDoc.body.firstElementChild) {
		var tmpHtml = agendaIframe.getAttribute("itip-agenda-html");
		agendaIframe.removeAttribute("itip-agenda-html");
		innerDoc.body.innerHTML = tmpHtml;

		var link = innerDoc.createElement("LINK");
		link.setAttribute("type", "text/css");
		link.setAttribute("rel", "stylesheet");
		link.setAttribute("href", "evo-file://$EVOLUTION_WEBKITDATADIR/webview.css");
		innerDoc.head.appendChild(link);
	}

	innerDoc.body.style.width = width + "px";

	var compInfoDiv = Evo.FindElement(iframe_id, "itip-comp-info-div");
	if (compInfoDiv) {
		if (compInfoDiv.scrollHeight == agendaIframe.scrollHeight && agendaIframe.scrollHeight > 400)
			agendaIframe.style.height = "400px";
		if (compInfoDiv.scrollHeight > 400)
			agendaIframe.style.height = compInfoDiv.scrollHeight + "px";
	}

	var elem, agendaDiv = innerDoc.getElementById("itip-agenda-div");
	if (agendaDiv) {
		agendaDiv.innerHTML = html;

		elem = innerDoc.getElementById("itip-agenda-column");
		if (elem)
			elem.style.width = width + "px";
		elem = innerDoc.getElementById("itip-agenda-div");
		if (elem)
			elem.style.width = width + "px";
		elem = innerDoc.getElementById("itip-agenda");
		if (elem)
			elem.style.width = width + "px";

		if (scrollToTime > 0 && innerDoc.scrollingElement)
			innerDoc.scrollingElement.scrollTo(0, scrollToTime > 60 ? (scrollToTime - 60) : 0);
	}

	window.webkit.messageHandlers.scheduleIFramesHeightUpdate.postMessage(0);
}

EvoItip.resizeAgendaFramesRecursive = function(doc)
{
	if (!doc)
		return;

	var ii, iframes;

	iframes = doc.getElementsByTagName("iframe");

	/* Update from bottom to top */
	for (ii = 0; ii < iframes.length; ii++) {
		EvoItip.resizeAgendaFramesRecursive(iframes[ii].contentDocument);
	}

	if (!doc.scrollingElement || !doc.defaultView || !doc.defaultView.frameElement)
		return;

	var agendaIframe = doc.getElementById("itip-agenda-iframe");

	if (!agendaIframe)
		return;

	var compInfoDiv = doc.getElementById("itip-comp-info-div");

	if (!compInfoDiv)
		return;

	if (compInfoDiv.scrollHeight == agendaIframe.scrollHeight && agendaIframe.scrollHeight > 400)
		agendaIframe.style.height = "400px";
	if (compInfoDiv.scrollHeight > 400)
		agendaIframe.style.height = compInfoDiv.scrollHeight + "px";

	if (agendaIframe.ownerDocument.scrollingElement && agendaIframe.ownerDocument.defaultView && agendaIframe.ownerDocument.defaultView.frameElement) {
		var parentFrame = agendaIframe.ownerDocument.defaultView.frameElement;
		var scrollTop = document.scrollingElement.scrollTop;

		parentFrame.style.width = "400px";
		parentFrame.style.width = (agendaIframe.ownerDocument.scrollingElement.scrollWidth + 5) + "px";
		parentFrame.style.height = parentFrame.ownerDocument.documentElement.clientHeight + "px";

		if (doc.scrollingElement.scrollHeight > parentFrame.clientHeight)
			parentFrame.style.height = doc.scrollingElement.scrollHeight + "px";

		document.scrollingElement.scrollTop = scrollTop;
	}
}
