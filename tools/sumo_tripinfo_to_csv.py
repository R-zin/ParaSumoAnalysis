#!/usr/bin/env python3
"""Convert SUMO tripinfo XML into the CSV layout consumed by the analyzers.

    vehicle_id,speed,travel_time,waiting_time,distance

Field mapping (per <tripinfo> record):
    vehicle_id   <- id
    speed        <- routeLength / duration   (mean trip speed, m/s)
    travel_time  <- duration                 (s)
    waiting_time <- waitingTime if present, else timeLoss  (s)
    distance     <- routeLength              (m)

stdlib-only (xml.etree) so it runs on any Python 3 without dependencies.

Usage:
    python3 tools/sumo_tripinfo_to_csv.py sumo/tripinfo.xml [out.csv]
    (writes to stdout when no output path is given)
"""

import sys
import xml.etree.ElementTree as ET


def convert(tripinfo_path, out):
    tree = ET.parse(tripinfo_path)
    root = tree.getroot()

    out.write("vehicle_id,speed,travel_time,waiting_time,distance\n")
    written = 0
    skipped = 0
    for rec in root.iter("tripinfo"):
        try:
            vid = rec.get("id", "")
            duration = float(rec.get("duration", "nan"))
            route_length = float(rec.get("routeLength", "nan"))

            # waitingTime is the direct waiting metric; older files carry
            # timeLoss instead, which is the closest available proxy.
            waiting_attr = rec.get("waitingTime")
            if waiting_attr is not None:
                waiting = float(waiting_attr)
            else:
                waiting = float(rec.get("timeLoss", "0.0"))

            if not vid or duration <= 0.0 or route_length < 0.0:
                skipped += 1
                continue

            speed = route_length / duration
            out.write(f"{vid},{speed:.4f},{duration:.4f},"
                      f"{waiting:.4f},{route_length:.4f}\n")
            written += 1
        except (ValueError, TypeError):
            skipped += 1

    print(f"converted {written} trips, skipped {skipped} malformed",
          file=sys.stderr)


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        sys.exit(__doc__)
    tripinfo = sys.argv[1]
    if len(sys.argv) == 3:
        with open(sys.argv[2], "w") as f:
            convert(tripinfo, f)
    else:
        convert(tripinfo, sys.stdout)


if __name__ == "__main__":
    main()
