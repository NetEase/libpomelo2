module.exports = function(app) {
  return new Handler(app);
};

var Handler = function(app) {
  this.app = app;
};

Handler.prototype.entry = function(msg, session, next) {
  console.log('get msg:', msg);

  next(null, {code: 200, msg: 'game server is ok.'});
};

