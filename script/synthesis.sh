#!/bin/tcsh -f

set world			= ../tool/WORLD/Release/test # world
set spec			= $argv[1] # specroral name
set f0				= $argv[2] # f0 name
set if0				= $argv[3] # interpolated f0 in logarithm name
set ap				= $argv[4] # ap name
set conf			= $argv[5] # conf name
set resyn			= $argv[6] # vocoded wav

# interpolation f0 in logarithm
./interp_f0_in_logarithm.pl $f0 | x2x +af > $if0 

# synthesis
$world --castfloat -o $resyn $conf $if0 $spec $ap
