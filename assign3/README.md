NetProg_Assign3


===============

|<------14--------->|<----60----->|<--1440-->|
----------------------------------------------
| Ethernet header   | ODR Header  | Payload  |
----------------------------------------------


Policy to Update in Routing table:
    1. With every packet, we get we look in routing table for that src in packet
      if we can update an entry. If entry is with lower broadcast id or lower 
      hopcount, we update it.
    2. We check if entry has not become stale.
    3. If staleness param is set, we remove entries for both src and dest in table





