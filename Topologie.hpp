#ifndef TOPOLOGIE_HPP
#define TOPOLOGIE_HPP
/**
* TopEthernet Header
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#define _WIN32_WINNT 0x0500
#define _CRT_SECURE_NO_DEPRECATE

// ================================================================================

#define PREFIX "TopEthernet> "

#include "os_basics.hpp"
#include "topo_init.hpp"
#include "logutils.hpp"
#include "layer.hpp"
#include "topmanuser.hpp"
#include "telegramm.hpp"
#include "topversion.hpp"
#include "eventloop.hpp"
#include "ip.hpp"
#ifndef _WINDOWS
#include "timer.hpp"
#include "file.hpp"
#include "log.hpp"
#include <errno.h>
#else
#include "time_compat.hpp"
#endif
#include "exithandler.hpp"

#include <stdlib.h>
#ifdef _WINDOWS
#include <Iphlpapi.h>
#include <conio.h>
#include <windows.h>
#include <sys/timeb.h>
#endif
#ifndef FREMDLING
#define FREMDLING
#endif
#include <TUTgrHeader.h>
#include <iostream>
#include <time.h>
#include <map>
#include <vector>
#include <queue>
#include "TopCommand.hpp"
#include <EthernetTgrDefinitions.h>

using namespace std;

#define __STDC__ 1
#include "getopt.h"

// ================================================================================

class cTOPO_ADDRESS
{
private:
    static const byte cUNKNOWN = 0;
    byte m_component_type;
    byte m_component_index;
    byte m_module_slot;
    byte m_module_port;
public:
    cTOPO_ADDRESS()
        : m_component_type(cUNKNOWN)
        , m_component_index(cUNKNOWN)
        , m_module_slot(cUNKNOWN)
        , m_module_port(cUNKNOWN) {}
      virtual ~cTOPO_ADDRESS() {}

      byte getComponentType() const { return m_component_type; }
      byte getComponentIndex() const { return m_component_index; }
      byte getModuleSlot() const { return m_module_slot; }
      byte getModulePort() const { return m_module_port; }

      void setComponentType(byte comp_type) { m_component_type = comp_type; }
      void setComponentIndex(byte comp_index) { m_component_index = comp_index; }
      void setModuleSlot(byte module_slot) { m_module_slot = module_slot; }
      void setModulePort(byte module_port) { m_module_port = module_port; }

      void set_netaddress(TUNETADDRESS *pAdr) const
      {
          pAdr->componentType = m_component_type;
          pAdr->componentIndex = m_component_index;
          pAdr->moduleSlot = m_module_slot;
          pAdr->modulePort = m_module_port;
      }
};

class cOBSERVER;
class cETHCONNECTBASE;
class TopoDataReceive;

#define VA(fmt,tmp)\
    char tmp[1024];\
    va_list args;\
    va_start(args, fmt);\
    vsprintf(tmp,fmt,args);\
    va_end(args);

class cTOPOLOGIE
    : public cTOPMANUSER, public cSEND_INTERFACE, public cRECV_INTERFACE
{
private:
    TopoDataReceive *pConn;
    cTOPMANMODULE* pModule;
    static CEventloop *pEventloop;
    static cLOGBASE*  pLog;
    static cOBSERVER *pObserver;
    static bool useCommandInterface; // false: not used, true: requested
    static bool ignoreAttributes;
    cIP handlerRemoteIp;
    cIP myRemoteIp; // localIp;
    cTOPO_ADDRESS src_address;
    cTOPO_ADDRESS dst_address;
    int lastMessageReturn;
    const char *read_port_numerical(const string &port);
    static string component;
    static int filelevel;
    static int consolelevel;
    static int verbose;
    static bool noEmptyMessages;
    static void usage(const char *err);
    static void print_version();
    static const char *VERSION_MSG;
    static const char *topEthernet_usage;
    static map<string,map<string,string>> compModRemoteList;
    static map<string,cTOPOLOGIE*> modLocalList;
    static cCOMMAND_INTERFACE commandDummy;
    static cMESSAGE_INTERFACE messageDummy;
    static cCOMMAND_INTERFACE* pCommand;
    cMESSAGE_INTERFACE* pMessage;
//    bool command;
public:
//    int resendMessages()
//    {
//        return pCommandServer->resendMessages();
//    }
    cTOPOLOGIE(cTOPMANMODULE* p_pModule);
    virtual ~cTOPOLOGIE();
//    bool isCommand();
//    static bool isComponentCommand();

    cTOPMANMODULE* getModule() const { return pModule; }
    typedef CTelegramm * CTelegramm_PTR;
    inline static CEventloop *eventloop()
    {
        return pEventloop;
    }
    inline static void logVerbose(const char* fmt, ...)
    {
        if (verbose > 0)
        {
            VA(fmt,tmp);
            cTOPMANLOG::log(tmp);
        }
    }
    template <typename T>
    static void dump(T prefix, T data, int len)
    {
        if (verbose > 0)
        {
            cTOPMANLOG::dump(cLOGBASE::LOG_DEBUG, PCSTR(prefix), data, len);
        }
    }
    inline static void error(const char* fmt, ...)
    {
        VA(fmt,tmp);
        cTOPMANLOG::error(tmp);
    }
    inline static void log(const char* fmt, ...)
    {
        VA(fmt,tmp);
        cTOPMANLOG::log(tmp);
    }
    inline static void trace(const char* fmt, ...)
    {
        VA(fmt,tmp);
        cTOPMANLOG::trace(pLog->getLoglevelFile(),tmp);
    }
    inline static void dump(DATATGR &tgr)
    {
        cTOPMANLOG::dump(pLog->getLoglevelFile(),PCHAR(tgr.data),tgr.getDataLen());
    }
    template <typename T>
    inline static void dump(T data, int len)
    {
        cTOPMANLOG::dump(pLog->getLoglevelFile(),PCSTR(data),len);
    }

    inline static cOBSERVER *observer()
    {
        return pObserver;
    }
    static int main( int argc, char *argv[]);
    inline bool isConnValid()
    {
        return pConn != nullptr;
    }
    static inline bool emptyMessagesForbidden()
    {
        return noEmptyMessages;
    }
    void start() override;
    void run() override;
    void stop() override;
    int receive(CTelegramm telegramm) override;
    int sendState(DATATGR &tgr) override;
    int recvCmd(DATATGR &tgr) override;
    int recvMsg(DATATGR &tgr) override;
    int writeTopoData(DATATGR &tgr) ;
    void writeObserver(DATATGR &tgr);
    static void doCommand(string module, string command);
//    static void doMessage(string module);
//    static void doCommandAll(string command);
//    static void doMessageAll();

};


// ================================================================================

class cOBSERVER : public cTOPMANUSER
{
private:
    bool started;
    bool running;
    string modulename;
public:
    cOBSERVER(const string& p_Modulename);
    void start() override;
    void run() override;
    void stop() override;
    ~cOBSERVER()  override {}
    int receive(CTelegramm telegramm) override;
    int send(CTelegramm telegramm) override;

    void sendToObserver(const cTOPO_ADDRESS &src, const cTOPO_ADDRESS &dst, CTelegramm telegramm);
};


// =eof=
#endif
