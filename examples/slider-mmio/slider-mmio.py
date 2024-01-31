#!/usr/bin/python3

import tkinter as tk
from threading import *
import coreio
import sys

# CoreIO peripheral:
#   000 W write to clear IRQ
#   004 R current slider value (0-100)

shutdown = Event()

def io_read(priv, addr, ba, flags):
    if((addr & 0xFFF) == 0x004 and len(ba) == 4):
        mutex.acquire()
        ba[0] = current_val[0]
        mutex.release()
        ba[1] = 0
        ba[2] = 0
        ba[3] = 0
    return 0

def io_write(priv, addr, ba : bytearray, flags):
    if((addr & 0xFFF) == 0x000 and len(ba) == 4):
        mutex.acquire()
        coreio.irq_update(rid=0, iid=0, state=0)
        mutex.release()
    return 0

def coreio_thread(arg):
    while (not shutdown.is_set() and coreio.mainloop(0) == 0):
        pass
    coreio.disconnect()
    print("[coreio] Disconnected")

def value_changed(event):
    mutex.acquire()
    current_val[0] = scale.get()
    coreio.irq_update(rid=0, iid=0, state=1)
    mutex.release()

if (len(sys.argv) < 2):
    print("Usage: %s IP:PORT" % (sys.argv[0]))
    sys.exit(-1)

if (coreio.connect(target=sys.argv[1]) != 0):
    sys.exit(-1)
coreio.register(rid=0, read=io_read, write=io_write, funcp=None)

mutex = Lock()
current_val = [ 0 ]

window = tk.Tk()
label = tk.Label(text="CoreIO app")
scale = tk.Scale(window, orient=tk.HORIZONTAL, length=200, from_=0.0, to=100.0, command=value_changed)
label.pack()
scale.pack()

def on_closing():
    shutdown.set();
    window.destroy()
    thread.join();

thread = Thread(target = coreio_thread, args = (10, ))
thread.start()

window.protocol("WM_DELETE_WINDOW", on_closing)
window.mainloop()
