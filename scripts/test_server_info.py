#!/usr/bin/env python3
import tango

db = tango.Database()
server_info = db.get_server_info("auxiliary_support_server/auxiliary")
print("Server info object type:", type(server_info))
print("\nAvailable attributes:")
for attr in dir(server_info):
    if not attr.startswith("_"):
        try:
            value = getattr(server_info, attr)
            if not callable(value):
                print(f"  {attr}: {value}")
        except:
            pass

