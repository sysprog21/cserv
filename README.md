# cserv

`cserv` is an event-driven and non-blocking web server. It ideally has one
worker process per cpu or processor core, and each one is capable of handling
thounds of incoming network connections per worker. There is no need to create
new threads or processes for each connection.

I/O multiplexing is achieved using [epoll](http://man7.org/linux/man-pages/man7/epoll.7.html).

## Features

* Single-threaded, non-blocking I/O based on event-driven model
* Multi-core support with processor affinity
* Implement load balancing among processes
* Hook and optimize blocking socket system calls
* Faciliate coroutines for fast task switching
* Efficient timeout handling
* Builtin memcache service 

## High-level Design

```text
+----------------------------------------------+
|                                              |
|  +-----------+   wait   +-----------------+  |  copy   +---------+
|  |           +---------->                 +------------>         |
|  | IO Device |    1     | Kernel's buffer |  |   2     | Process |
|  |           <----------+                 <------------+         |
|  +-----------+          +-----------------+  |         +---------+
|                                              |
+----------------------------------------------+
```

## Build from Source

At the moment, `cserv` supports Linux based systems with epoll system call.
Building `cserv` is straightforward.
```shell
$ make
```

## Usage

Start the web server.
```shell
$ ./cserv start
````

Stop the web server.
```shell
$ ./cserv stop
```

Check the configurations.
```shell
$ ./cserv conf
```

By default the server accepts connections on port 8081, if you want to assign
other port for the server, modify file `conf/cserv.conf` and restart the server.

## License
`cserv` is released under the MIT License. Use of this source code is governed
by a MIT License that can be found in the LICENSE file.
