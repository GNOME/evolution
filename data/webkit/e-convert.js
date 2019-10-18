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

var EvoConvert = {
	MIN_PARAGRAPH_WIDTH : 5, // in characters
	MIN_OL_WIDTH : 6, // includes ". " at the end
	TAB_WIDTH : 8, // in characters

	ALIGN_LEFT : 0,
	ALIGN_CENTER : 1,
	ALIGN_RIGHT : 2,
	ALIGN_JUSTIFY : 3
};

EvoConvert.GetOLMaxLetters = function(type, levels)
{
	if (type && type.toUpperCase() == "I") {
		if (levels < 2)
			return 1;
		if (levels < 3)
			return 2;
		if (levels < 8)
			return 3;
		if (levels < 18)
			return 4;
		if (levels < 28)
			return 5;
		if (levels < 38)
			return 6;
		if (levels < 88)
			return 7
		if (levels < 188)
			return 8;
		if (levels < 288)
			return 9;
		if (levels < 388)
			return 10;
		if (levels < 888)
			return 11;
		return 12;
	} else if (type && type.toUpperCase() == "A") {
		if (levels < 27)
			return 1;
		if (levels < 703)
			return 2;
		if (levels < 18279)
			return 3;
		return 4;
	} else {
		if (levels < 10)
			return 1;
		if (levels < 100)
			return 2;
		if (levels < 1000)
			return 3;
		if (levels < 10000)
			return 4;
		return 5;
	}
}

EvoConvert.getOLIndexValue = function(type, value)
{
	var str = "";

	if (type == "A" || type == "a") { // alpha
		var add = type.charCodeAt(0);

		do {
			str = String.fromCharCode(((value - 1) % 26) + add) + str;
			value = Math.floor((value - 1) / 26);
		} while (value);
	} else if (type == "I" || type == "i") { // roman
		var base = "IVXLCDM";
		var b, r, add = 0;

		if (value > 3999) {
			str = "?";
		} else {
			if (type == "i")
				base = base.toLowerCase();

			for (b = 0; value > 0 && b < 7 - 1; b += 2, value = Math.floor(value / 10)) {
				r = value % 10;
				if (r != 0) {
					if (r < 4) {
						for (; r; r--)
							str = String.fromCharCode(base.charCodeAt(b) + add) + str;
					} else if (r == 4) {
						str = String.fromCharCode(base.charCodeAt(b + 1) + add) + str;
						str = String.fromCharCode(base.charCodeAt(b) + add) + str;
					} else if (r == 5) {
						str = String.fromCharCode(base.charCodeAt(b + 1) + add) + str;
					} else if (r < 9) {
						for (; r > 5; r--)
							str = String.fromCharCode(base.charCodeAt(b) + add) + str;
						str = String.fromCharCode(base.charCodeAt(b + 1) + add) + str;
					} else if (r == 9) {
						str = String.fromCharCode(base.charCodeAt(b + 2) + add) + str;
						str = String.fromCharCode(base.charCodeAt(b) + add) + str;
					}
				}
			}
		}
	} else { // numeric
		str = "" + value;
	}

	return str;
}

EvoConvert.getComputedOrNodeStyle = function(node)
{
	if (!node)
		return null;

	var parent = node;

	while (parent && !(parent === document.body)) {
		parent = parent.parentElement;
	}

	if (parent)
		return window.getComputedStyle(node);

	return node.style;
}

