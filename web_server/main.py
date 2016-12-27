import machine
import socket

pins = [machine.Pin(i, machine.Pin.OUT) for i in (0, 2, 4, 5, 12, 13, 14, 15)]

pins[1].value(1)

html = """<!DOCTYPE html>
<html>
    <head> <title>ESP8266 Pins</title> </head>
    <body> <h1>ESP8266 Pins</h1>
        <table border="1"> <tr><th>Pin</th><th>Value</th></tr> %s </table>
    </body>
</html>
"""


import uasyncio as asyncio

@asyncio.coroutine
def serve(reader, writer):
    print(reader, writer)
    print("================")
    print((yield from reader.read()))
    yield from writer.awrite("HTTP/1.0 200 OK\r\n\r\nHello.\r\n")
    print("After response write")
    yield from writer.aclose()
    print("Finished processing request")


import logging
#logging.basicConfig(level=logging.INFO)
logging.basicConfig(level=logging.DEBUG)
loop = asyncio.get_event_loop()
loop.call_soon(asyncio.start_server(serve, "0.0.0.0", 80))
loop.run_forever()
loop.close()

print('all done')

'''
addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]

s = socket.socket()
s.bind(addr)
s.listen(1)

print('listening on', addr)


while True:
    cl, addr = s.accept()
    print('client connected from', addr)
    cl_file = cl.makefile('rwb', 0)
    while True:
        line = cl_file.readline()
        if not line or line == b'\r\n':
            break
    rows = ['<tr><td>%s</td><td>%d</td></tr>' % (str(p), p.value()) for p in pins]
    response = html % '\n'.join(rows)
    cl.send(response)
    cl.close()
    cl_file.close()
'''
