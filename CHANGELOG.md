2015-06-30 / 0.3.5
====================
- revert commit 798b2504 and re-fix it
- refuse to send req/noti if not connected
- py: decrease refcount if req/noti failed

2015-06-30 / 0.3.4
====================
- do not reset {conn_pending|write_wait} queue before reconnecting
- fix a protential race condition bug

2015-06-30 / 0.3.3
====================
- fix a definitely race condition bug

2015-05-30 / 0.3.2
====================
- fix a definitely race condition bug
- fix serveral potential race condition bugs and tidy code

2015-05-21 / 0.3.1
====================
- fix a bug that leads reconnect failure for tls
- add null check for client_proto, etc.
- allow write_async\_cb to be invoked when NOT\_CONN
- stop check timeout for writing queue

2015-05-15 / 0.3.0
====================
- use cjson instead of jansson, cjson is more simple and bug-free
- some other bugfixes

2015-04-10 / 0.2.1
====================
- upgrade libuv to 1.4.2 
- multi bugfix

2015-03-02 / 0.2.0
====================
- compile: enable -fPIC by default
- cs: add c# binding, Thanks to @hbbalfred
- bugfix: fix a fatal bug for tcp__handshake_ack

2015-02-02 / 0.1.7
====================
- dummy: fix a double-free bug for dummy transport
- tls: use tls 1.2 instead of ssl 3

2015-01-30 / 0.1.6
====================
- protocol: fix protobuf decode for repeated string
- accept a pull request which makes compiling more friendly for android platform
- client: remove event  first before firing the event
- poll: adding is_in_poll to avoid poll recursion
- client: fix a bug that leads to coredump when resetting

2014-11-11 / 0.1.5
====================
- client: don't poll before request/notify
- java, py: fix binding code bug
- refactoring: rename field name ex_data to ls_ex_data of struct pc_config_t
- tls: fix incorrect event emitting when cert is bad
- tls: update certificate for test case
- tr: destroy the mutex after uv loop exit
- tls: more log output by info_callback
- tls: more comment 

2014-10-31 / 0.1.4
====================
- py: fix protential deadlock for python binding
- reconn: fix incorrect reconn delay calc
- bugfix: init tcp handle before dns looking up

2014-10-15 / 0.1.3
=====================
- bugfix: typo for = <-> ==
- bugfix: fix warnings for multi-platform compilation
- clean code

2014-10-10 / 0.1.2
=====================
- set timeout to PC_WITHOUT_TIMEOUT for internal pkg
- bugfix: freeaddrinfo should be called after connect
- jansson: make valgrind happy
- bugfix: incorrent init for uv_tcp_t, this leads memory leak

2014-09-30 / 0.1.1
=====================
- misc bug fix

2014-09-03 / 0.1.0
=====================

- release first version
