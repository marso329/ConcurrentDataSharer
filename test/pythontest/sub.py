from ConcurrentDataSharer import ConcurrentDataSharer
import time
test=ConcurrentDataSharer("test")
time.sleep(1)
test1=ConcurrentDataSharer("test")
print("init OK")
test.setValue("hello","hello")
time.sleep(1)
name=test.getName()
def sub(data):
	print(data)
test1.subscribe(name,"hello",sub)
print("subscription setup")
time.sleep(3)
test.setValue("hello","olleh")
time.sleep(5)
print("killed")

