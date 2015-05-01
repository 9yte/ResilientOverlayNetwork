//                   In the name of GOD
/**
 * Partov is a simulation engine, supporting emulation as well,
 * making it possible to create virtual networks.
 *  
 * Copyright © 2009-2014 Behnam Momeni.
 * 
 * This file is part of the Partov.
 * 
 * Partov is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Partov is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Partov.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "cf.h"

#include "sm.h"
#include "frame.h"

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

uint64_t ClientFramework::sentBytes = 0;
uint64_t ClientFramework::receivedBytes = 0;

pthread_mutex_t ClientFramework::mutex;
pthread_cond_t ClientFramework::initializationCompletedCondition;
bool ClientFramework::initialized;

ClientFramework *ClientFramework::me = NULL;

/* Singleton Class */
ClientFramework::ClientFramework (int argc, char *argv[]) {
  connected = false;
  machine = NULL;
  progName = argv[0];
  parseArguments (argc, argv);

  initialized = false;
  pthread_mutex_init (&mutex, NULL);
  pthread_cond_init (&initializationCompletedCondition, NULL);
}

ClientFramework::~ClientFramework () {
  if (connected) {
    close (sfd);
  }
  if (machine) {
    delete machine;
  }
  pthread_mutex_destroy (&mutex);
  pthread_cond_destroy (&initializationCompletedCondition);
}

