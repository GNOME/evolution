#!/usr/bin/perl

$maj = 0;
$min = 0;
while (<>) {
  if (/xerces-c(\d+)_(\d+)\.so$/) {
    if ($1 > $maj || (\\$1 == $maj && \$2 > $min)) {
      $maj = $1;
      $min = $2;
    }
  }
}
print $maj,"_",$min
