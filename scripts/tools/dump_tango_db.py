#!/usr/bin/env python3
import json
import tango
from typing import Dict, Any


def main():
    db = tango.Database()

    # Servers
    servers = db.get_server_list()
    print("=== Servers ===")
    for s in servers:
        print(" ", s)

    # Devices
    devices = db.get_device_name("*", "*")
    print("\n=== Devices ===")
    for d in devices:
        print(" ", d)

    # Device properties
    print("\n=== Device Properties ===")
    def to_serializable(val):
        try:
            json.dumps(val)
            return val
        except TypeError:
            # tango returns list-like std::vector wrappers; cast to list/str
            if isinstance(val, (list, tuple)):
                return [to_serializable(v) for v in val]
            return str(val)

    for d in devices:
        prop_list_datum = db.get_device_property_list(d, "*")
        # Extract the list of property names from the DbDatum
        prop_list = list(prop_list_datum.value_string)
        
        if not prop_list:
            print(f"\n[{d}]")
            print("{}")
            continue

        raw_props: Dict[str, Any] = db.get_device_property(d, prop_list)
        props = {k: to_serializable(v) for k, v in raw_props.items()}
        print(f"\n[{d}]")
        print(json.dumps(props, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()


