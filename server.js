const WebSocket = require("ws");
const server = new WebSocket.Server({ port: 8080 }); 

server.on("connection", (ws) => {
  console.log("✅ A client connected (ESP32 or Browser)");

  ws.on("message", (msg) => {
    const messageString = msg.toString();
    console.log("📩 Received message, broadcasting:", messageString); 

    // Broadcast ang mensahe sa LAHAT ng clients
    server.clients.forEach(client => {
      if (client.readyState === WebSocket.OPEN) {
        client.send(messageString);
      }
    });
  });

  ws.on("close", () => console.log("❌ A client disconnected"));
  ws.on("error", (err) => console.error("⚠️ WebSocket error:", err));
});

console.log("🚀 WebSocket server running at ws://192.168.8.102:8080");