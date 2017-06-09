'use strict';

if (process.platform !== 'win32') return;

const hookMouse = require('bindings')('ll_mouse_hooks');

let runID = 1;

module.exports = {
  listen: (cb) => {
    const cID = runID;
    runID += 1;
    hookMouse.stop();

    setTimeout(() => {
      if (runID - 1 !== cID) return;
      hookMouse.run((event) => {
        const details = event.split('::');
        if (details.length === 1) {
          cb(details[0]);
        } else {
          cb(details[0], parseInt(details[1], 10), parseInt(details[2], 10));
        }
      });
    }, 0);
  },
  stop: () => hookMouse.stop(),
};
