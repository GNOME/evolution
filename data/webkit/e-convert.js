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
	ALIGN_JUSTIFY : 3,

	NOWRAP_CHAR_START : "\x01",
	NOWRAP_CHAR_END : "\x02",

	E_HTML_LINK_TO_TEXT_NONE		: 0,
	E_HTML_LINK_TO_TEXT_INLINE		: 1,
	E_HTML_LINK_TO_TEXT_REFERENCE		: 2,
	E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL : 3,
};

/* EvoConvert.linkRequiresReference() function is added in the C code */

EvoConvert.processAnchor = function(node, context)
{
	var str;

	if (!node.innerText.includes(" ") && !node.innerText.includes("\n"))
		str = EvoConvert.NOWRAP_CHAR_START + node.innerText + EvoConvert.NOWRAP_CHAR_END;
	else
		str = node.innerText;
	if (context && node.href && EvoConvert.linkRequiresReference(node.href, node.innerText)) {
		if (context.link_to_text == EvoConvert.E_HTML_LINK_TO_TEXT_INLINE) {
			str += " <" + EvoConvert.NOWRAP_CHAR_START + node.href + EvoConvert.NOWRAP_CHAR_END + ">";
		} else if (context.link_to_text == EvoConvert.E_HTML_LINK_TO_TEXT_REFERENCE) {
			var index;

			for (index = 0; index < context.append_refs.length; index++) {
				if (context.append_refs[index].href == node.href)
					break;
			}

			if (index == context.append_refs.length)
				context.append_refs[context.append_refs.length] = { label : node.innerText, href : node.href };

			str += " [" + (index + 1) + "]";
		} else if (context.link_to_text == EvoConvert.E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL) {
			var index;

			for (index = 0; index < context.append_refs.length; index++) {
				if (context.append_refs[index].href == node.href)
					break;
			}

			if (index == context.append_refs.length)
				context.append_refs[context.append_refs.length] = { href : node.href };

			str += " [" + (index + 1) + "]";
		}
	}

	return str;
}

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

