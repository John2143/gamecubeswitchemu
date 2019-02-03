const dgram = require('dgram');
const server = dgram.createSocket('udp4');
const fs = require("fs");

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
    for(let i = 0; i < msg.length; i += 9){
        fs.writeFileSync("./ram/" + i / 9, msg.slice(i, i + 9));
    }
});

server.on('listening', () => {
  const address = server.address();
  console.log(`server listening ${address.address}:${address.port}`);
});

server.bind(5555);