void ClientFramework::parseArguments (int argc, char *argv[]) {
  const int MAX_COUNT_OF_ARGS = 8;
  const int COUNT_OF_OPTIONAL_ARGS = 2;
  bool mark[MAX_COUNT_OF_ARGS];
  memset (mark, 0, sizeof(mark));
  for (int i = 1; i < argc; ++i) {
    if (strcmp (argv[i], "--ip") == 0) {
      checkForRedundantArgument (mark[0]);
      parseIPArgument (argv[++i]);

    } else if (strcmp (argv[i], "--port") == 0) {
      checkForRedundantArgument (mark[1]);
      parsePortArgument (argv[++i]);

    } else if (strcmp (argv[i], "--map") == 0) {
      checkForRedundantArgument (mark[2]);
      parseMapNameArgument (argv[++i]);

    } else if (strcmp (argv[i], "--user") == 0) {
      checkForRedundantArgument (mark[3]);
      parseUserNameArgument (argv[++i]);

    } else if (strcmp (argv[i], "--pass") == 0) {
      checkForRedundantArgument (mark[4]);
      parsePasswordArgument (argv[++i]);

    } else if (strcmp (argv[i], "--id") == 0) {
      checkForRedundantArgument (mark[5]);
      parseIDArgument (argv[++i]);
      needNewMap = 0;

    } else if (strcmp (argv[i], "--new") == 0) {
      checkForRedundantArgument (mark[5]);
      needNewMap = 1;

    } else if (strcmp (argv[i], "--free") == 0) {
      checkForRedundantArgument (mark[5]);
      needNewMap = -1;

    } else if (strcmp (argv[i], "--node") == 0) {
      checkForRedundantArgument (mark[6]);
      parseNodeNameArgument (argv[++i]);

    } else if (strcmp (argv[i], "--args") == 0) {
      checkForRedundantArgument (mark[7]);
      int userProgramArgc = argc - ++i;
      parseUserProgramArguments (userProgramArgc, &(argv[i]));
      break;

    } else {
      usage ();
    }
  }
  const int COUNT_OF_MANDATORY_ARGS = MAX_COUNT_OF_ARGS - COUNT_OF_OPTIONAL_ARGS;
  for (int i = 0; i < COUNT_OF_MANDATORY_ARGS; ++i) {
    if (!mark[i]) {
      usage ();
    }
  }
  memset (&server, 0, sizeof(sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_port = htons (port);
  server.sin_addr.s_addr = htonl (ip);
}

inline void ClientFramework::parseIPArgument (char *arg) {
  std::string ipString (arg);
  if (!parseIP (ipString, ip)) {
    usage ();
  }
}

inline void ClientFramework::parsePortArgument (char *arg) {
  if (!parsePort (arg, port)) {
    usage ();
  }
}

inline void ClientFramework::parseMapNameArgument (char *arg) {
  mapName = arg;
}

inline void ClientFramework::parseUserNameArgument (char *arg) {
  userName = arg;
}

inline void ClientFramework::parseIDArgument (char *arg) {
  creatorId = arg;
}

inline void ClientFramework::parsePasswordArgument (char *arg) {
  password = arg;
}

inline void ClientFramework::parseNodeNameArgument (char *arg) {
  nodeName = arg;
}

inline void ClientFramework::checkForRedundantArgument (bool &mark) const {
  if (mark) {
    usage ();
  }
  mark = true;
}

inline void ClientFramework::parseUserProgramArguments (int argc, char *argv[]) const {
  SimulatedMachine::parseArguments (argc, argv);
}

bool ClientFramework::connectToServer () {
  uint32 ip = ntohl (server.sin_addr.s_addr);
  std::cout << "Connecting to Partov server at " << (ip >> 24) << "."
      << ((ip >> 16) & 0xFF) << "." << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF) << ":"
      << ntohs (server.sin_port) << std::endl;
  std::cout << "           to map/node \"" << mapName << "\"/\"" << nodeName << "\" ... "
      << std::flush;
  sfd = socket (PF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    std::cout << "[Failed]" << std::endl;
    return false;
  }
  connected = true;
  if (connect (sfd, reinterpret_cast < sockaddr * > (&server), sizeof(sockaddr_in))) {
    std::cout << "[Failed]" << std::endl;
    return false;
  }
  std::cout << " [Done]" << std::endl;
  return true;
}

bool ClientFramework::doInitialNegotiations () {
  return doSigningInNegotiations (buffer) && doMapSelectingNegotiations (buffer)
      && doNodeSelectingNegotiations (buffer)
      && doInformationSynchronizationNegotiations (buffer);
}

bool ClientFramework::doInitialRecoveryNegotiations () {
  return doSigningInNegotiations (buffer) && doMapSelectingNegotiations (buffer)
      && doNodeSelectingNegotiations (buffer);
}

bool ClientFramework::doSigningInNegotiations (byte *buf) {
  std::cout << "Signing in... " << std::flush;

  if (userName.length () > 20 || password.length () > 50) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Username/Password is too long!!" << std::endl;
    return false;
  }
  uint16 *ts = reinterpret_cast < uint16 * > (buf);
  buf += sizeof(uint16);

  uint16 size = userName.length () + 1;
  (*reinterpret_cast < uint16 * > (buf)) = htons (size);
  buf += sizeof(uint16);
  strcpy (reinterpret_cast < char * > (buf), userName.data ());
  buf += size;

  size = password.length () + 1;
  (*reinterpret_cast < uint16 * > (buf)) = htons (size);
  buf += sizeof(uint16);
  strcpy (reinterpret_cast < char * > (buf), password.data ());
  buf += size;

  size = buf - buffer;
  (*ts) = htons (size - sizeof(uint16));

  if (!sendOrReceive (true, 40, size)) {
    return false;
  }
  if (!sendOrReceive (false, 41, 2 * sizeof(uint32))) {
    return false;
  }
  buf = buffer;
  int c1 = ntohl (*reinterpret_cast < uint32 * > (buf));
  buf += sizeof(uint32);
  int c2 = ntohl (*reinterpret_cast < uint32 * > (buf));
  buf += sizeof(uint32);
  if (c1 != SigningInNegotiationType || c2 != 1) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Username or password is incorrect. [error code 42]" << std::endl;
    return false;
  } else {
    std::cout << "[Done]" << std::endl;
  }
  return true;
}

