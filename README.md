# qtcp

This is a library that can open raw TCP connections for a q process to communicate through. It serves the same purpose as my other project ```qtcpproxy``` but this is in-process.
The reason I took so long to make this is that using sd1/sd0 (which is required for making proper worker threads) on Windows is not trivial.

See tcp.q and test.q for example usage. The functions labelled as "callback" can be redefined to provide custom behavior.

# build

Windows only: create `config.cmd` based on the example and run `b32.cmd` and/or `b64.cmd` (requires k.h and c.dll from the KxSystems/kdb project)

# API

* ```.tcp.connect[alias;host;port]```: connect to the given host:port. The alias can be used to identify the connection in the success/failure callbacks.
* ```.tcp.listen[alias;port]```: listen on the given port. The alias can be used to identify the connection in the success/failure callbacks.
* ```.tcp.send[handle;msg]```: send message on socket
* ```.tcp.close[handle]```: close socket
* ```.udp.listen[alias;port]```: listen on the given port. The alias can be used to identify the connection in the success/failure callbacks.
* ```.udp.send[handle;host;port;msg]```: send UDP message.

## callback
* ```.tcp.connFailed[alias;msg]```: called when the connection fails, gets the alias from the .tcp.connect call and the error message
* ```.tcp.connSuccess[alias;handle]```: called when a connection succeeds, gets the alias and the handle
* ```.tcp.listenFailed[alias;msg]```: called when listening fails, gets the alias from the .tcp.connect call and the error message
* ```.tcp.listenSuccess[alias;handle]```: called when listening succeeds, gets the alias and the handle
* ```.tcp.disconnect[handle]```: called when a connection is disconnected
* ```.tcp.receive:[handle;msg]```: called when socket receives a message
* ```.tcp.clientConnect:[listenHandle;handle;address]```: called when a client connects to the listening server
* ```.udp.listenSuccess[alias;handle]```: called when UDP listening succeeds, gets the alias and the handle
* ```.udp.receive[handle;host;port;msg]```: called when UDP socket receives a message
