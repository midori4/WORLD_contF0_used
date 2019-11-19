#!/usr/bin/perl

@f0 = split(/\n/, `x2x +fa $ARGV[0]`);

push(@start_frame, 0);
push(@start_f0, 0);

$n = 0;
for ($t=1; $t<@f0; $t++) {
    if ($f0[$t-1] == 0 && $f0[$t] > 0) {
        $end_frame[$n] = $t;
        $end_f0[$n] = $f0[$t];
        $n++;
    }
    if ($f0[$t] > 0 && $f0[$t+1] == 0) {
        $start_frame[$n] = $t;
        $start_f0[$n] = $f0[$t];
    }
}

push(@end_frame, $t-1);
push(@end_f0, 0);

@out_f0 = @f0;

for ($n=0; $n<@start_frame; $n++) {
    for ($t=$start_frame[$n]; $t<=$end_frame[$n]; $t++) {
        if ($n == 0) {
            $out_f0[$t] = $end_f0[0];
        } elsif ($n == $#start_frame) {
            $out_f0[$t] = $start_f0[$#start_f0];
        } else {
            $out_f0[$t] = exp((log($end_f0[$n]) - log($start_f0[$n])) / ($end_frame[$n] - $start_frame[$n]) * ($t - $start_frame[$n]) + log($start_f0[$n]));
        }
    }
}

for ($t=0; $t<@f0; $t++) {
    print "$out_f0[$t]\n";
}
