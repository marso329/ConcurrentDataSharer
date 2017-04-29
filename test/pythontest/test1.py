from ConcurrentDataSharer import ConcurrentDataSharer
test=ConcurrentDataSharer("test")
test.setValue("hello","dadawdaw")
print(test.getValue("hello"))
