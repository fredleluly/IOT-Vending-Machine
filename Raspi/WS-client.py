import asyncio
import websockets

async def connect():
    uri = "ws://192.168.4.1:81"
    async with websockets.connect(uri) as websocket:
        while True:
            message = await websocket.recv()
            print(message)
            # kirim perintah jika perlu
            # await websocket.send("toggleRelay")

asyncio.run(connect())