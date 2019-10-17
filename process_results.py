#!/usr/bin/python

import itertools
import collections
import re
import os.path as path

Power = collections.namedtuple("Power", ["time", "soc_e", "soc_p", "ca57_e", "ca57_p"])
Burn = collections.namedtuple("Burn", ["time", "cycles"])
Measurement = collections.namedtuple("Measurement", ["power", "burn"])

re_burn_cycles = re.compile("Total: (\d+) cycles.+")
re_burn_time = re.compile("Total time passed: ([0-9.]+) s.*")

re_pwr_time = re.compile("Total time: ([0-9.]+) .*")
re_pwr_soc = re.compile(".*SOC: ([0-9.]+) J \(([0-9.]+) W\).*")
re_pwr_ca57 = re.compile(".*CA57: ([0-9.]+) J \(([0-9.]+) W\).*")

def process_burn(data):
    lines = data.split("\n")
    match = re_burn_cycles.match(lines[-2])
    cycles = int(match.group(1))

    match = re_burn_time.match(lines[-3])
    time = float(match.group(1))

    return Burn(time, cycles)

def process_power(data):
    lines = data.split("\n")

    match = re_pwr_time.match(lines[-3])
    time = float(match.group(1))

    match = re_pwr_soc.match(lines[-3])
    soc_e = float(match.group(1))
    soc_p = float(match.group(2))

    match = re_pwr_ca57.match(lines[-3])
    ca57_e = float(match.group(1))
    ca57_p = float(match.group(2))

    return Power(time, soc_e, soc_p, ca57_e, ca57_p)

GOVS = ["ondemand", "powersave", "performance"]
CORES = ["1", "2", "4", "idle"]
CPUS = ["0-3", "4-7"]

def read_data(p):
    result = {}
    for gov in GOVS:
        result[gov] = dict()
        for core in CORES:
            result[gov][core] = dict()

    for gov in GOVS:
        for core in CORES:
            for cpu in CPUS:
                suffix = f"{gov}-{cpu}-c{core}.txt"
                with open(path.join(p, "power-"+suffix), "rt") as f:
                    data = f.read()
                power = process_power(data)

                if core == "idle":
                    burn = None
                else:
                    with open(path.join(p, "burn-"+suffix), "rt") as f:
                        data = f.read()
                    burn = process_burn(data)
                result[gov][core][cpu] = Measurement(power, burn)

    return result

def format_measurement(measurement):
    power = measurement.power
    burn = measurement.burn

    total_e = power.soc_e + power.ca57_e
    if burn:
        efficiency = 1000 * total_e / burn.cycles
    else:
        efficiency = "    -"

    return f" {power.soc_e:10} | {power.ca57_e:10} | {total_e:10.6} | {efficiency:10.6}"

def print_result(data):
    print(" {gov:14} {cpu:6} {core:6}  {soc_e:10} | {ca57_e:10} | {total_e:10} | {efficiency:10}".format(gov="governor",
      cpu = "cpus", core = "thrds", soc_e = "SOC, J",  ca57_e = "CA57, J", total_e = "Total J", efficiency = "Eff J/cycle" ))
    for gov in GOVS:
        for cpu in CPUS:
            for core in CORES:
                print(f" {gov:14} {cpu:6} {core:6} " + format_measurement(data[gov][core][cpu]))

def main():
    data = read_data("/srv/rcar-root/home/root/base-25u")
    print_result(data)

if __name__ == "__main__":
    main()