bool ClientFramework::doMapSelectingNegotiations (byte *buf) {
  std::cout << "Selecting map... " << std::flush;

  if (needNewMap != 0) {
    creatorId = userName;
  }
  if (3 * sizeof(uint16) + mapName.length () + creatorId.length () + sizeof(int32)
      > FRAME_BUFFER_SIZE) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Map name is too long." << std::endl;
    return false;
  }

  uint16 *ts = reinterpret_cast < uint16 * > (buf);
  buf += sizeof(uint16);

  uint16 size = mapName.length () + 1;
  (*reinterpret_cast < uint16 * > (buf)) = htons (size);
  buf += sizeof(uint16);
  strcpy (reinterpret_cast < char * > (buf), mapName.data ());
  buf += size;

  size = creatorId.length () + 1;
  (*reinterpret_cast < uint16 * > (buf)) = htons (size);
  buf += sizeof(uint16);
  strcpy (reinterpret_cast < char * > (buf), creatorId.data ());
  buf += size;

  (*reinterpret_cast < int32 * > (buf)) = htonl (needNewMap);
  buf += sizeof(int32);

  size = buf - buffer;
  (*ts) = htons (size - sizeof(uint16));

  if (!sendOrReceive (true, 2, size)) {
    return false;
  }
  // processing server response...
  do {
    if (!sendOrReceive (false, 3, 2 * sizeof(uint32))) {
      return false;
    }
    buf = buffer;
    int c1 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int c2 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    if (c1 != MapNodeSelectingNegotiationType) {
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 4]" << std::endl;
      return false;
    }
    switch (c2) {
    case MapNotExistCommand:
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed (map not exists). [error code 5]"
          << std::endl;
      return false;

    case MaxMapIpuViolatedCommand:
      std::cout << "[Failed]" << std::endl;
      std::cout
          << "+++ Initial negotiations failed (maximum allowable map instances are already allocated). [error code 6]"
          << std::endl;
      return false;

    case MapSelectedCommand:
      std::cout << "[Done]" << std::endl;
      break;

    case MapInstanceResourcesAreReleased:
      std::cout << "[Done]" << std::endl;
      std::cout << "+++ Whole of the resources of the map instance are released."
          << std::endl;
      return false;

    case OutOfResourceCommand:
      std::cout << "[Failed]" << std::endl;
      std::cout
          << "+++ Initial negotiations failed (out of resource; retry later). [error code 7]"
          << std::endl;
      return false;

    default:
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 8]" << std::endl;
      return false;
    }

  } while (0);

  return true;
}

bool ClientFramework::doNodeSelectingNegotiations (byte *buf) {
  if (nodeName == "") {
    // there is no node name specified.
    std::cout << "No node name specified. So we could exit now." << std::endl;
    std::cout
        << "The map will remain on the server. Do not forget to free it after simulation."
        << std::endl;
    destroy ();
    exit (0);
  }
  std::cout << "Connecting to node... " << std::flush;

  unsigned int ms = nodeName.length () + 1;
  (*reinterpret_cast < uint16 * > (buf)) = htons (ms);
  buf += sizeof(uint16);

  if (ms > FRAME_BUFFER_SIZE - sizeof(uint16)) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Node name is too long." << std::endl;
    return false;
  }
  strcpy (reinterpret_cast < char * > (buf), nodeName.data ());
  buf += ms;

  if (!sendOrReceive (true, 9, buf - buffer)) {
    return false;
  }
  // processing server response...
  do {
    if (!sendOrReceive (false, 10, 2 * sizeof(uint32))) {
      return false;
    }
    buf = buffer;
    int c1 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int c2 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    if (c1 != MapNodeSelectingNegotiationType) {
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 11]" << std::endl;
      return false;
    }
    switch (c2) {
    case NodeNotExistCommand:
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed (node not exists). [error code 12]"
          << std::endl;
      return false;

    case NodeSelectedCommand:
      std::cout << "[Done]" << std::endl;
      break;

    default:
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 13]" << std::endl;
      return false;
    }

  } while (0);

  return true;
}

bool ClientFramework::doInformationSynchronizationNegotiations (byte *buf) {
  std::cout << "Synchronizing information... " << std::flush;

  if (!doInterfacesInformationSynchronizationNegotiations (buf)) {
    return false;
  }
  if (!doCustomInformationSynchronizationNegotiations (buf)) {
    return false;
  }
  std::cout << "[Done]" << std::endl;

  machine->printInterfacesInformation ();
  const std::string &ci = machine->getCustomInformation ();
  if (ci == "") {
    std::cout << "There is no custom information for this machine." << std::endl;
  } else {
    std::cout << "Custom information for this machine are:" << std::endl;
    std::cout << ci << std::endl;
  }
  std::cout << "==========================================" << std::endl;

  return true;
}

