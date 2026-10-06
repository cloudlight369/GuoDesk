// v3.24.0 E2E 的本地下载源：只发固定字节，测完由脚本停掉进程
const http = require("http");
const port = parseInt(process.argv[2] || "8099", 10);
http.createServer((req, res) => {
  res.writeHead(200, { "Content-Type": "application/octet-stream" });
  res.end(Buffer.alloc(200000, 7));
}).listen(port);
