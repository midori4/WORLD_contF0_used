# Analyze
./test -m 7 -i 001-sibutomo.wav --outf0 f0/001-sibutomo.f0 --outsp sp/001-sibutomo.sp --outap ap/001-sibutomo.ap --outconf conf/001-sibutomo.conf

# Synthesis
./test -o Syn_001-sibutomo.wav conf/001-sibutomo.conf f0/001-sibutomo.f0 sp/001-sibutomo.sp ap/001-sibutomo.ap
