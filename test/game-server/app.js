var pomelo = require('pomelo');
var fs = require('fs');

/**
 * Init app for client.
 */
var app = pomelo.createApp();
app.set('name', 'test');

// app configuration
if (app.serverId === 'connector-server-1') {
  app.configure('production|development',  function(){
    app.set('connectorConfig',
      {
        connector : pomelo.connectors.hybridconnector,
        heartbeat : 3,
        useProtobuf: true,
        useDict: true
      });
  });
} else {
  app.configure('production|development',  function(){
    app.set('connectorConfig',
      {
        connector : pomelo.connectors.hybridconnector,
        heartbeat : 3,
        useProtobuf: true,
        useDict: true,
        ssl: {
          type: 'tls',
          key: fs.readFileSync('./server.key'),
          cert: fs.readFileSync('./server.crt'),
          ca: [fs.readFileSync('./server.crt')],
          handshakeTimeout: 5000
        } 
      });
  });
}

// start app
app.start();

process.on('uncaughtException', function (err) {
  console.error(' Caught exception: ' + err.stack);
});
