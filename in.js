const dgram = require('dgram');
const server = dgram.createSocket('udp4');
const fs = require("fs");

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

function checksum(slice){
    let total = 0;
    for(let v of slice){
        total += v;
    }
    return (total % 0x100);
}

server.on('message', (msg, rinfo) => {
    const packetSize = 11;
    for(let i = 0; i < msg.length; i += 11){
        let block = msg.slice(i, i + 11);
        if(block[0]){
            let c = checksum(block.slice(1, 9))
            let e = block[9]
            if(c !== e){
                console.log("bad checksum" + i)
                continue;
            }
        }
        fs.writeFileSync("./ram/" + i / 11, block);
    }
});

server.on('listening', () => {
  const address = server.address();
  console.log(`server listening ${address.address}:${address.port}`);
});

server.bind(5555);
