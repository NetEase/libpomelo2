module.exports = function(app) {
  return new Handler(app);
};

var Handler = function(app) {
  this.app = app;
};

Handler.prototype.entry = function(msg, session, next) {
  console.error(msg);
  next(null, {code: 200, msg: 'game server is ok.'});
};

Handler.prototype.notify = function(msg, session, next) {
  next(null, null);
  console.log('get notify', msg);
}

