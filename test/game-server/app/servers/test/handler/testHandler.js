module.exports = function(app) {
  return new Handler(app);
};

var Handler = function(app) {
  this.app = app;
};

Handler.prototype.notify = function(msg, session, next) {
  console.log('get msg:', msg);

  var channelService = this.app.get('channelService');

  channelService.broadcast('connector', 'onPush', {content: 'test content', topic: 'test topic', id: 42});

  next(null); 
};

