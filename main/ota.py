import hashlib
import sys
import struct

def generate_ota_file(in_file, out_file):
    with open(in_file, 'rb') as inf:  
        with open(out_file, 'wb') as outf:
            outf.write('OTA\0')
            outf.write('\0' * 20)
            m = hashlib.md5()
            in_len = 0
            while True:
                data = inf.read(1024)
                if not data:
                    break
                in_len += len(data)
                m.update(data)
                outf.write(data)
            
            outf.seek(4)
            outf.write(struct.pack('<l', in_len))
            outf.write(m.digest())


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Not enough arguments!\n%s <bin file> <output ota file>" % (sys.argv[0]))
        sys.exit(1)
    generate_ota_file(sys.argv[1], sys.argv[2])

