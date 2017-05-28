from ConcurrentDataSharer import ConcurrentDataSharer
test=ConcurrentDataSharer("test")
test1=ConcurrentDataSharer("test")
test.setValue("hello","hello")
name=test.getName()
def sub(data):
	print(data)
test1.subscribe("name","hello",sub)
