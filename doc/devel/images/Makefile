
# this is only a temporary makefile

# you need ps2eps, groff, pnmutils, and ghostscript installed

PICS=e-config-flow.pic e-popup-merge-1.pic e-popup-merge-2.pic
EPS=$(PICS:.pic=.eps)
PNG=$(PICS:.pic=.png)

all: $(PNG)

%.eps: %.pic
	groff -p $^ | ps2eps -f -l > $@

%.png: %.pic
	groff -p $^ \
	| gs -q -dSAFER -dNOPAUSE -dBATCH -r180 -sDEVICE=pnmraw -sOutputFile=- - -c quit \
	| pnmcrop \
	| pnmscale 0.5 \
	| pnmtopng > $@

