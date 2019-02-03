let fs = require("fs");

let file = fs.readFileSync("./out", "utf-8");
let log = console.log;

let lastbit = 1;
let bitlen = 999999;
let time0, time1;
for(let bit of file){
    if(bit != "1" && bit != "0") continue;

    bitlen++;
    if(lastbit == bit) continue;

    lastbit = bit;

    if(bit == 0){
        time1 = bitlen;
    }else{
        time0 = bitlen;
    }

    bitlen = 0;

    if(bit == 0){
        if(time1 > 100){
            log("--------------------------------");
        }else{
            if(time1 > time0){
                process.stdout.write("1");
            }else{
                process.stdout.write("0");
            }
        }
    }
}