bool ClientFramework::doInterfacesInformationSynchronizationNegotiations (byte *buf) {
  (*reinterpret_cast < uint32 * > (buf)) = htonl (InterfacesInformationRequestCommand);
  if (!sendOrReceive (true, 14, sizeof(uint32))) {
    return false;
  }
  // synchronizing interfaces information...
  do {
    if (!sendOrReceive (false, 15, 3 * sizeof(uint32))) {
      return false;
    }
    int c1 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int c2 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int count = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);

    if (c1 != InformationSynchronizationType
        || c2 != InterfacesInformationSynchronizationSubType || count < 1) {
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 16]" << std::endl;
      return false;
    }
    machine = new SimulatedMachine (this, count);
    for (int i = 0; i < count; ++i) {
      if (!readInterfaceInformation (buffer, i)) {
        return false;
      }
    }

  } while (0);

  return true;
}

bool ClientFramework::readInterfaceInformation (byte *buf, int index) {
  if (!sendOrReceive (false, 17, sizeof(uint16))) {
    return false;
  }
  uint32 size = ntohs (*reinterpret_cast < uint16 * > (buf));
  if (size > FRAME_BUFFER_SIZE) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Initial negotiations failed. [error code 18]" << std::endl;
    return false;
  }
  if (!sendOrReceive (false, 19, size)) {
    return false;
  }
  const char *ifName = reinterpret_cast < char * > (buf);
  if (strcmp (ifName, "edu::sharif::partov::nse::map::interface::EthernetInterface") != 0
      && strcmp (ifName,
          "edu::sharif::partov::nse::map::interface::EthernetPhysicalInterface") != 0) {
    std::cout << "[Failed]" << std::endl;
    std::cout
        << "+++ Initial negotiations failed (unknown interface type). [error code 19]"
        << std::endl;
    return false;
  }
  buf += size - sizeof(uint64) - 2 * sizeof(uint32);

  uint64 mac = (*reinterpret_cast < uint64 * > (buf));
  buf += sizeof(uint64);

  uint32 ip = ntohl (*reinterpret_cast < uint32 * > (buf));
  buf += sizeof(uint32);

  uint32 mask = ntohl (*reinterpret_cast < uint32 * > (buf));
  buf += sizeof(uint32);

  machine->initInterface (index, mac, ip, mask);

  return true;
}

bool ClientFramework::doCustomInformationSynchronizationNegotiations (byte *buf) {
  (*reinterpret_cast < uint32 * > (buf)) = htonl (CustomInformationRequestCommand);
  if (!sendOrReceive (true, 20, sizeof(uint32))) {
    return false;
  }
  // synchronizing custom information...
  do {
    if (!sendOrReceive (false, 21, 2 * sizeof(uint32) + sizeof(uint16))) {
      return false;
    }
    int c1 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int c2 = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    int size = ntohs (*reinterpret_cast < uint16 * > (buf));
    buf += sizeof(uint16);

    if (c1 != InformationSynchronizationType
        || c2 != CustomInformationSynchronizationSubType || size < 1) {
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code 22]" << std::endl;
      return false;
    }
    if (!sendOrReceive (false, 23, size)) {
      return false;
    }
    std::string ci = reinterpret_cast < char * > (buffer);
    machine->setCustomInformation (ci);

  } while (0);

  return true;
}

