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
