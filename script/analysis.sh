#!/bin/tcsh -f

set wav				= $argv[1] # input wav name
set spec			= $argv[2] # specroral name
set f0				= $argv[3] # f0 name
set ap				= $argv[4] # ap name
set conf			= $argv[5] # conf name

set ap_threshold = 0.0

# analysis
python ../tool/analysis.py -t $ap_threshold -s $spec -f $f0 -a $ap -c $conf -i $wav --world
