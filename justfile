build:
    mkdir -p build
    gcc scr/node.c -o build/node
    gcc scr/server.c -o build/server
    gcc scr/client.c -o build/client

run:
    ./build/node 9091 &
    ./build/node 9092 &
    ./build/server &
    ./build/client /home/alekc/OCuCP/tar_work_dir/Kot_A.A./course-work/scr/file

part_clean:
    rm -f sorted_*.txt
    rm -f final_sorted.txt
    rm -f received_file.txt
    rm -f part_*.txt

clean:
    rm -f sorted_*.txt
    rm -f final_sorted.txt
    rm -f received_file.txt
    rm -f part_*.txt
    rm -f sorted_file
    rm -f build/*
