# CF

The _Client Framework (CF)_ speaks with _PartovNSE_ (the server) using _SCFCP_ (a protocol over TCP) to instantiate/release virtual topologies and to remotely expand virtual nodes' behavior, running on the server, by dictating internal logic of _SimulatedNode_ plugin instances.
Latest stable version of CF is **3.1.0**.

This repo contains two main folders:

  - The **base** folder containing main codes, including SCFCP implementation. It connects to Partov central server, instantiates _SimulatedMachine_ class, initializes it accordingly and passes execution control to it as follows:
    + Before instantiating _SimulatedMachine_ class, the `SimulatedMachine::parseArguments ()` static method is called to pass arguments which were passed to CF through _--args_ option,
    + Then _SimulatedMachine_ will be instantiated and after loading all interfaces of the corresponding virtual node, the `SimulatedMachine::initialize ()` method is called,
    + After complete initialization the `SimulatedMachine::run ()` method is called in its own thread of execution (a helper thread) which will be terminated upon return (this method can be used for sending packets independent of incoming packets),
    + Concurrently, in the main thread of execution, the `SimulatedMachine::processFrame ()` method is called whenever a new frame is received by the corresponding virtual node on the server (frame is passed in a shared buffer and is valid just while processFrame is running),
    + All threads can call the synchronized `Machine::sendFrame ()` method to send/inject created frames over interfaces of corresponding virtual node,
  - The **user** folder containing implementation of _SimulatedMachine_ which overrides behavior of _SimulatedNode_ plugin instance on the server-side. For normal usages, your code should be written totally in this folder (remember to update the Makefile when you add new files).

## Installation

Just run **make** to compile the code. The CF does not have any special dependency and should be compilable on most Linux systems.

## Usage

CF needs to authenticate itself to central server using username/password. Write down your credentials in the _info.sh_ file and run

    chmod 600 ./info.sh

to prevent any access by other users. Then you can use three other scripts as follows:

  - The **new.sh** for instantiating a new topology,
  - The **free.sh** for releasing the previously assigned topology instance,
    * Do not forget to free the instance when your simulation finished (required for log files flushing).
  - The **run.sh** for connecting your program to configured virtual node (if any).

## Resources

You can read **CFManual.pdf** for a more complete guide to CF.

## License
    Copyright © 2014  Behnam Momeni

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see {http://www.gnu.org/licenses/}.
