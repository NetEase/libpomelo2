import pomelo
import time

Client = pomelo.Client
Client.lib_init(Client.PC_LOG_INFO, None, None)

def resp_callback(rc, resp):
    print 'request status: ', Client.rc_to_str(rc), 'resp:', resp

def notify_callback(rc):
    print 'notify status: ', Client.rc_to_str(rc)

def event_callback(ev, arg1, arg2):
    if ev == Client.PC_EV_USER_DEFINED_PUSH:
        print 'get push message, route: ', arg1, 'msg: ', arg2
    else:
        print 'network event:', Client.ev_to_str(ev), arg1, arg2

c = Client()

# disable tls, enable poll
c.init(False, True)

c.connect('127.0.0.1', 3010)

c.request('connector.entryHandler.entry', '{"name": "test"}', 10, resp_callback);

c.notify('test.testHandler.notify', '{"content": "test content"}', 10, notify_callback)

c.add_ev_handler(Client.PC_EV_USER_DEFINED_PUSH, "onPush", event_callback)

time.sleep(10)

c.poll()

ret = c.destroy()

Client.lib_cleanup()
