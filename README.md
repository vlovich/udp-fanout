This is similar to samplicate.  It has fewer options, but supports dynamic registration/deregistration via an admin port.

Given a source port (currently hard-coded to syslog port of 514), it will copy all incoming packets
to the registered listeners.

The admin port (currently hard-coded to 10000) receives control messages.

To register a listener:

echo -n register | socat - udp-sendto:localhost:10000,sourceport=35656

will register the current machine's port 35656 as a receiver.

To unregister a listener:

echo -n unregister | socat - udp-sendto:localhost:10000,sourceport=35656

If you spoof the source UDP address, then you can also register on behalf of other people.
