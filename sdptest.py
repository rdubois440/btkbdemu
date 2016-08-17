#!/usr/bin/python

import os
import bluetooth
from bluetooth import *
import dbus
import time
import evdev
from evdev import *

class Bluetooth:
	P_CTRL = 17
	P_INTR = 19
	HOST = 0
	PORT = 1

	def __init__(self):
		os.system("hciconfig hci0 class 0x002540")
		os.system("hciconfig hci0 name TestGrosChene")
		os.system("hciconfig hci0 piscan")

		self.scontrol = BluetoothSocket(L2CAP)
		self.sinterrupt = BluetoothSocket(L2CAP)


		self.scontrol.bind(("", Bluetooth.P_CTRL))
		self.sinterrupt.bind(("", Bluetooth.P_INTR))

		self.bus=dbus.SystemBus()
		try: 
			self.manager = dbus.Interface(self.bus.get_object("org.bluez", "/"), "org.bluez.Manager")
			adapter_path = self.manager.DefaultAdapter()
			self.service = dbus.Interface(self.bus.get_object("org.bluez", adapter_path), "org.bluez.Service")
		except:
			sys.exit("Could not configure bluetooth. Is bluetooth started ?")

		try: 
			print "Opening path" + sys.path[0] + "/sdp_record.xml"
			print "Before open"
			fh = open(sys.path[0] + "/sdp_record.xml", "r")
			print "after open"
			self.service_record = fh.read()
			print "after read"
			fh.close()
			print "after close"

		except:
			sys.exit("Could not read the xml record. Exiting")


	def listen(self):

		print "Inside listen"
		self.service_handle = self.service.AddRecord(self.service_record)
		print "service record added"
		self.scontrol.listen(1)
		self.sinterrupt.listen(1)
		print "waiting for a connection"

		self.control, self.cinfo = self.scontrol.accept()
		print "Got a connection on the control channel " + self.cinfo[Bluetooth.HOST]

		self.cinterrupt, self.cinfo = self.sinterrupt.accept()
		print "Got a connection on the interrupt channel " + self.cinfo[Bluetooth.HOST]
		

	def send_input(self, ir):
		hex_str = ""
		
		for element in ir:
			if type(element) is list:
				bin_str = ""
				for bit in element:
					bin_str += str(bit)
				hex_str += chr(int(bin_str,2))
			else:
				hex_str += chr(element)

		self.cinterrupt.send(hex_str)


				
class Keyboard():
	def __init__(self):
		self.state = [
			0xa1,
			1,
			[0, 0, 0, 0, 0, 0, 0, 0],
			0,
			0, 0, 0, 0, 0, 0]
		have_dev = False
		while have_dev == False:
			try:
				self.dev = InputDevice("/dev/input/event0")
				have_dev = True

			except OSError:
				print "Keyboard not found, waiting 3 secs and retrying"
				time.sleep(3)
		print "Found a keyboard"

	def change_state(self, event):
		evdev_code = ecodes.KEY[event.code]
		modkey_element = keymap.modkey(evdev_code)
		if (modkey_element > 0):
			if self.state[2][modkey_element] == 0:
				self.state[2][modkey_element] = 1
			else:
				self.state[2][modkey_element] = 0
		else:
			hex_key = keymap.convert(ecodes.KEY[event.code])	
			for i in range(4, 10):
				if self.state[i] == hex_key and event.value == 0:
					self.state[i] = 0
				elif self.state[i] == 0 and event.value == 1:
					self.state[i] = hex_key
					break
				
	
	def event_loop(self, bt):
		for event in self.dev.read_loop():
			if event.type == ecodes.EV_KEY and event.value<2:
				self.change_state(event)
				bt.send_input(self.state)
					
if __name__ == "__main__":

	print "Starting 1"
	if not os.geteuid() == 0:
		sys.exit("Can only run as root")

	print "Starting 2"
	bt = Bluetooth()
	bt.listen()

	print "Listen sucessful"

	kb = Keyboard()
	kb.event_loop(bt)






	

