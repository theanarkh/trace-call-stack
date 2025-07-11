let installed = false;
function install() {
    if (installed) {
        return;
    }
    installed = true;
    try {
        const addon = require('./build/Release/trace_call_stack.node');
        addon.install();
    } catch (error) {
        console.log(error);
    }
}
module.exports = {
  install
};          
