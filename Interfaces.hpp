#ifndef INTERFACES_HPP
#define INTERFACES_HPP
/**
* COMMAND_INTERFACE
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/
#define WINDOWS_IGNORE_PACKING_MISMATCH
#include <EthernetTgrDefinitions.h>
#include <string>
#include <map>

// nach topologie oder pipe/tcp/udp/etc senden
class cSEND_INTERFACE
{
public:
    virtual int recvCmd(DATATGR &) { return false;} // als dummy nix tun
    virtual int sendState(DATATGR &){ return false;} // als dummy nix tun
//    void debug(){ printf("cSEND_INTERFACE\n");}

};

class cRECV_INTERFACE
{
public:
    virtual int recvMsg(DATATGR &) { return false;} // als dummy nix tun
};

class cCOMMAND_INTERFACE
{
public:
    virtual int sendCmd(DATATGR &) { return false;} // als dummy nix tun
    virtual void start(){}
};

class cMESSAGE_INTERFACE
{
public:
    virtual bool set(DATATGR &){return false;}; // als dummy nix tun
    virtual bool set(std::string, std::string){return false;} // als dummy nix tun
    virtual int sendMsg()
    {
        return 0;
    } // als dummy nix tun
    void debug(){ printf("cMESSAGE_INTERFACE\n");}
};



// Interface für alle Ethernet Empfänger
class TopoDataReceive
{
public:
    virtual int receiveData(DATATGR &tgr)=0;
    virtual void runData()=0;
    virtual void stopData()=0;
    enum ConnState { Invalid, Instantiated, Paused, Running, Connected };
    bool isValid(){ return connState>Instantiated; }
    bool isRunning(){ return connState>Paused; }
    bool isConnected(){ return connState>Running; }
protected:
    TopoDataReceive()
        : connState(Instantiated)
    {}
    void setPaused(){connState=Paused; }
    void setRunning(){connState=Running; }
//    bool setConnected(){connState=Connected; }
private:
    ConnState connState;
};

// =eof=
#endif
