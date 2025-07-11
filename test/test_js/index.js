const { install } = require('../../src/trace_call_stack');

install();

function a() {
    b();
}

function b() {
    c();
}

function c() {
    for (let i = 0; i < 1000000000; i++) {
        
    }
}

while(1) {
    a();
}