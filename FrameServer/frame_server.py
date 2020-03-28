import zmq
from random import randrange
import time

port = 5558
context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind( "tcp://*:{}".format(port))

memory = {}

def generate_image(width, height, bits):
		
		
	f = "{}x{}x{}".format(width, height, bits)
	if f in memory:
		print("Returning memorized {}".format(f))
		return memory[f]
		
	ba = bytearray()
	if bits == 32:
		for i in range(width*height):
			ba.append(0xaa)
			ba.append(0x00)
			ba.append(0xaa)
			ba.append(0xaa)
			
	if bits == 24:
		for i in range(width*height):
			ba.append(0x00)
			ba.append(0xff)
			ba.append(0xff)
			
				
			
	memory[f] = ba
	return ba
	
	
while True:
	#  Wait for next request from client
	message = socket.recv()
	print("Received request: {}".format( message))
	
	parts = message.decode().split(" ")
	if len(parts) >= 4:
		# Message 
		# GET_FRAME WIDTH HEIGHT BITS BINSIZE
		if parts[0] == "GET_FRAME":			
			image = generate_image(int(parts[1]), int(parts[2]), int(parts[3]))
			time.sleep (0.1)
			print("Sending image of length {}".format(len(image)))
			socket.send(image)
