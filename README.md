### aoip-core

This repository is under development.

```bash
$ mkdir Build
$ cd Build 
$ cmake ..
$ make

# save the multicast stream in an output.wav file
$ ./capture
ctrl-c

# ptpv2 client (two-step)
$ ./simple-ptpc
Detected a PTPv2 Announce message. ptp_server_id=xxxxfffe512378
Synced. sequence=54781, offset=156832339608160
Synced. sequence=55037, offset=156832155048241
Synced. sequence=55293, offset=156830966310995
Synced. sequence=55549, offset=156832531048756
Synced. sequence=55805, offset=156833341937846
Synced. sequence=56061, offset=156833763767357
^C
```

