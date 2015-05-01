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

#ifndef CLIENT_FRAMEWORK_H_
#define CLIENT_FRAMEWORK_H_

#include "partovdef.h"

#include <netinet/in.h>

class Machine;
class Frame;

/* Singleton */class ClientFramework {
public:
  static const uint32 FRAME_BUFFER_SIZE = 1524;

protected:
  enum StubCommunicationProtocolType {
    SigningInNegotiationType = 0,
    MapNodeSelectingNegotiationType = 1,
    InformationSynchronizationType = 2,
    SimulationStartedNotificationType = 3,
    RawFrameReceivedNotificationType = 4,
    InvalidInterfaceIndexType = 5
  };

  enum InformationSynchronizationSubTypes {
    InterfacesInformationSynchronizationSubType = 1,
    CustomInformationSynchronizationSubType = 2
  };

  enum MapNodeSelectingNegotiationCommands {
    MapNotExistCommand = 0,
    MaxMapIpuViolatedCommand = 1,
    MapSelectedCommand = 2,
    NodeNotExistCommand = 3,
    NodeSelectedCommand = 4,
    OutOfResourceCommand = 5,
    MapInstanceResourcesAreReleased = 6
  };

  enum InformationRequestingCommand {
    InterfacesInformationRequestCommand = 0,
    CustomInformationRequestCommand = 1,
    StartingSimulationRequestCommand = 2
  };

  enum SimulationCommand {
    DisconnectCommand = 0,
    SendFrameCommand = 1,
    ChangeIPAddressCommand = 2,
    ChangeNetmaskCommand = 3
  };

private:
  bool connected;
  const char *progName;
  uint32 ip;
  uint16 port;

  struct sockaddr_in server;
  std::string mapName;
  std::string nodeName;

  std::string userName;
  std::string password;

  std::string creatorId;
  int needNewMap;

  Machine *machine;

  static uint64_t sentBytes;
  static uint64_t receivedBytes;

  static ClientFramework *me;

  static pthread_mutex_t mutex;
  static pthread_cond_t initializationCompletedCondition;
  static bool initialized;

protected:
  int sfd;
  byte buffer[FRAME_BUFFER_SIZE];

  bool sendOrReceive (bool sendIt, int errorCode, int size);
  bool sendOrReceive (bool sendIt, int errorCode, int size, byte *buf) const;

private:
  ClientFramework (int argc, char *argv[]);
  ~ClientFramework ();

public:
  static void init (int argc, char *argv[]);
  static ClientFramework *getInstance ();
  static void destroy ();

  static void systemInterrupt (int signalNumber);

  bool connectToServer ();
  bool doInitialNegotiations ();
  bool startSimulation ();

  void simulateMachine ();

  void printArguments () const;

  bool /* Synchronized */sendFrame (Frame frame, int ifaceIndex) const;

  bool /* Synchronized */notifyChangeOfIPAddress (uint32 newIP, int ifaceIndex) const;
  bool /* Synchronized */notifyChangeOfNetmask (uint32 newNetmask,
      int ifaceIndex) const;

protected:
  bool realSendFrame (Frame frame, int ifaceIndex) const;

  bool realNotifyChangeOfIPAddress (uint32 newIP, int ifaceIndex) const;
  bool realNotifyChangeOfNetmask (uint32 newNetmask, int ifaceIndex) const;

  void simulationEventLoop ();
  bool doInitialRecoveryNegotiations ();
  void recoverSimulation ();
  bool announceSimulationResume ();

private:
  void parseArguments (int argc, char *argv[]);
  inline void parseIPArgument (char *arg);
  inline void parsePortArgument (char *arg);
  inline void parseMapNameArgument (char *arg);
  inline void parseUserNameArgument (char *arg);
  inline void parseIDArgument (char *arg);
  inline void parsePasswordArgument (char *arg);
  inline void parseNodeNameArgument (char *arg);
  inline void parseUserProgramArguments (int argc, char *argv[]) const;

  inline void checkForRedundantArgument (bool &mark) const;

  bool parseIP (std::string &ip, uint32 &res) const;
  bool parsePort (const char *port, uint16 &res) const;

protected:
  bool doSigningInNegotiations (byte *buf);
  bool doMapSelectingNegotiations (byte *buf);
  bool doNodeSelectingNegotiations (byte *buf);
  bool doInformationSynchronizationNegotiations (byte *buf);
  bool doInterfacesInformationSynchronizationNegotiations (byte *buf);
  bool readInterfaceInformation (byte *buf, int index);
  bool doCustomInformationSynchronizationNegotiations (byte *buf);

  void usage () const;

private:
  static void *st_run (void *arg);
};

#endif /* cf.h */