EvoConvert.replaceList = function(element, tagName, normalDivWidth, context)
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

				var anchors = child.getElementsByTagName("A"), jj;

				for (jj = anchors.length - 1; jj >= 0; jj--) {
					var anchor = anchors[jj], str;

					str = EvoConvert.processAnchor(anchor, context);
					anchor.parentNode.insertBefore(document.createTextNode(str), anchor);
					anchor.remove();
				}

				node = document.createElement("DIV");
				if (list.style.width.endsWith("ch")) {
					var width = parseInt(list.style.width.slice(0, -2));

					if (Number.isInteger(width))
						node.style.width = "" + width + "ch";
				} else if (normalDivWidth > 0) {
					var width, needs;

					if (tagName == "UL") {
						needs = 3;
					} else {
						needs = EvoConvert.GetOLMaxLetters(list.getAttribute("type"), list.children.length) + 2; // length of ". " suffix

						if (needs < EvoConvert.MIN_OL_WIDTH)
							needs = EvoConvert.MIN_OL_WIDTH;
					}

					width = normalDivWidth - needs;

					if (width < EvoConvert.MIN_PARAGRAPH_WIDTH)
						width = EvoConvert.MIN_PARAGRAPH_WIDTH;

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
			collapseEndingWhiteSpace : whiteSpace != "pre",
			canWrap : whiteSpace != "nowrap",
			charWrap : whiteSpace == "pre",
			inAnchor : 0,
			ignoreLineLetters : 0, // used for EvoConvert.NOWRAP_CHAR_START and EvoConvert.NOWRAP_CHAR_END, which should be skipped in width calculation
			useWrapWidth : wrapWidth,
			spacesFrom : -1, // in 'str'
			lastWrapableChar : -1, // in this->line
			lineLetters : 0,
			line : "",

			maybeRecalcIgnoreLineLetters : function() {
				if (this.ignoreLineLetters) {
					var ii, len = this.line.length, chr;

					this.ignoreLineLetters = 0;

					for (ii = 0; ii < len; ii++) {
						chr = this.line[ii];

						if (chr == EvoConvert.NOWRAP_CHAR_START || chr == EvoConvert.NOWRAP_CHAR_END)
							this.ignoreLineLetters++;
					}
				}
			},

			mayConsumeWhitespaceAfterWrap : function(str, ii) {
				return ii > 0 && this.line == "" && str[ii - 1] == EvoConvert.NOWRAP_CHAR_END;
			},

			isInUnwrapPart : function() {
				if (this.inAnchor)
					return true;

				if (this.line[0] == EvoConvert.NOWRAP_CHAR_START)
					return this.line.indexOf(EvoConvert.NOWRAP_CHAR_END) < 0;

				return false;
			},

			shouldWrap : function(nextChar) {
				return this.canWrap && (!this.collapseWhiteSpace || nextChar != '\n') &&
					(!this.isInUnwrapPart() || this.lastWrapableChar != -1) && (this.lineLetters - this.ignoreLineLetters > this.useWrapWidth || (
					((!this.charWrap && (nextChar == " " || nextChar == "\t") && this.lineLetters - this.ignoreLineLetters > this.useWrapWidth) ||
					((this.charWrap || (nextChar != " " && nextChar != "\t")) && this.lineLetters - this.ignoreLineLetters == this.useWrapWidth)) && (
					this.lastWrapableChar == -1/* || this.lastWrapableChar == this.line.length*/)));
			},

			commitSpaces : function(ii) {
				if (this.spacesFrom != -1 && (!this.canWrap || this.line.length - this.ignoreLineLetters <= this.useWrapWidth)) {
					var spaces;

					spaces = ii - this.spacesFrom;

					if (this.canWrap && this.line.length - this.ignoreLineLetters + spaces > this.useWrapWidth)
						spaces = this.useWrapWidth - this.line.length + this.ignoreLineLetters;

					if (!this.canWrap || (this.line.length - this.ignoreLineLetters + spaces <= this.useWrapWidth) && spaces >= 0) {
						if (this.collapseWhiteSpace && (!extraIndent || lines.length))
							this.line += " ";
						else
							this.line += " ".repeat(spaces);
					}

					this.spacesFrom = -1;
					this.lastWrapableChar = this.line.length;
				} else if (this.spacesFrom != -1) {
					this.lastWrapableChar = this.line.length;
				}
			},

			commit : function(ii, force) {
				this.commitSpaces(ii);

				var didWrap = false;

				if (this.canWrap && this.lastWrapableChar != -1 && this.lineLetters - this.ignoreLineLetters > this.useWrapWidth) {
					lines[lines.length] = this.line.substr(0, this.lastWrapableChar);
					this.line = this.line.substr(this.lastWrapableChar);
					this.maybeRecalcIgnoreLineLetters();
					didWrap = true;
				} else if (!this.isInUnwrapPart() && this.useWrapWidth != -1 && this.lineLetters - this.ignoreLineLetters > this.useWrapWidth) {
					var jj;

					if (this.charWrap) {
						var subLineLetters = 0, ignoreSubLineLetters = 0, chr;

						for(jj = 0; jj < this.line.length; jj++) {
							chr = this.line.charAt(jj);

							if (chr == "\t") {
								subLineLetters = subLineLetters - ((subLineLetters - ignoreSubLineLetters) % EvoConvert.TAB_SIZE) + EvoConvert.TAB_SIZE;
							} else {
								subLineLetters++;
							}

							if (chr == EvoConvert.NOWRAP_CHAR_START || chr == EvoConvert.NOWRAP_CHAR_END)
								ignoreSubLineLetters++;

							if (subLineLetters - ignoreSubLineLetters >= this.useWrapWidth)
								break;
						}
					} else {
						jj = this.line.length;
					}

					lines[lines.length] = this.line.substr(0, jj);
					this.line = this.line.substr(jj);
					this.maybeRecalcIgnoreLineLetters();
					didWrap = true;
				} else {
					lines[lines.length] = this.line;
					this.line = "";
					this.ignoreLineLetters = 0;
				}

				if (this.canWrap && this.collapseEndingWhiteSpace && didWrap && lines[lines.length - 1].endsWith(" ")) {
					if (lines[lines.length - 1].length == 1)
						lines.length = lines.length - 1;
					else
						lines[lines.length - 1] = lines[lines.length - 1].substr(0, lines[lines.length - 1].length - 1);
				}

				if (force && this.line) {
					lines[lines.length] = this.line;
					this.line = "";
					this.ignoreLineLetters = 0;
				}

				this.lineLetters = this.canWrap ? EvoConvert.calcLineLetters(this.line) : this.line.length;
				this.spacesFrom = -1;
				this.lastWrapableChar = -1;
			}
		};

		if (worker.useWrapWidth < EvoConvert.MIN_PARAGRAPH_WIDTH)
			worker.useWrapWidth = EvoConvert.MIN_PARAGRAPH_WIDTH;

		var chr, isHighSurrogate = false;

		for (ii = 0; ii < str.length; ii += 1 + (isHighSurrogate ? 1 : 0)) {
			// surrogate are two characters "high+low"; high: 0xD800 - 0xDBFF; low: 0xDC00 - 0xDFFF
			// and cannot split after the high surrogate, because that would break the character encoding
			isHighSurrogate = str.charCodeAt(ii) >= 0xd800 && str.charCodeAt(ii) <= 0xdbff;
			if (isHighSurrogate)
				chr = str.substr(ii, 2);
			else
				chr = str.charAt(ii);

			if (chr == EvoConvert.NOWRAP_CHAR_START)
				worker.inAnchor++;

			if (chr == "\n") {
				if (!worker.mayConsumeWhitespaceAfterWrap(str, ii))
					worker.commit(ii, true);
			} else if (!worker.charWrap && !worker.collapseWhiteSpace && chr == "\t") {
				if (worker.shouldWrap(str[ii + 1]))
					worker.commit(ii, false);
				else
					worker.commitSpaces(ii);

				var add = chr; // this expands the tab, instead of leaving it '\t'...  " ".repeat(EvoConvert.TAB_WIDTH - ((worker.lineLetters - worker.ignoreLineLetters) % EvoConvert.TAB_WIDTH));

				worker.lineLetters = worker.lineLetters - ((worker.lineLetters - worker.ignoreLineLetters) % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;

				if (worker.shouldWrap(str[ii + 1]))
					worker.commit(ii, false);

				worker.line += add;
				worker.lineLetters += add.length;
			} else if (!worker.charWrap && (chr == " " || chr == "\t")) {
				var setSpacesFrom = false;

				if (chr == "\t") {
					worker.lineLetters = worker.lineLetters - ((worker.lineLetters - worker.ignoreLineLetters) % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;
					setSpacesFrom = true;
				} else if ((worker.spacesFrom == -1 && worker.line != "") || (!worker.collapseWhiteSpace && !worker.mayConsumeWhitespaceAfterWrap(str, ii))) {
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
					worker.lineLetters = worker.lineLetters - ((worker.lineLetters - worker.ignoreLineLetters) % EvoConvert.TAB_WIDTH) + EvoConvert.TAB_WIDTH;
				else
					worker.lineLetters += chr.length;

				if (chr == EvoConvert.NOWRAP_CHAR_START || chr == EvoConvert.NOWRAP_CHAR_END)
					worker.ignoreLineLetters++;

				if (chr == EvoConvert.NOWRAP_CHAR_END && worker.inAnchor)
					worker.inAnchor--;

				if (worker.shouldWrap(str[ii + 1]))
					worker.commit(ii, false);

				if (chr == "-" && worker.line.length && !worker.inAnchor)
					worker.lastWrapableChar = worker.line.length;
				else if (chr == EvoConvert.NOWRAP_CHAR_START && quoteLevel > 0 && worker.lastWrapableChar < 0 && worker.inAnchor <= 1 && worker.line.length > 1) {
					worker.lastWrapableChar = worker.line.length;
				}
			}
		}

		while (worker.line.length || worker.spacesFrom != -1 || !lines.length) {
			worker.commit(ii, false);
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

EvoConvert.appendNodeText = function(node, str, text)
{
	/* This breaks "-- <br>", thus disable it for now. Cannot distinguish from test 70 of /EWebView/ConvertToPlain.

	if (node && node.parentElement && text.startsWith('\n') && str.endsWith(" ")) {
		var whiteSpace = "normal";

		if (node.parentElement)
			whiteSpace = window.getComputedStyle(node.parentElement).whiteSpace;

		if (!whiteSpace || whiteSpace == "normal") {
			return str.substr(0, str.length - 1) + text;
		}
	} */

	return str + text;
}

EvoConvert.extractElemText = function(elem, normalDivWidth, quoteLevel, context)
{
	if (!elem)
		return "";

	if (!elem.childNodes.length) {
		if (elem.innerText)
			return elem.innerText;
		return "";
	}

	var str = "", ii;

	for (ii = 0; ii < elem.childNodes.length; ii++) {
		var node = elem.childNodes.item(ii);

		if (!node)
			continue;

		str = EvoConvert.appendNodeText(node, str, EvoConvert.processNode(node, normalDivWidth, quoteLevel, context));
	}

	return str;
}

EvoConvert.mergeConsecutiveSpaces = function(str)
{
	if (str.indexOf("  ") >= 0) {
		var words = str.split(" "), ii, word;

		str = "";

		for (ii = 0; ii < words.length; ii++) {
			word = words[ii];

			if (word) {
				if (ii)
					str += " ";

				str += word;
			}
		}

		if (!words[words.length - 1])
			str += " ";
	}

	return str;
}

EvoConvert.RemoveInsignificantNewLines = function(node, stripSingleSpace)
{
	var str = "";

	if (node && node.nodeType == node.TEXT_NODE) {
		var has_rnt;
		str = node.nodeValue;

		has_rnt = str.indexOf("\r") >= 0 ||
			  str.indexOf("\n") >= 0 ||
			  str.indexOf("\t") >= 0;
		if (has_rnt || str.indexOf(" ") >= 0) {
			var whiteSpace = "normal";

			if (node.parentElement)
				whiteSpace = window.getComputedStyle(node.parentElement).whiteSpace;

			if (whiteSpace == "pre-line") {
				str = EvoConvert.mergeConsecutiveSpaces(str.replace(/\t/g, " "));
			} else if (!whiteSpace || whiteSpace == "normal" || whiteSpace == "nowrap") {
				if (str == "\n" || str == "\r" || str == "\r\n") {
					var previousSibling = node.previousElementSibling;
					if (stripSingleSpace || (previousSibling && (previousSibling.tagName == "BR" ||
					    previousSibling.tagName == "DIV" || previousSibling.tagName == "P" ||
					    previousSibling.tagName == "BODY")))
						str = "";
				} else if (has_rnt) {
					if ((!node.previousSibling || node.previousSibling.tagName) && (str[0] == '\r' || str[0] == '\n')) {
						var ii;

						for (ii = 0; str[ii] == '\n' || str[ii] == '\r'; ii++) {
							// do nothing, just skip all leading insignificant new lines
						}

						str = str.substr(ii);
					}

					if (str.length > 0 && (!node.nextSibling || node.nextSibling.tagName) && (str[str.length - 1] == '\r' || str[str.length - 1] == '\n')) {
						var ii;

						for (ii = str.length - 1; ii >= 0 && (str[ii] == '\n' || str[ii] == '\r'); ii--) {
							// do nothing, just skip all trailing insignificant new lines
						}

						str = str.substr(0, ii + 1);
					}

					if (str.length == 0 && stripSingleSpace === false)
						str = " ";

					str = EvoConvert.mergeConsecutiveSpaces(str.replace(/\t/g, " ").replace(/\r/g, " ").replace(/\n/g, " "));

					if ((!whiteSpace || whiteSpace == "normal") && str == " ") {
						if (stripSingleSpace || (!node.previousElementSibling && !node.nextElementSibling &&
						    node.parentElement && (node.parentElement.tagName == "DIV" ||
						    node.parentElement.tagName == "DIV" || node.parentElement.tagName == "P" ||
						    node.parentElement.tagName == "PRE" || node.parentElement.tagName == "BODY"))) {
							str = "";
						}
					}
				}
			}
		}
	}

	return str;
}

EvoConvert.processNode = function(node, normalDivWidth, quoteLevel, context)
{
	var str = "";

	if (node.nodeType == node.TEXT_NODE) {
		str = EvoConvert.RemoveInsignificantNewLines(node, false);
	} else if (node.nodeType == node.ELEMENT_NODE) {
		if (node.hidden ||
		    node.tagName == "STYLE" ||
		    node.tagName == "META" ||
		    (node.tagName == "SPAN" && node.classList.contains("-x-evo-quoted")))
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
			var liText, extraIndent, width, useDefaultWidth = false;

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
					useDefaultWidth = true;
			} else {
				useDefaultWidth = true;
			}

			var childrenWillWrap = true;
			if (!node.classList.contains("gmail_quote")) {
				childrenWillWrap = node.childNodes.length > 0 && liText == "";
				var ii;
				for (ii = 0; childrenWillWrap && ii < node.childNodes.length; ii++) {
					var child = node.childNodes.item(ii);
					childrenWillWrap = (child.nodeType == child.ELEMENT_NODE &&
							   ((child.tagName == "DIV" && !child.getAttribute("x-evo-li-text")) ||
							    child.tagName == "P" ||
							    child.tagName == "PRE" ||
							    child.tagName == "BLOCKQUOTE" ||
							    child.tagName == "BR")) ||
							   (child.nodeType == child.TEXT_NODE &&
							    EvoConvert.RemoveInsignificantNewLines(child, true).trim().length == 0);
				}
			}

			if (childrenWillWrap) {
				width = -1;
			} else if (useDefaultWidth && normalDivWidth > 0) {
				width = normalDivWidth - (quoteLevel * 2);

				if (width < EvoConvert.MIN_PARAGRAPH_WIDTH)
					width = EvoConvert.MIN_PARAGRAPH_WIDTH;
			}

			str = EvoConvert.formatParagraph(EvoConvert.extractElemText(node, normalDivWidth, quoteLevel, context), ltr, align, indent, whiteSpace, width, extraIndent, liText, quoteLevel);

			if (!liText && node.parentElement && (node.parentElement.tagName == "DIV" || node.parentElement.tagName == "P") &&
			    style.display == "block" && str != "" && node.previousSibling &&
			    ((node.previousSibling.nodeType == node.ELEMENT_NODE && node.previousSibling.tagName != "DIV" && node.previousSibling.tagName != "P" && node.previousSibling.tagName != "BR") ||
			    (node.previousSibling.nodeType == node.TEXT_NODE && EvoConvert.RemoveInsignificantNewLines(node.previousSibling, true) != ""))) {
				str = "\n" + str;
			}
		} else if (node.tagName == "PRE") {
			str = EvoConvert.formatParagraph(EvoConvert.extractElemText(node, normalDivWidth, quoteLevel, context), ltr, align, indent, "pre", -1, 0, "", quoteLevel);
		} else if (node.tagName == "BR") {
			// ignore new-lines added by wrapping, treat them as spaces
			if (node.classList.contains("-x-evo-wrap-br")) {
				if (node.hasAttribute("x-evo-is-space"))
					str += " ";
			} else {
				str = "\n";
			}
		} else if (node.tagName == "IMG") {
			str = EvoConvert.ImgToText(node);
		} else if (node.tagName == "A") {
			str = EvoConvert.processAnchor(node, context);
		} else {
			var isBlockquote = node.tagName == "BLOCKQUOTE";

			str = EvoConvert.extractElemText(node, normalDivWidth, quoteLevel + (isBlockquote ? 1 : 0), context);

			if (isBlockquote) {
				var ii, lines = str.split("\n"), prefix, suffix;

				prefix = ltr ? EvoConvert.getQuotePrefix(1, ltr) : "";
				suffix = ltr ? "" : EvoConvert.getQuotePrefix(1, ltr);

				str = "";

				for (ii = 0; ii < lines.length; ii++) {
					if (ii + 1 == lines.length && !lines[ii])
						break;

					str += prefix + lines[ii] + suffix + "\n";
				}
			}

			if ((!isBlockquote || !str.endsWith("\n")) &&
			    str != "\n" && ((style && style.display == "block") || node.tagName == "ADDRESS" || node.tagName == "TR")) {
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
 *
 * The link_to_text should be one of EvoConvert.E_HTML_LINK_TO_TEXT_... constants, if not
 * defined the 'EvoConvert.E_HTML_LINK_TO_TEXT_NONE' is assumed.
 */
EvoConvert.ToPlainText = function(element, normalDivWidth, link_to_text)
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

	var disconnectFromHead = false;

	if (!element.isConnected) {
		// this is needed to be able to use window.getComputedStyle()
		document.head.appendChild(element);
		disconnectFromHead = true;
	}

	var context = {
		link_to_text : link_to_text,
		append_refs : []
	};

	try {
		var uls, ols, str = "", ii;

		uls = element.getElementsByTagName("UL");
		ols = element.getElementsByTagName("OL");

		if (uls.length > 0 || ols.length > 0) {
			element = element.cloneNode(true);

			if (uls.length)
				EvoConvert.replaceList(element, "UL", normalDivWidth, context);

			if (ols.length)
				EvoConvert.replaceList(element, "OL", normalDivWidth, context);
		}

		for (ii = 0; ii < element.childNodes.length; ii++) {
			var node = element.childNodes.item(ii);

			if (!node)
				continue;

			str = EvoConvert.appendNodeText(node, str, EvoConvert.processNode(node, normalDivWidth, 0, context));
		}

		if (context.append_refs.length > 0) {
			if (!str.endsWith("\n"))
				str += "\n";
			str += "\n";

			for (ii = 0; ii < context.append_refs.length; ii++) {
				var prefix = "[" + (ii + 1) + "] ";
				if (context.append_refs[ii].label) {
					var indent = prefix.length;
					prefix += context.append_refs[ii].label.replace(/\r/g, "").replace(/\n/g, " ");
					if (normalDivWidth && normalDivWidth > 0 &&
					    normalDivWidth < (prefix.length + 1 + context.append_refs[ii].href.length)) {
						prefix += "\n" + " ".repeat(indent);
					} else {
						prefix += " ";
					}
				}
				str += prefix + context.append_refs[ii].href + "\n";
			}
		}
	} finally {
		try {
			if (disconnectFromHead)
				document.head.removeChild(element);
		} catch (err) {
		}
	}

	// to not add empty lines at the end of the text on re-editing
	if (str.endsWith("\n\n"))
		str = str.substr(0, str.length - 1);

	// remove EvoConvert.NOWRAP_CHAR_START and EvoConvert.NOWRAP_CHAR_END from the result
	return str.replace(/\x01/g, "").replace(/\x02/g, "");
}
