#!/bin/sh

ts=10

for cpus in 0-3 4-7; do
    xl vcpu-pin 0 all $cpus
    for cpu in 0 1 2 3; do
	echo powersave > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_governor;
	echo performance > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_governor;
    done;

    for gov in ondemand performance powersave; do
	for cpu in 0 1 2 3; do
	    echo $gov > /sys/devices/system/cpu/cpu$cpu/cpufreq/scaling_governor;
	done;

	for cores in 1 2 4 idle; do
	    echo $gov $cpus $cores
	    if [ $cores != idle ]; then
		cpuburn -t $ts -c $cores -u100 > burn-$gov-$cpus-c$cores.txt&
	    fi
	    svcur $ts > power-$gov-$cpus-c$cores.txt&
	    wait
	done
    done
done
