ZMQVirtualCam
=============

A DirectShow source that creates a virtual webcam that can be fed frames using a ZeroMQ connection.

Installation
============
Simply use the installer for automatic installation, or manual install can be done using regsvr32

> regsvr32 32\ZMQVirtualCam.dll
> regsvr32 64\ZMQVirtualCam.dll

Uninstall
=========
Simply use the installer for automatic removal. To manually remove the filter use

> regsvr32 /u 32\ZMQVirtualCam.dll
> regsvr32 /u 64\ZMQVirtualCam.dll

Usage
=====

A Python 3 example is provided in FrameServer. Modify this to send RGB formatted binary images when so requested.

You'll need zmq for Python which can be installed with

> pip install pyzmq

Note that you don't have to use Python, just any code that supports the ZeroMQ transport. Which is just about any 
language out there.

https://zeromq.org/languages

Building
========
Requirements:

* Visual Studio 2017
* CMake 3
* git
* inno setup ( optional )

Simply run build.bat, this will download and build the libzmq dependency and then build the 32 and 64 bit version of the filter.

References
==========
* VirtualCam. Example code from http://tmhare.mvps.org/downloads.htm

* BaseClasses. https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/Win7Samples/multimedia/directshow/baseclasses (MIT License)

* ZeroMQ. https://zeromq.org (GNU LESSER GENERAL PUBLIC LICENSE v3)


Copyright
=========
Copyright GNU GPL v3 (c) 2020 Ron Bessems
