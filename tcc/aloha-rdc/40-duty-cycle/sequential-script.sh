#!/bin/bash

for i in {1..11}
do
    echo "Running sim aloha-rdc-$i.csc"
    java -Xmx4096m -jar /home/jonathan/contiki-aloha/tools/cooja/dist/cooja.jar -nogui="/home/jonathan/contiki-aloha/tcc/aloha-rdc/40-duty-cycle/aloha-rdc-${i}.csc" -contiki=/home/jonathan/contiki-aloha -random-seed=$RANDOM
done
