# How to run

The report details this as well.

```bash
make # only need to do this once of course
./p2p <Type> <args...>
```

i.e. an example would be...

```bash
make;
xterm -hold -title "Peer 2" -e "./p2p init 2 4 5 15" &
xterm -hold -title "Peer 4" -e "./p2p init 4 5 8 15" &
xterm -hold -title "Peer 5" -e "./p2p init 5 8 9 15" &
xterm -hold -title "Peer 8" -e "./p2p init 8 9 14 15" &
xterm -hold -title "Peer 9" -e "./p2p init 9 14 19 15" &
xterm -hold -title "Peer 14" -e "./p2p init 14 19 2 15" &
xterm -hold -title "Peer 19" -e "./p2p init 19 2 4 15" &
```
