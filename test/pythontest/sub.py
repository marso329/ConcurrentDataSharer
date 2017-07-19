from ConcurrentDataSharer import ConcurrentDataSharer
import time
class testClass:
	var=10
	def __init__(self):
		pass
	def testFunc(self,data):
		print(data)
test=ConcurrentDataSharer("test")
time.sleep(1)
test1=ConcurrentDataSharer("test")
print("init OK")
test.setValue("hello","hello")
time.sleep(1)
name=test.getName()
def sub(data):
	print(data)
temp=testClass()
test1.subscribe(name,"hello",temp.testFunc)
print("subscription setup")
time.sleep(3)
test.setValue("hello","olleh")
time.sleep(5)
print("killed")

