'use strict';

/* semi-convention: private functions start with lower-case letter,
   public functions start with upper-case letter. */

function EvoConvertToPlainText(element)
{
	if (!element)
		return null;

	return element.innerText;
}
