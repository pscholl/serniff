from serial import *
from collections import namedtuple
from struct import Struct

def sniff(channel):
    s = Serial('/dev/ttyUSB0', 115200, timeout=.2, parity=PARITY_NONE,
               stopbits=1, bytesize=8, rtscts=1, dsrdtr=0)

    s.open()
    s.write("%d\n"%channel)

    # synchronize to sender, by consuming the correct answer
    toread = "started on channel %d\r\n"%channel
    while len(toread) != 0:
        if toread[0] == s.read():
            toread = toread[1:]
        else:
            toread = "started on channel %d\r\n"%channel

    # packets will always be returned at maximum size, so read until there
    # is enough data before yielding (which is similar to returning, but when
    # called again the code stays in the loop) the buf back to the caller.
    buf = []
    while 1:
        buf.extend (s.read(ps.size - len(buf)))
        if len(buf) == ps.size:
            yield buf
            buf = []
        else:
            yield []

# create a named struct and start rx'ing packets
ps = Struct('!BBHQBBHQBBBB118sH')
p  = namedtuple('mac', 'src_addrmode src_pad src_panid src_addr\
                        dst_addrmode dst_pad dst_panid dst_addr\
                        linkquality securityuse aclentry sdu_length\
                        sdu pad')

# This is the main loop, first all channels on the ieee802.15.4 are scanned,
# until a packet is received on any channel. We a packet on a channel has been
# received, all packets there will be printed. 
channel = 0
while 1:
    for chan in range(11, 26):
        print "ch: %d"%chan
        for packet in sniff(chan):
            if packet != []:
                channel = chan
            break

        if channel != 0:
            break

    if channel != 0:
        break

for buf in sniff(channel):
    if buf != []:
        # packet will be a namedtuple containing the packet and its
        # attributes. The payload can be found in the "sdu" attribute, its
        # length in "sdu_length", i.e.
        #  print packet.sdu_length, packet.sdu
        # will print the payload of the packet and its length.
        packet = p._make(ps.unpack(''.join(packet)))
        print "ch: %d, %s"%(chan,packet))