bool ClientFramework::startSimulation () {
  std::cout << "Starting simulation... " << std::flush;
  (*reinterpret_cast < uint32 * > (buffer)) = htonl (StartingSimulationRequestCommand);
  if (!sendOrReceive (true, 24, sizeof(uint32))) {
    return false;
  }
  if (!sendOrReceive (false, 25, sizeof(uint32))) {
    return false;
  }
  int c = ntohl (*reinterpret_cast < uint32 * > (buffer));
  if (c != SimulationStartedNotificationType) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Starting simulation does not verified by server." << std::endl;
    return false;
  }

  struct sigaction sa;
  memset (&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = &ClientFramework::systemInterrupt;

  if (sigaction (SIGINT, &sa, NULL) == -1) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "Failed to handle the interrupt signal." << std::endl;
    std::cout << "Make sure that you have POSIX library installed." << std::endl;
    return false;
  }
  std::cout << "[Done]" << std::endl;
  std::cout
      << "Simulation started. You will receive all frames of the machine from now on."
      << std::endl;
  std::cout << "==========================================" << std::endl;

  simulateMachine ();
  systemInterrupt (SIGINT);

  return true;
}

void ClientFramework::systemInterrupt (int signalNumber) {
  if (signalNumber != SIGINT) {
    return;
  }
  std::cout << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "Sent: " << sentBytes << " bytes(s)" << std::endl;
  std::cout << "Received: " << receivedBytes << " bytes(s)" << std::endl;

  destroy ();
  exit (0);
}

void ClientFramework::simulateMachine () {
  // simulating machine...
  pthread_t stid; /* simulator thread id */
  if (pthread_create (&stid, NULL, &ClientFramework::st_run, machine)) {
    /* Can not create the thread. */
    std::cout << "Can not start the simulator thread. Check POSIX installation."
        << std::endl;
    return;
  }
  pthread_mutex_lock (&mutex);
  while (!initialized) {
    pthread_cond_wait (&initializationCompletedCondition, &mutex);
  }
  pthread_mutex_unlock (&mutex);

  simulationEventLoop ();
  recoverSimulation ();
}

void ClientFramework::simulationEventLoop () {
  forever {
    if (!sendOrReceive (false, -1, sizeof(uint32) + sizeof(uint16))) {
      std::cout << "An IO exception occurred (error code 26). Aborting simulation..."
          << std::endl;
      break;
    }
    byte *buf = buffer;
    int c = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    unsigned int size = ntohs (*reinterpret_cast < uint16 * > (buf));
    buf += sizeof(uint16);

    if (c == InvalidInterfaceIndexType) {
      if (size < 1 || !sendOrReceive (false, -1, size)) {
        std::cout << "An IO exception occurred (error code 27). Aborting simulation..."
            << std::endl;
        break;
      }
      const char *desc = reinterpret_cast < char * > (buffer);
      std::cout << "Warning: Interface index was wrong." << std::endl;
      std::cout << "-- " << desc << std::endl;
      continue;
    }
    if (c != RawFrameReceivedNotificationType || size < sizeof(uint32)
        || size > FRAME_BUFFER_SIZE) {
      std::cout << "An IO exception occurred (error code 28). Aborting simulation..."
          << std::endl;
      break;
    }
    if (!sendOrReceive (false, -1, size)) {
      std::cout << "An IO exception occurred (error code 29). Aborting simulation..."
          << std::endl;
      break;
    }
    buf = buffer;

    int interfaceIndex = ntohl (*reinterpret_cast < uint32 * > (buf));
    buf += sizeof(uint32);
    if (interfaceIndex < 0 || interfaceIndex >= machine->getCountOfInterfaces ()) {
      std::cout << "An IO exception occurred (error code 30). Aborting simulation..."
          << std::endl;
      break;
    }
    size -= sizeof(uint32);
    // The frame is placed at "buf" with length "size"
    receivedBytes += size;
    machine->processFrame (Frame (size, buf), interfaceIndex);
  }
}

void *ClientFramework::st_run (void *arg) {
  Machine *machine = reinterpret_cast < Machine * > (arg);

  machine->initialize ();

  pthread_mutex_lock (&mutex);
  initialized = true;
  pthread_mutex_unlock (&mutex);

  pthread_cond_signal (&initializationCompletedCondition);

  machine->run ();

  return NULL;
}

bool ClientFramework::sendFrame (Frame frame, int ifaceIndex) const {
  pthread_mutex_lock (&mutex);
  bool res = realSendFrame (frame, ifaceIndex);
  pthread_mutex_unlock (&mutex);

  return res;
}

