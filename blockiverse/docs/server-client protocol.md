
#Server/Client protocol

opcode overview:
```
         int: [+|-] N db_1 ... db_2^N
       float: f [-] [<whole digits>] [.<fraction digits>] [e<exponent digits>] ;
        blob: b len data
      string: " len data
   objectref: o id
 method call: . id method
 object gone: ~ id
 dmc message: : id len name midx
      atomic: ( <params> <method call> )
```
- db is a raw databyte
- len is a LE 32-bit unsigned integer
- id is LE 32-bit unsigned integer object id
- midx is LE 32-bit unsigned integer method index
- name is a string (preceeding len is length)
- method is LE 32-bit unsigned integer method index

debug mode:
when built debug all binary values (except blobs)
will be indicated as hexadecimal representations matching the
length of data involved
0xNNNNNNNN for #, 0xNN for databytes

atomic blocks:

If the endpoints were to invoke a method call with arguments then
concurrnecy issues may occur as parameters of endpoint B's method
call get interpreted by enpoint A as return values to its current
method call (or vice versa).  To prevent such problems one endpoint
(the server by convention since its calls tend to be preemptive in
nature) brackets its calls with the atomic operators ( and ) which
tells the receiver not to use the normal argument stack to hold the
bracketed values.  This stack is destroyed when the closing ) is
encountered.  Atomic blocks are meant to make preemptive (out of
band) informational calls to 'sink' type methods that may accept
arguments but may not push return values.

NB: Method calls that neither accept parameters nor generate
return values do not need to use atomic blocks.

*** TODO ***
The current protocol implementation does not yet implement
atomic blocks.

dynamic method call (DMC):

Objects may be polymorphic (as a derived class during program
compilation) and may also add additional methods at runtime.

Whenever the object first becomes valid at one endpoint an
initial set of dmc messages will convey the initial interface
to the other endpoint.

If the object instance polymorphs further at runtime then
additional dmc messages may be emitted whenever such changes
occur.  Instances may only add new methods to the existing
contract, never remove an existing method.

##datatypes

decimals:  f [-] [ &lt;whole digits&gt; ] [ . &lt;fraction digits&gt; ] [ e &lt;exponent digits&gt; ] ;

eg: -23.51e7 is the stream: f - 2 3 . 5 7 e 7 ;

NB: no digits (ie: f ;) is the value 0.0e0

integer: [+|-] N db_1 ... db_2^N

N is a single digit and indicates 2^N bytes follow
db_x are the corresponding databytes

examples (representing the value 1 where the hex values represent raw binary bytes):
```
         char (2^0=1 byte ): + 0 0x01
        short (2^1=2 bytes): + 1 0x01 0x00
         long (2^2=4 bytes): + 2 0x01 0x00 0x00 0x00
    long long (2^3=8 bytes): + 3 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00
             ...
    maxsize (2^9=512 bytes): + 9 0x01 [511 0x00's]
```
unsigned integers omit the +

integer data is considered magnitude-only (not 2's complemented) so + and -
are ncessary to indicate presence of a sign (will get converted to appropriate
endpoint's signed representation)

NB: current blockiverse implementation only uses range 0 &lt;= N &lt;= 3 (64-bit ints)

integers are little endian in support of the protocol's iterative pattern

len is same as #

the # indicates a raw (binary) 32-bit unsigned little-endian
value which is considered by o to be an object ID
```
binary blob: b len data
     string: " len data
  objectref: o #
```
##opcodes

datavalues push onto a stack when encountered on the agent's inbound stream

a method call is the steam pattern:
```
. id method
```

so the stream:
```
. 0x01 0x00 0x00 0x00 0x10 0x00 0x00 0x00
```
calls method 0x00000010 (16) of object 0x00000001 (1)

methods called pull their arguments from values stacked prior
to the methodcall

return values from method will be pushed is values back
to the caller

so objectref.method(a,b,c) looks like:
&lt;c&gt; &lt;b&gt; &lt;a&gt; &lt;objectref&gt; &lt;method&gt; .

stack empty when an argument is required raises an error condition

the pattern:
```
~ id
```
indicates a previous object no longer exists
well designed client and server should send such messages whenever
objects get destructed (go out of parent's code scope, are delete'ed, etc)

receipt of a method call on a destructed object raises an error condition

as a security measure the protocol stack (both ends) only accept incoming
objectrefs of objects it has already pushed to the other end (the endpoint
keeping a list of such active objects) and will raise an error condition
upon encountering any objectrefs in the stream not previously given
to the other end.  any objects destroyed (and reported with ~ see above)
are removed from the above list so subsequent method calls on such
objects also raises an error.

method 0 of any valid objectref is a contract/type/class/interface specifier

the return value may be checked to determine an appropriate type object
has been received in a method or that is of the appropriate version

there is no destruct method.  it is considered the responsibility of the giver
to manage lifetimes and issue destructor messages when given objects expire.

NB: deletion of all given objects is considered as a connection termination
at this point no communication is possible since no valid method calls exist
past that point (not even to log in).  the disconnect method on either bootstrap
object (see handshake below) is also sufficient for one end or the other
to cleanly terminate the connection

## handshake

the client and server each have their own bootstrap object that is immediately
given to the other via the outbound stream upon connection as an objectref.

all subsequent interaction is through method calls on the received bootstrap object
or any objects subsequently received from the other end during the session.
