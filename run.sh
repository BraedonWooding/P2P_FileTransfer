#!/bin/sh

Xterm -hold -title "Peer 2" -e "./p2p init 2 4 5 5" &
Xterm -hold -title "Peer 4" -e "./p2p init 4 5 8 5" &
Xterm -hold -title "Peer 5" -e "./p2p init 5 8 9 5" &
Xterm -hold -title "Peer 8" -e "./p2p init 8 9 14 5" &
Xterm -hold -title "Peer 9" -e "./p2p init 9 14 19 5" &
Xterm -hold -title "Peer 14" -e "./p2p init 14 19 2 5" &
Xterm -hold -title "Peer 19" -e "./p2p init 19 2 4 5" &

kill $!