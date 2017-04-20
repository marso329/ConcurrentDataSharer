import time
from ConcurrentDataSharer import ConcurrentDataSharer
test=ConcurrentDataSharer("test")
print(test.getClients())
time.sleep(10)