bool ClientFramework::notifyChangeOfIPAddress (uint32 newIP, int ifaceIndex) const {
  pthread_mutex_lock (&mutex);
  bool res = realNotifyChangeOfIPAddress (newIP, ifaceIndex);
  pthread_mutex_unlock (&mutex);

  return res;
}

bool ClientFramework::notifyChangeOfNetmask (uint32 newNetmask, int ifaceIndex) const {
  pthread_mutex_lock (&mutex);
  bool res = realNotifyChangeOfNetmask (newNetmask, ifaceIndex);
  pthread_mutex_unlock (&mutex);

  return res;
}

bool ClientFramework::realSendFrame (Frame frame, int ifaceIndex) const {
  byte mybuf[2 * sizeof(uint32) + sizeof(uint16)];

if  (frame.length < 14 || frame.length >= ((1 << 16) - sizeof(uint32))) {
    std::cout << "Error in sending frame on simulated machine. [error code 31]"
    << std::endl;
    return false;
  }

  byte *buf = mybuf;
  (*reinterpret_cast<uint32 *> (buf)) = htonl (SendFrameCommand);
  buf += sizeof(uint32);

  (*reinterpret_cast<uint16 *> (buf)) = htons (sizeof(uint32) + frame.length);
  buf += sizeof(uint16);

  (*reinterpret_cast<uint32 *> (buf)) = htonl (ifaceIndex);
  buf += sizeof(uint32);

  if (!sendOrReceive (true, -1, sizeof(mybuf), mybuf)) {
    std::cout << "Error in sending frame on simulated machine. [error code 32]"
    << std::endl;
    return false;
  }
  if (!sendOrReceive (true, -1, frame.length, frame.data)) {
    std::cout << "Error in sending frame on simulated machine. [error code 33]"
    << std::endl;
    return false;
  }
  sentBytes += frame.length;
  return true;
}

bool ClientFramework::realNotifyChangeOfIPAddress (uint32 newIP, int ifaceIndex) const {
  byte mybuf[3 * sizeof(uint32)];

byte  *buf = mybuf;
  (*reinterpret_cast<uint32 *> (buf)) = htonl (ChangeIPAddressCommand);
  buf += sizeof(uint32);

  (*reinterpret_cast<uint32 *> (buf)) = htonl (ifaceIndex);
  buf += sizeof(uint32);

  (*reinterpret_cast<uint32 *> (buf)) = htonl (newIP);
  buf += sizeof(uint32);

  if (!sendOrReceive (true, -1, sizeof(mybuf), mybuf)) {
    std::cout << "Error while notifying change of IP address (iface index: "
    << ifaceIndex << "). [error code 43]" << std::endl;
    return false;
  }
  return true;
}

bool ClientFramework::realNotifyChangeOfNetmask (uint32 newNetmask,
    int ifaceIndex) const {
  byte mybuf[3 * sizeof(uint32)];

byte  *buf = mybuf;
  (*reinterpret_cast<uint32 *> (buf)) = htonl (ChangeNetmaskCommand);
  buf += sizeof(uint32);

  (*reinterpret_cast<uint32 *> (buf)) = htonl (ifaceIndex);
  buf += sizeof(uint32);

  (*reinterpret_cast<uint32 *> (buf)) = htonl (newNetmask);
  buf += sizeof(uint32);

  if (!sendOrReceive (true, -1, sizeof(mybuf), mybuf)) {
    std::cout << "Error while notifying change of network mask (iface index: "
    << ifaceIndex << "). [error code 44]" << std::endl;
    return false;
  }
  return true;
}

bool ClientFramework::sendOrReceive (bool sendIt, int errorCode, int size) {
  return sendOrReceive (sendIt, errorCode, size, buffer);
}

bool ClientFramework::sendOrReceive (bool sendIt, int errorCode, int size,
    byte *buf) const {
  int count = 0;
  do {
    size -= count;
    buf += count;
    if (sendIt) {
      count = send (sfd, buf, size, 0);
    } else {
      count = recv (sfd, buf, size, 0);
    }
    if (count < 0) {
      if (errorCode == -1) {
        return false;
      }
      std::cout << "[Failed]" << std::endl;
      std::cout << "+++ Initial negotiations failed. [error code " << errorCode << "]"
          << std::endl;
      return false;
    }
  } while (count < size);
  return true;
}

