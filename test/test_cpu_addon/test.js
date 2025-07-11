const { compute } = require('./build/Release/test.node');
const { install } = require('../../src/trace_call_stack');

install();
compute();