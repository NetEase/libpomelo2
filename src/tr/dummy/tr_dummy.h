/**
 * Copyright (c) 2014,2015 NetEase, Inc. and other Pomelo contributors
 * MIT Licensed.
 */

#ifndef TR_DUMMY_H
#define TR_DUMMY_H

#include <pomelo_trans.h>

pc_transport_plugin_t* pc_tr_dummy_trans_plugin();

#define TR_DUMMY_RESP ("{\"msg\": \"dummy msg\"")

#endif