void ClientFramework::recoverSimulation () {
  forever {
    std::cout << "Trying to recover the simulation..............." << std::endl;
    pthread_mutex_lock (&mutex);

    close (sfd);
    connected = false;
    needNewMap = 0;
    if (connectToServer () && doInitialRecoveryNegotiations ()
        && announceSimulationResume ()) {
      pthread_mutex_unlock (&mutex);
      simulationEventLoop ();
    } else {
      pthread_mutex_unlock (&mutex);
      break;
    }
  }
}

bool ClientFramework::announceSimulationResume () {
  std::cout << "Resuming simulation......... " << std::flush;
  (*reinterpret_cast < uint32 * > (buffer)) = htonl (StartingSimulationRequestCommand);
  if (!sendOrReceive (true, 24, sizeof(uint32))) {
    return false;
  }
  if (!sendOrReceive (false, 25, sizeof(uint32))) {
    return false;
  }
  int c = ntohl (*reinterpret_cast < uint32 * > (buffer));
  if (c != SimulationStartedNotificationType) {
    std::cout << "[Failed]" << std::endl;
    std::cout << "+++ Resuming simulation does not verified by server." << std::endl;
    return false;
  }
  std::cout << "[Done]" << std::endl;
  std::cout << "==========================================" << std::endl;

  return true;
}

bool ClientFramework::parseIP (std::string &ip, uint32 &res) const {
  typedef std::string::size_type st_uint;
  ip.push_back ('.');
  st_uint dotPos = 0;
  union {
    byte arr[4];
    uint32 ip;
  } val;
  for (int i = 0; i < 4; ++i) {
    st_uint ndp = ip.find ('.', dotPos);
    if (ndp == std::string::npos) {
      return false;
    }
    unsigned int num = 0;
    if (ndp - dotPos > 3) {
      return false;
    }
    while (dotPos < ndp) {
      int d = ip[dotPos++] - '0';
      if (d < 0 || d > 9) {
        return false;
      }
      num = num * 10 + d;
    }
    if (num > 255) {
      return false;
    }
    val.arr[i] = static_cast < byte > (num);
    ++dotPos;
  }
  res = ntohl (val.ip);
  if (dotPos == ip.length ()) {
    return true;
  }
  return false;
}

bool ClientFramework::parsePort (const char *port, uint16 &res) const {
  int num = 0;
  for (int i = 0; port[i]; ++i) {
    int digit = port[i] - '0';
    if (digit < 0 || digit > 9) {
      return false;
    }
    num = num * 10 + digit;
  }
  if (num >= (1 << 16)) {
    return false;
  }
  res = static_cast < uint16 > (num);

  return true;
}

void ClientFramework::printArguments () const {
  std::cout << " ____   __   ____  ____  __   _  _     ___  ____ " << std::endl;
  std::cout << "(  _ \\ / _\\ (  _ \\(_  _)/  \\ / )( \\   / __)(  __)" << std::endl;
  std::cout << " ) __//    \\ )   /  )( (  O )\\ \\/ /  ( (__  ) _) " << std::endl;
  std::cout << "(__)  \\_/\\_/(__\\_) (__) \\__/  \\__/    \\___)(__)  " << std::endl;
  std::cout << "                                                 " << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "Server ip: " << (ip >> 24) << "." << ((ip >> 16) & 0xFF) << "."
      << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF) << std::endl;
  std::cout << "Server port: " << port << std::endl;
  std::cout << "Map name: " << mapName << std::endl;
  if (nodeName == "") {
    std::cout << "Node name not specified." << std::endl;
  } else {
    std::cout << "Node name: " << nodeName << std::endl;
  }
  std::cout << "==========================================" << std::endl;
}

