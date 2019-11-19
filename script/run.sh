#!/bin/tcsh -f

set sp = mht
set set = a

set sp_dir = ../data/wav/$sp

mkdir -p ../out/conf/$sp/$set

set f0_extract_method = reaper # see tool/config.py
mkdir -p ../out/{spec,ap,f0,if0,resyn}/$f0_extract_method/$sp/$set

foreach wav ( $sp_dir/$set/*.wav )
# foreach wav ( $sp_dir/$set/${sp}sd${set}01.wav )
	set spec			= ../out/spec/$f0_extract_method/$sp/$set/$wav:t:r.sp
	set f0				= ../out/f0/$f0_extract_method/$sp/$set/$wav:t:r.f0
	set if0				= ../out/if0/$f0_extract_method/$sp/$set/$wav:t:r.if0
	set ap				= ../out/ap/$f0_extract_method/$sp/$set/$wav:t:r.ap
	set conf			= ../out/conf/$sp/$set/$wav:t:r.conf
	set resyn			= ../out/resyn/$f0_extract_method/$sp/$set/$wav:t:r.wav
	./analysis.sh $wav $spec $f0 $ap $conf
	./synthesis.sh $spec $f0 $if0 $ap $conf $resyn
end