EvoConvert.replaceList = function(element, tagName)
{
	var ll, lists, type = null;

	if (tagName == "OL")
		type = "";

	lists = element.getElementsByTagName(tagName);

	for (ll = lists.length - 1; ll >= 0; ll--) {
		var list;

		list = lists.item(ll);

		if (!list)
			continue;

		var style = EvoConvert.getComputedOrNodeStyle(list), ltr, ii, prefixSuffix, indent;

		if (!style)
			style = list.style;

		ltr = !style || style.direction != "rtl";

		if (type == null) {
			var level = 0, parent = list;

			for (parent = list.parentElement; parent; parent = parent.parentElement) {
				if (parent.tagName == "UL" || parent.tagName == "OL")
					level++;
			}

			switch (level % 3) {
			default:
			case 0:
				prefixSuffix = " * ";
				break;
			case 1:
				prefixSuffix = " - ";
				break;
			case 2:
				prefixSuffix = " + ";
				break;
			}

			indent = 3;
		} else {
			type = list.getAttribute("type");

			if (!type)
				type = "";

			var nChildren = 0, child;
			for (ii = 0; ii < list.children.length; ii++) {
				child = list.children.item(ii);
				if (child.tagName == "LI")
					nChildren++;
			}

			prefixSuffix = ltr ? ". " : " .";
			indent = EvoConvert.GetOLMaxLetters(type, nChildren) + prefixSuffix.length;
			if (indent < EvoConvert.MIN_OL_WIDTH)
				indent = EvoConvert.MIN_OL_WIDTH;
		}

		if (list.hasAttribute("x-evo-extra-indent")) {
			var tmp = list.getAttribute("x-evo-extra-indent");

			if (tmp) {
				tmp = parseInt(tmp);

				if (!Number.isInteger(tmp))
					tmp = 0;
			} else {
				tmp = 0;
			}

			indent += tmp;
		}

		var liCount = 0;

		for (ii = 0; ii < list.children.length; ii++) {
			var child = list.children.item(ii), node;

			if (!child)
				continue;

			// nested lists
			if (child.tagName == "DIV" && child.hasAttribute("x-evo-extra-indent") && child.hasAttribute("x-evo-li-text")) {
				node = child.cloneNode(true);

				var tmp = child.getAttribute("x-evo-extra-indent");

				if (tmp) {
					tmp = parseInt(tmp);

					if (!Number.isInteger(tmp))
						tmp = 0;
				} else {
					tmp = 0;
				}

				node.setAttribute("x-evo-extra-indent", indent + tmp);

				tmp = node.getAttribute("x-evo-li-text");
				if (ltr)
					tmp = " ".repeat(indent) + tmp;
				else
					tmp = tmp + " ".repeat(indent);

				node.setAttribute("x-evo-li-text", tmp);
			} else if (child.tagName == "LI") {
				liCount++;

				node = document.createElement("DIV");
				if (list.style.width.endsWith("ch")) {
					var width = parseInt(list.style.width.slice(0, -2));

					if (Number.isInteger(width))
						node.style.width = "" + width + "ch";
				}
				node.style.textAlign = list.style.testAlign;
				node.style.direction = list.style.direction;
				node.style.marginLeft = list.style.marginLeft;
				node.style.marginRight = list.style.marginRight;
				node.setAttribute("x-evo-extra-indent", indent);
				node.innerText = child.innerText;

				if (type == null) {
					node.setAttribute("x-evo-li-text", prefixSuffix);
				} else {
					var prefix;

					prefix = EvoConvert.getOLIndexValue(type, liCount);

					while (prefix.length + 2 /* prefixSuffix.length */ < indent) {
						prefix = ltr ? " " + prefix : prefix + " ";
					}

					node.setAttribute("x-evo-li-text", ltr ? prefix + prefixSuffix : prefixSuffix + prefix);
				}
			} else {
				node = child.cloneNode(true);

				if (node.tagName == "UL" || node.tagName == "OL") {
					var tmp = child.getAttribute("x-evo-extra-indent");

					if (tmp) {
						tmp = parseInt(tmp);

						if (!Number.isInteger(tmp))
							tmp = 0;
					} else {
						tmp = 0;
					}

					node.setAttribute("x-evo-extra-indent", indent + tmp);
				}
			}

			list.parentNode.insertBefore(node, list);
		}

		list.parentNode.removeChild(list);
	}
}

EvoConvert.calcLineLetters = function(line)
{
	var len;

	if (line.search("\t") >= 0) {
		var jj;

		len = 0;

		for (jj = 0; jj < line.length; jj++) {
			if (line.charAt(jj) == "\t") {
				len = len - (len % EvoConvert.TAB_SIZE) + EvoConvert.TAB_SIZE;
			} else {
				len++;
			}
		}
	} else {
		len = line.length;
	}

	return len;
}

EvoConvert.getQuotePrefix = function(quoteLevel, ltr)
{
	var prefix = "";

	if (quoteLevel > 0) {
		prefix = ltr ? "> " : " <";
		prefix = prefix.repeat(quoteLevel);
	}

	return prefix;
}

