import os


out = '/home/wangzhe/cxx/in/in'

if os.path.exists(out):
	os.remove(out)

_size = 64*1024

base = [i&0xff for i in range(1024)]

print(len(base))

with open(out, 'wb') as fo:
	for i in range(_size):
		fo.write(bytearray(base))
