#!/usr/bin/perl

# Tiny Perl script to check the mozilla version. I don't even like Perl.
# to make things easier on myself, I'm just going to treat the mozilla version as base 10. This will probably break

# Author: Andrew Chatham

$ver = 0;
while (<>) {
  if (/useragent.misc\", \"rv:([0-9]+)\.([0-9]+)\.([0-9]+)/) { $ver = $1 * 100 + $2 * 10 + $3; }
}
print $ver;
