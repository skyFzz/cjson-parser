gcc -I../../include -fPIC -c dynamic_array.c -o dynamic_array.o
gcc -I../../include -fPIC -c map.c -o map.o
gcc -I../../include -fPIC -c ordered_map.c -o ordered_map.o

gcc -shared -fPIC dynamic_array.o -o ../../lib/libdynamic_array.so
gcc -shared -fPIC map.o -o ../../lib/libmap.so
gcc -shared -fPIC ordered_map.o -o ../../lib/libordered_map.so

