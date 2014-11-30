###blockiverse/

Blockiverse-extensions to minetest engine are contained in this folder.

I have basically written a ground-up server/client communication to use a lightweight
symmetrical communication protocol based on a value stack and method calls that
consume arguments from this stack and submit returned results upstream to the remote
endpoint's value stack.  The system somewhat resembles a simplistic form of what I
envision classic Forth would be had its environment been set up to work over a
point-to-point communications stream.

Object lifetimes are safely tracked, method calls are sanitized to prevent attempts
to call methods on bogus objects (it's checked at the endpoint whenever a method call
is received thus is secure in this regard).  The result is a very lightweight open-ended
distributed object protocol.  The protocol is symmetrical meaning both sides may send
values to the other and end submit method calls on object references previously
received from the other end.  On connection a handshake occurs where each end posts its
root object to the other in order to bootstrap the process.

Subsequent objects received must all ultimately originate from method calls initially
made by an endpoint on its received root object.

In Blockiverse the client creates a clientRoot whose reference is sent to the server and
likewise the server creates a serverRoot whose reference is sent to the client.

Within the protocol itself anything derived from bvnet::object may be transmitted to
the other end as an object reference once registered in the current session via
session.register().  The registration step implements the tracking that allows both
endpoints to track object availability and send messages upstream to the other end
when any objects get destructed to inform the endpoint that the object may no longer
be used.
