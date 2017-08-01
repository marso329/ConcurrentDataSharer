import time
from ConcurrentDataSharer import ConcurrentDataSharer
test=ConcurrentDataSharer("test")
test.setValue("hello","hello")
time.sleep(100)
