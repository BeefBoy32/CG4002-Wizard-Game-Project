import time
import sys

for i in range(10):
    print(f"Counter: {i}", end="\r")   # overwrite the line
    # sys.stdout.flush()                 # force refresh
    time.sleep(1)

print("\nDone!")  