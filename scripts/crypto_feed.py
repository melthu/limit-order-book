#!/usr/bin/env python3
# Streams Binance.US BTC/USD depth20 snapshots to stdout, one JSON per line.
# Pipe into the C++ book driver:   python3 scripts/crypto_feed.py | ./build/crypto
# Needs: pip install websockets
import asyncio
import sys
import websockets

URL = "wss://stream.binance.us:9443/ws/btcusd@depth20@100ms"

async def main():
    async with websockets.connect(URL) as ws:
        while True:
            sys.stdout.write(await ws.recv() + "\n")
            sys.stdout.flush()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