void ClientFramework::usage () const {
  std::cout << " ____                 __                       ____     ____    "                             << std::endl;
  std::cout << "/\\  _`\\              /\\ \\__                   /\\  _`\\  /\\  _`\\  "                     << std::endl;
  std::cout << "\\ \\ \\L\\ \\ __     _ __\\ \\ ,_\\   ___   __  __   \\ \\ \\/\\_\\\\ \\ \\L\\_\\"           << std::endl;
  std::cout << " \\ \\ ,__/'__`\\  /\\`'__\\ \\ \\/  / __`\\/\\ \\/\\ \\   \\ \\ \\/_/_\\ \\  _\\/"           << std::endl;
  std::cout << "  \\ \\ \\/\\ \\L\\.\\_\\ \\ \\/ \\ \\ \\_/\\ \\L\\ \\ \\ \\_/ |   \\ \\ \\L\\ \\\\ \\ \\/ "  << std::endl;
  std::cout << "   \\ \\_\\ \\__/.\\_\\\\ \\_\\  \\ \\__\\ \\____/\\ \\___/     \\ \\____/ \\ \\_\\ "         << std::endl;
  std::cout << "    \\/_/\\/__/\\/_/ \\/_/   \\/__/\\/___/  \\/__/       \\/___/   \\/_/ "                    << std::endl;
  std::cout << "                                                                "                             << std::endl;
  std::cout << "Partov Project - Version 3.1 -- Client Framework"                  << std::endl;
  std::cout << "Code-name: PARTOV (Portable And Reliable Tool fOr Virtualization)" << std::endl;
  std::cout << "Copyright © 2009-2014 Behnam Momeni."                              << std::endl;
  std::cout << std::endl;
  std::cout << "Usage:"                         << std::endl;
  std::cout << "  " << progName << " <options>" << std::endl;
  std::cout << "  <options>:"                   << std::endl;
  std::cout << "       --ip <server-ipv4>      The ip (version 4) of the Partov server (like 192.168.0.1)"              << std::endl;
  std::cout << "       --port <server-port>    The port of the Partov server (like 9339)"                               << std::endl;
  std::cout << "       --map <map-name>        The name of the map/topology which you want to connect to (like router)" << std::endl;
  std::cout << "       --user <user-name>      Your username; will be used for authentication."                         << std::endl;
  std::cout << "       --pass <password>       Your password; will be used for authentication."                         << std::endl;
  std::cout << "       --node <node-name>      (optional) The name of the node which you want to simulate"              << std::endl;
  std::cout << "                                    it within <map-name> map (if any)"                                  << std::endl;
  std::cout << "       --id <creator-username> (not used in conjunction with --new or --free options)"                  << std::endl;
  std::cout << "                                    Try to connect to the map instance which was created"               << std::endl;
  std::cout << "                                    by <creator-username> user previously."                             << std::endl;
  std::cout << "       --new                   (not used in conjunction with --id or --free options)"                   << std::endl;
  std::cout << "                                    Try to create a new map instance."                                  << std::endl;
  std::cout << "                                    Any user can only create (at maximum) ipu instance(s) of each map." << std::endl;
  std::cout << "       --free                  (not used in conjunction with --id or --new options)"                    << std::endl;
  std::cout << "                                    Free resources of the map instance which was created"               << std::endl;
  std::cout << "                                    by this user. Any user can only remove his/her owned instances."    << std::endl;
  std::cout << "       --args <arg>[ <arg>...] (optional) The arguments which come after --args will be passed to"      << std::endl;
  std::cout << "                                    the SimulatedMachine::parseArguments method. The --args must be"    << std::endl;
  std::cout << "                                    latest argument which is not intended to be passed to user program."<< std::endl;
  std::cout << "For any bugs, report to <b_momeni@ce.sharif.edu>"                               << std::endl;
  std::cout << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>." << std::endl;
  std::cout << "This is free software: you are free to change and redistribute it."             << std::endl;
  std::cout << "There is NO WARRANTY, to the extent permitted by law."                          << std::endl;
  exit (-1);
}

void ClientFramework::init (int argc, char *argv[]) {
  if (me) {
    return;
  }
  me = new ClientFramework (argc, argv);
}

ClientFramework *ClientFramework::getInstance () {
  return me;
}

void ClientFramework::destroy () {
  delete me;
}

