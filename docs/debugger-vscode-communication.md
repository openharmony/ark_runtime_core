# Communication between debugger and VSCode
Server is an application that receives commands through the socket and executes them in debugger.

Command `ark --debugger --debugger-port=<port_number> --debugger-library-path=<path_to_debugger_library>` launches debug server.

`--debugger-library-path` is an optional parameter.

VSCode provides us client interface and sending commands to the debugger with socket.

### Description of debug starting process
Server side
1. Server creates a TCP socket for communication with client.
2. Firstly server accepts a test client.
3. Secondly server accepts a real client and debug session begins.

Client side
1. Client runs a server other than waiting for a TCP socket.
2. It's critical for VSCode to have a debug server initialized at allocated time so we establish a test connection firstly to check it.
3. We create a test client every 100 milliseconds to check is socket ready for a debug session.
4. When the connection is established (if it is at the allocated time) test client disconnects and real client connects to server.
5. If connection was not established at the allocated time the `Can not launch debug server` error will be thrown.
