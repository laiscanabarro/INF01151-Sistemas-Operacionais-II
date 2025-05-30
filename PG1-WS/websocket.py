import asyncio
import os
import websockets

from asyncio import run
from asyncio import Future

from websockets import serve


async def http_handler(path, headers):
    """Route HTTP requests to their handlers"""
    from http import HTTPStatus
    from websockets.http import Headers

    base_dir = os.path.dirname(os.path.abspath(__file__))

    if path == '/ui-chat':
        # Entregar para o navegador o conteúdo do arquivo chat.html,
        # que corresponde ao cliente chat implementado pelo webservice
        with open(os.path.join(base_dir, "chat.html")) as f:
            headers = Headers(**{'Content-Type': 'text/html'})
            body = bytes(f.read(), 'utf-8')

            return HTTPStatus.OK, headers, body

    elif path == '/ui-echo':
        # Entregar para o navegador o conteúdo do arquivo echo.html,
        # que corresponde ao cliente echo implementado pelo webservice
        with open(os.path.join(base_dir, "echo.html")) as f:
            headers = Headers(**{'Content-Type': 'text/html'})
            body = bytes(f.read(), 'utf-8')

            return HTTPStatus.OK, headers, body
        
    else:
        return None


async def echo(websocket):
    """Echo WebSocket handler"""
    async for message in websocket:
        await websocket.send(message)

async def chat(websocket, sessions={}):
    """Chat WebSocket handler"""
    remote = websocket.remote_address
    sessions[remote] = websocket

    logs_message = f"Cliente {remote} se conectou"
    for socket in sessions.values():
        await socket.send(logs_message)

    try:
        async for message in websocket:
            await websocket.send(message)
            logs_message = f"Cliente {remote} enviou: {message}"
            for socket in sessions.values():
                if socket != websocket:
                    await socket.send(logs_message)

    finally:
        if remote in sessions:
            del sessions[remote]
        logs_message = f"Cliente {remote} se desconectou"
        for socket in sessions.values():
            await socket.send(logs_message)

async def web_socket_router(websocket, path):
    """Route WebSocket requests to their handlers"""
    if path == '/':
        await websocket.close(reason=f'needs a path')
    elif path == '/echo':
        await echo(websocket)
    elif path == '/chat':
        await chat(websocket)
    else:
        await websocket.close(reason=f'path not found: {path}')


async def main():
    async with serve(web_socket_router, "localhost", 8080, process_request=http_handler):
        await Future()


run(main())
