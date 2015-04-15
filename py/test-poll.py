#
#  Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
#  MIT Licensed.
#

import pomelo
import time
import os

def lc_callback(*args):
    if args[0] == Client.PC_LOCAL_STORAGE_OP_WRITE:
        lcstr = args[1]

        with open("pomelo.dat", "w") as f:
            f.write(lcstr)
        return 0

    else:
        if os.path.exists("pomelo.dat"):
            with open("pomelo.dat", "r") as f:
               return f.read()
        else:
            return None


def resp_callback(rc, resp):
    print 'request status: ', Client.rc_to_str(rc), 'resp:', resp

def notify_callback(rc):
    print 'notify status: ', Client.rc_to_str(rc)

def event_callback(ev, arg1, arg2):
    if ev == Client.PC_EV_USER_DEFINED_PUSH:
        print 'get push message, route: ', arg1, 'msg: ', arg2
    else:
        print 'network event:', Client.ev_to_str(ev), arg1, arg2

Client = pomelo.Client

Client.lib_init(Client.PC_LOG_WARN, None, None)

c = Client()

# disable tls, enable poll
c.init(False, True, lc_callback)

handler_id = c.add_ev_handler(event_callback)

c.connect('127.0.0.1', 3010)

time.sleep(1);

c.request('connector.entryHandler.entry', '{"name": "test"}', 10, resp_callback);

c.notify('test.testHandler.notify', '{"content": "test content"}', 10, notify_callback)

time.sleep(10)

c.poll()

c.rm_ev_handler(handler_id)

time.sleep(10)

ret = c.destroy()

Client.lib_cleanup()