EvoConvert.formatParagraph = function(str, ltr, align, indent, whiteSpace, wrapWidth, extraIndent, liText, quoteLevel)
{
	if (!str || str == "")
		return liText ? liText : str;

	var lines = [], ii;

	// wrap the string first
	if (wrapWidth > 0) {
		var worker = {
			collapseWhiteSpace : whiteSpace != "pre" && whiteSpace != "pre-wrap",
			canWrap : whiteSpace != "nowrap",
			charWrap : whiteSpace == "pre",
			useWrapWidth : wrapWidth,
			spacesFrom : -1, // in 'str'
			lastSpace : -1, // in this->line
			lastWasWholeLine : false, // to distinguish between new line in the text and new line from wrapping with while line text
			lineLetters : 0,
			line : "",

			shouldWrap : function() {
				return worker.canWrap && (worker.lineLetters > worker.useWrapWidth || (
					worker.lineLetters == worker.useWrapWidth && (
					worker.lastSpace == -1/* || worker.lastSpace == worker.line.length*/)));
			},

			commitSpaces : function(ii) {
				if (worker.spacesFrom != -1 && (!worker.canWrap || worker.line.length <= worker.useWrapWidth)) {
					var spaces;

					spaces = ii - worker.spacesFrom;

					if (worker.canWrap && worker.line.length + spaces > worker.useWrapWidth)
						spaces = worker.useWrapWidth - worker.line.length;

					if (!worker.canWrap || (worker.line.length + spaces <= worker.useWrapWidth) && spaces >= 0) {
						if (worker.collapseWhiteSpace && (!extraIndent || lines.length))
							worker.line += " ";
						else
							worker.line += " ".repeat(spaces);
					}

					worker.spacesFrom = -1;
					worker.lastSpace = worker.line.length;
				} else if (worker.spacesFrom != -1) {
					worker.lastSpace = worker.line.length;
				}
			},

			commit : function(ii) {
				worker.commitSpaces(ii);

				if (worker.canWrap && worker.lastSpace != -1 && worker.lineLetters > worker.useWrapWidth) {
					lines[lines.length] = worker.line.substr(0, worker.lastSpace);
					worker.line = worker.line.substr(worker.lastSpace);
				} else if (worker.charWrap && worker.useWrapWidth != -1 && worker.lineLetters > worker.useWrapWidth) {
					var jj, subLineLetters = 0;

					for(jj = 0; jj < worker.line.length; jj++) {
						if (worker.line.charAt(jj) == "\t") {
							subLineLetters = subLineLetters - (subLineLetters % EvoConvert.TAB_SIZE) + EvoConvert.TAB_SIZE;
						} else {
							subLineLetters++;
						}

						if (subLineLetters >= worker.useWrapWidth)
							break;
					}

					lines[lines.length] = worker.line.substr(0, jj);
					worker.line = worker.line.substr(jj);
				} else if (worker.lastWasWholeLine && worker.line == "") {
					worker.lastWasWholeLine = false;
				} else {
					lines[lines.length] = worker.line;
					worker.line = "";
					worker.lastWasWholeLine = true;
				}

				if (worker.canWrap && worker.collapseWhiteSpace && lines[lines.length - 1].endsWith(" ")) {
					if (lines[lines.length - 1].length == 1)
						lines.length = lines.length - 1;
					else
						lines[lines.length - 1] = lines[lines.length - 1].substr(0, lines[lines.length - 1].length - 1);
				}

				worker.lineLetters = worker.canWrap ? EvoConvert.calcLineLetters(worker.line) : worker.line.length;
				worker.spacesFrom = -1;
				worker.lastSpace = -1;
			}
		};

		if (worker.useWrapWidth < EvoConvert.MIN_PARAGRAPH_WIDTH)
			worker.useWrapWidth = EvoConvert.MIN_PARAGRAPH_WIDTH;

		var chr;

		for (ii = 0; ii < str.length; ii++) {
			chr = str.charAt(ii);

			if (chr == "\r")
				continue;

			if (chr == "\n") {
				worker.commit(ii);
			} else if (!worker.charWrap && !worker.collapseWhiteSpace && chr == "\t") {
				if (worker.shouldWrap())
					worker.commit(ii);
				else
					worker.commitSpaces(ii);

				var add = " ".repeat(EvoConvert.TAB_WIDTH - (worker.lineLetters % EvoConvert.TAB_WIDTH));

				worker.lineLetters = worker.lineLetters - (worker.lineLetters % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;

				if (worker.shouldWrap())
					worker.commit(ii);

				worker.line += add;
				worker.lineLetters += add.length;
			} else if (!worker.charWrap && (chr == " " || chr == "\t")) {
				var setSpacesFrom = false;

				if (chr == '\t') {
					worker.lineLetters = worker.lineLetters - (worker.lineLetters % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;
					setSpacesFrom = true;
				} else if ((worker.spacesFrom == -1 && worker.line != "") || !worker.collapseWhiteSpace) {
					worker.lineLetters++;
					setSpacesFrom = true;
				}

				// all spaces at the end of paragraph line are ignored
				if (setSpacesFrom && worker.spacesFrom == -1)
					worker.spacesFrom = ii;
			} else {
				worker.commitSpaces(ii);
				worker.line += chr;

				if (chr == "\t")
					worker.lineLetters = worker.lineLetters - (worker.lineLetters % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;
				else
					worker.lineLetters++;

				if (worker.shouldWrap())
					worker.commit(ii);
			}
		}

		while (worker.line.length || worker.spacesFrom != -1 || !lines.length) {
			worker.commit(ii);
		}
	} else {
		if (str.endsWith("\n"))
			str = str.substr(0, str.length - 1);

		lines = str.split("\n");
	}

	var extraIndentStr = extraIndent > 0 ? " ".repeat(extraIndent) : null;

	if (wrapWidth <= 0) {
		for (ii = 0; ii < lines.length; ii++) {
			var len = EvoConvert.calcLineLetters(lines[ii]);

			if (wrapWidth < len)
				wrapWidth = len;
		}
	}

	str = "";

	for (ii = 0; ii < lines.length; ii++) {
		var line = lines[ii];

		if ((!ltr && align == EvoConvert.ALIGN_LEFT) ||
		    (ltr && align == EvoConvert.ALIGN_RIGHT)) {
			var len = EvoConvert.calcLineLetters(line);

			if (len < wrapWidth) {
				var add = " ".repeat(wrapWidth - len);

				if (ltr)
					line = add + line;
				else
					line = line + add;
			}
		} else if (align == EvoConvert.ALIGN_CENTER) {
			var len = EvoConvert.calcLineLetters(line);

			if (len < wrapWidth) {
				var add = " ".repeat((wrapWidth - len) / 2);

				if (ltr)
					line = add + line;
				else
					line = line + add;
			}
		} else if (align == EvoConvert.ALIGN_JUSTIFY && ii + 1 < lines.length) {
			var len = EvoConvert.calcLineLetters(line);

			if (len < wrapWidth) {
				var words = line.split(" ");

				if (words.length > 1) {
					var nAdd = (wrapWidth - len);
					var add = " ".repeat(nAdd / (words.length - 1) >= 1 ? nAdd / (words.length - 1) : nAdd), jj;

					for (jj = 0; jj < words.length - 1 && nAdd > 0; jj++) {
						words[jj] = words[jj] + add;
						nAdd -= add.length;

						if (nAdd > 0 && jj + 2 >= words.length) {
							words[jj] = " ".repeat(nAdd) + words[jj];
						}
					}

					line = words[0];

					for (jj = 1; jj < words.length; jj++) {
						line = line + " " + words[jj];
					}
				}
			}
		}

		if (!ii && liText) {
			if (ltr)
				line = liText + line;
			else
				line = line + liText;
		} else if (extraIndentStr && ii > 0) {
			if (ltr)
				line = extraIndentStr + line;
			else
				line = line + extraIndentStr;

		}

		if (indent != "") {
			if (ltr && align != EvoConvert.ALIGN_RIGHT)
				line = indent + line;
			else
				line = line + indent;
		}

		if (quoteLevel > 0) {
			if (ltr)
				line = EvoConvert.getQuotePrefix(quoteLevel, ltr) + line;
			else
				line = line + EvoConvert.getQuotePrefix(quoteLevel, ltr);
		}

		str += line + "\n";
	}

	return str;
}

EvoConvert.ImgToText = function(img)
{
	if (!img)
		return "";

	var txt;

	txt = img.alt;

	return txt ? txt : "";
}

EvoConvert.extractElemText = function(elem, normalDivWidth, quoteLevel)
{
	if (!elem)
		return "";

	if (!elem.childNodes.length)
		return elem.innerText;

	var str = "", ii;

	for (ii = 0; ii < elem.childNodes.length; ii++) {
		var node = elem.childNodes.item(ii);

		if (!node)
			continue;

		str += EvoConvert.processNode(node, normalDivWidth, quoteLevel);
	}

	return str;
}

EvoConvert.processNode = function(node, normalDivWidth, quoteLevel)
{
	var str = "";

	if (node.nodeType == node.TEXT_NODE) {
		str = node.nodeValue;
	} else if (node.nodeType == node.ELEMENT_NODE) {
		if (node.hidden)
			return str;

		var style = EvoConvert.getComputedOrNodeStyle(node), ltr, align, indent, whiteSpace;

		if (!style)
			style = node.style;

		ltr = !style || style.direction != "rtl";

		align = style ? style.textAlign : "";
		if (!align || align == "")
			align = node.style.textAlign;
		if (align)
			align = align.toLowerCase();
		if (!align)
			align = "";
		if (align == "" || align == "start")
			align = ltr ? "left" : "right";

		if (align == "center")
			align = EvoConvert.ALIGN_CENTER;
		else if (align == "right")
			align = EvoConvert.ALIGN_RIGHT;
		else if (align == "justify")
			align = EvoConvert.ALIGN_JUSTIFY;
		else
			align = EvoConvert.ALIGN_LEFT;

		// mixed indent and opposite text align does nothing
		if ((ltr && align == EvoConvert.ALIGN_RIGHT) ||
		    (!ltr && align == EvoConvert.ALIGN_LEFT)) {
			indent = "";
		} else {
			// computed style's 'indent' uses pixels, not characters
			indent = ltr ? node.style.marginLeft : node.style.marginRight;
		}

		if (indent && indent.endsWith("ch")) {
			indent = parseInt(indent.slice(0, -2));
			if (!Number.isInteger(indent) || indent < 0)
				indent = 0;
		} else {
			indent = 0;
		}

		if (indent > 0)
			indent = " ".repeat(indent);
		else
			indent = "";

		whiteSpace = style ? style.whiteSpace.toLowerCase() : "";

		if (node.tagName == "DIV" || node.tagName == "P") {
			var liText, extraIndent, width;

			liText = node.getAttribute("x-evo-li-text");
			if (!liText)
				liText = "";

			extraIndent = node.getAttribute("x-evo-extra-indent");
			extraIndent = extraIndent ? parseInt(extraIndent, 10) : 0;
			if (!Number.isInteger(extraIndent)) {
				extraIndent = 0;
			}

			width = node.style.width;
			if (width && width.endsWith("ch")) {
				width = parseInt(width.slice(0, -2));
				if (!Number.isInteger(width) || width < 0)
					width = normalDivWidth;
			} else {
				width = normalDivWidth;
			}

			str = EvoConvert.formatParagraph(EvoConvert.extractElemText(node, normalDivWidth, quoteLevel), ltr, align, indent, whiteSpace, width, extraIndent, liText, quoteLevel);
		} else if (node.tagName == "PRE") {
			str = EvoConvert.formatParagraph(EvoConvert.extractElemText(node, normalDivWidth, quoteLevel), ltr, align, indent, "pre", -1, 0, "", quoteLevel);
		} else if (node.tagName == "BR") {
			str = "\n";
		} else if (node.tagName == "IMG") {
			str = EvoConvert.ImgToText(node);
		} else {
			var isBlockquote = node.tagName == "BLOCKQUOTE";

			str = EvoConvert.extractElemText(node, normalDivWidth, quoteLevel + (isBlockquote ? 1 : 0));

			if ((!isBlockquote || !str.endsWith("\n")) &&
			    str != "\n" && ((style && style.display == "block") || node.tagName == "ADDRESS")) {
				str += "\n";
			}
		}
	}

	return str;
}

/*
 * Converts element and its children to plain text. Any <div>,<ul>,<ol>, as an immediate child
 * of the element, is wrapped to upto normalDivWidth characters, if it's defined and greater
 * than EvoConvert.MIN_PARAGRAPH_WIDTH.
 */
EvoConvert.ToPlainText = function(element, normalDivWidth)
{
	if (!element)
		return null;

	if (element.tagName == "HTML") {
		var bodys;

		bodys = element.getElementsByTagName("BODY");

		if (bodys.length == 1)
			element = bodys.item(0);
	}

	if (!element)
		return null;

	if (!normalDivWidth)
		normalDivWidth = -1;

	var uls, ols, str = "", ii;

	uls = element.getElementsByTagName("UL");
	ols = element.getElementsByTagName("OL");

	if (uls.length > 0 || ols.length > 0) {
		element = element.cloneNode(true);

		if (uls.length)
			EvoConvert.replaceList(element, "UL");

		if (ols.length)
			EvoConvert.replaceList(element, "OL");
	}

	for (ii = 0; ii < element.childNodes.length; ii++) {
		var node = element.childNodes.item(ii);

		if (!node)
			continue;

		str += EvoConvert.processNode(node, normalDivWidth, 0);
	}

	return str;
}
