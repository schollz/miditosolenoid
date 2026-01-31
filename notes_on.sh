#!/bin/bash

# loop from 60 to 68
for i in {60..72}
do
NOTE=$i make midi-note-on 
echo "Played MIDI note $i"  
sleep 0.5
# NOTE=$i make midi-note-off 
# sleep 0.5
done