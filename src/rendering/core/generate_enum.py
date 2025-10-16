import re 
import sys

if len(sys.argv) != 2:
    print("Expected path to enum header", file=sys.stderr)
    exit(-1)

enum_file = open(sys.argv[1], 'rt').read()

texture_enum = re.search(r'enum class TextureFormat {\n([ \w,\n]*)\n};', enum_file, re.DOTALL).groups()[0]

texture_enum = texture_enum.replace(' ', '').split('\n')

print(texture_enum)
