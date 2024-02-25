#!/bin/bash

i=${ID}

echo "Running sim aloha-rdc-$i.csc"
java -Xmx4096m -jar /home/jonathan/contiki-aloha/tools/cooja/dist/cooja.jar -nogui="/home/jonathan/contiki-aloha/tcc/csma/csma-${i}.csc" -contiki=/home/jonathan/contiki-aloha -random-seed=$RANDOM