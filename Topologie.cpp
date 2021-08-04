/**
* TopEthernet Source
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#include "topEthernet.hpp"
#include "TopConsole.hpp"
#include "TopPipe.hpp"
#include "TopCommand.hpp"
#include <regex>
#include <string>

#define USAGE \
"[-Vv] [-n] [-c command] [-C] [-p logpath] [-d debuglevel] [--] [compname]\n"\
"  -V, --version            Show version of program\n"\
"  -v, --verbose            Increment verbosity\n"\
"  -n, --noempty            No empty messages for ISOTP\n"\
"  -h, --help               This message\n"\
"  -i --ignoreAttributes    only Console, ignore "\
"  -c cmd, --command cmd    deactivate command interface with 'off'\n"\
"  -p logpath, --path logpath Path for logfile\n"\
"  -d level, --debuglevel     level Debuglevel for printing to logfile\n"\
"  Level can be LOG_NONE, LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,\n"\
"     LOG_NOTICE, LOG_INFO, LOG_DEBUG for file and console in following\n"\
"     notation:\n"\
"  level                    set filelevel and consolelevel\n"\
"  filelevel:consolelevel   set filelevel and consolelevel\n"\
"  filelevel                set filelevel, leave consolelevel untouched\n"\
"  :consolelevel            set consoleleve, leave filelevel untouched\n"\
"\n"\
" Defaults: filelevel=LOG_DEBUG, consolelevel=LOG_DEBUG\n"\
"           Log to file is inactive.\n"\
"           command interface is 'on'\n"

// ================================================================================


string cTOPOLOGIE::component("Ethernet"); // Defaultkomponentenname, wird durch Cmd-line Param ?berschrieben
// const char* cTOPOLOGIE::VERSION_MSG=APP_VERSION APP_BUILDDATE;
const char* cTOPOLOGIE::VERSION_MSG = "1.0.0 (" __DATE__ ")";

const char* cTOPOLOGIE::topEthernet_usage = USAGE;

int cTOPOLOGIE::filelevel = cLOGBASE::LOG_DEBUG;
int cTOPOLOGIE::consolelevel = cLOGBASE::LOG_DEBUG;
int cTOPOLOGIE::verbose = 0;
bool cTOPOLOGIE::noEmptyMessages=0;
bool  cTOPOLOGIE::useCommandInterface=1;
bool cTOPOLOGIE::ignoreAttributes=0;
cCOMMAND_INTERFACE cTOPOLOGIE::commandDummy;
cMESSAGE_INTERFACE cTOPOLOGIE::messageDummy;
cCOMMAND_INTERFACE* cTOPOLOGIE::pCommand=&cTOPOLOGIE::commandDummy;

CEventloop *cTOPOLOGIE::pEventloop=nullptr; // zum starten des Servers merken
cLOGBASE*  cTOPOLOGIE::pLog=nullptr;
cOBSERVER *cTOPOLOGIE::pObserver=nullptr;
map<string,map<string,string>> cTOPOLOGIE::compModRemoteList;
map<string,cTOPOLOGIE*> cTOPOLOGIE::modLocalList;


void cTOPOLOGIE::writeObserver(DATATGR &tgr)
{
    if(!pObserver) return;
    pObserver->sendToObserver(src_address, dst_address, *CTelegramm_PTR(&tgr));
}

const char *cTOPOLOGIE::read_port_numerical(const string &port)
{
    for(char c:port)
    {
        if(!isdigit(c))
        {
            return port.c_str();
        }
    }
    // ACHTUNG: Zuweisung muss eigene Codezeile sein, da static!
    static string new_port;
    new_port = port;
    while (new_port.size() < 5)
    {
        new_port.insert(0, "0");
    }
    return new_port.c_str();
}


cTOPOLOGIE::cTOPOLOGIE(cTOPMANMODULE* p_pModule)
    : cTOPMANUSER(p_pModule->getName())
    , pConn(NULL)
    , pModule(p_pModule)
    , handlerRemoteIp("")
    , myRemoteIp("")
    , lastMessageReturn(-1)
    , pMessage(&messageDummy)
//    , command(cmd>0)
{
    printf("cTOPOLOGIE::cTOPOLOGIE pMessage=%p",pMessage);
    static const int cLEN=4;
    char charvalue[cLEN];
    string modulename = pModule->getModuleName();
//    command &= (modulename=="Command");
    if (getAttribute(modulename.c_str(), "SrcCompTypeId", (char*)&charvalue, cLEN) >= 0)
        src_address.setComponentType(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "SrcCompId", (char*)&charvalue, cLEN) >= 0)
        src_address.setComponentIndex(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "SrcModuleSlotId", (char*)&charvalue, cLEN) >= 0)
        src_address.setModuleSlot(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "SrcModulePortId", (char*)&charvalue, cLEN) >= 0)
        src_address.setModulePort(atoi(charvalue));

    if (getAttribute(modulename.c_str(), "DestCompTypeId", (char*)&charvalue, cLEN) >= 0)
        dst_address.setComponentType(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "DestCompId", (char*)&charvalue, cLEN) >= 0)
        dst_address.setComponentIndex(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "DestModuleSlotId", (char*)&charvalue, cLEN) >= 0)
        dst_address.setModuleSlot(atoi(charvalue));
    if (getAttribute(modulename.c_str(), "DestModulePortId", (char*)&charvalue, cLEN) >= 0)
        dst_address.setModulePort(atoi(charvalue));

    cTOPMANLOG::error(PREFIX "set topaddress module=%s has SrcCompTypeId=%d SrcCompId=%d SrcModuleSlotId=%d SrcModulePortId=%d\n",
                modulename.c_str(),
                src_address.getComponentType(), src_address.getComponentIndex(),
                src_address.getModuleSlot(), src_address.getModulePort());

    cTOPMANLOG::error(PREFIX "set topaddress module=%s has DestCompTypeId=%d DestCompId=%d DestModuleSlotId=%d DestModulePortId=%d\n",
        modulename.c_str(),
        dst_address.getComponentType(), dst_address.getComponentIndex(),
        dst_address.getModuleSlot(), dst_address.getModulePort());

    if(ignoreAttributes)
    {
        pConn=new cCONSOLE(pModule->getModuleName());
        return;
    }
    string prot;
    pModule->getAttribute("EthProt", prot);
    const string destModuleName = p_pModule->getDestModuleName();
    auto itComp = compModRemoteList.find(p_pModule->getDestCompName());
    if(itComp!=compModRemoteList.end())
    {
        auto itMod=(itComp->second).find(destModuleName);
        if(itMod != (itComp->second).end())
        {
            handlerRemoteIp = cIP(itMod->second);
        }
    }
    if(handlerRemoteIp.is_valid())
    {
        trace("cTOPOLOGIE> module=%s is adapting ip=%s\n",destModuleName.c_str(), handlerRemoteIp.address_and_port().c_str());
    } else
    {
        trace("cTOPOLOGIE> module=%s handler ip not found\n", destModuleName.c_str());
    }

    if ((prot=="TCP") || (prot=="UDP") || (prot=="ISOTP"))
    {
        string value;
        pModule->getAttribute("Local", value); // Attribut irreführenderweise "Local", entspricht "Remote" beim Emu-Handler
        myRemoteIp/*localIp*/ =  cIP(value);
        pModule->getAttribute("Remote", value); // Attribut irreführenderweise "Remote", entspricht "Local" beim Emu-Handler
        cIP myLocalIp/*remoteIp*/(value);
        if(myLocalIp/*remoteIp*/.is_valid())
        {
            if(myRemoteIp/*localIp*/.is_valid() || myRemoteIp/*localIp*/.isDynamic())
            {
                if(prot=="ISOTP")
                {
                    static const int cPORT_STR_DIM = 128;
                    char myLocalIp_port[cPORT_STR_DIM];
                    char myRemoteIp_port[cPORT_STR_DIM];

                    // Die Ports werden bei ISOTP als String interpretiert und benoetigen führende Nullen
                    // 09.06.2021: remote und local wurden vertauscht
                    strncpy(myRemoteIp_port, read_port_numerical(myRemoteIp.port()), cPORT_STR_DIM);
                    strncpy(myLocalIp_port, read_port_numerical(myLocalIp.port()), cPORT_STR_DIM);
                    cIP isoLocalIp(myLocalIp.address(),102);
                    cIP isoRemoteIp(myRemoteIp.address(),102);
                    pConn = new cISOTP_ETHCONN(isoLocalIp,isoRemoteIp, *this,myLocalIp_port,myRemoteIp_port);
                    // Hinweis: Attribut ist "Local", bezichnet aber das anzuschlissende Gerät
                    trace("cTOPOLOGIE> module=%s local ip=%s(ISO-TP) ready\n",pModule->getName().c_str(),myRemoteIp.address_and_port().c_str());
                }
                if(prot=="TCP")
                {
                    // Verbindung ("connect") mit zu adaptierender Hardware vorbereiten
                    string ac;
                    pModule->getAttribute("AutoConnect",ac); // "1", anderen wert, oder nichts einlesen
                    pConn = new cTCP_ETHCONN(myLocalIp,myRemoteIp,*this,ac=="1");
                    trace("cTOPOLOGIE> module=%s \"Local\" ip=%s(TCP) ready\n",pModule->getName().c_str(),myRemoteIp.address_and_port().c_str());
                }
                if(prot=="UDP")
                {
                    pConn = new cUDP_ETHCONN(myLocalIp,myRemoteIp,*this);
                    trace("cTOPOLOGIE> module=%s \"Local\" ip=%s(UDP) ready\n",pModule->getName().c_str(),myRemoteIp.address_and_port().c_str());
                }
            } else
            {
                cTOPMANLOG::error("cTOPOLOGIE::cTOPOLOGIE> *** warning *** missing or invalid attribute \"Local\" for module=\"%s\"\n", pModule->getName().c_str());
            }
        } else
        {
            cTOPMANLOG::error("cTOPOLOGIE::cTOPOLOGIE> *** warning *** missing or invalid attribute \"Remote\" for module=\"%s\"\n", pModule->getName().c_str());
        }
    }
    if(prot=="")
    {   // kein Ethernet Protokoll angegeben
        string pipeName;
        if(pModule->getAttribute("PipeClient", pipeName)>0)
        {
            pConn=new cPIPE_CLIENT(this, pipeName);
        } else
        {
            if(pModule->getAttribute("PipeServer", pipeName)>0)
            {
                pConn=new cPIPE_SERVER(this, pipeName);
                if(!pConn->isValid())
                {
                    //                    delete pConn;
                    pConn=NULLPTR;
                }
            } else
            {
                // wenn nix passt ist es console
                pConn=new cCONSOLE(pModule->getModuleName());

            }
        }
    }
}


cTOPOLOGIE::~cTOPOLOGIE()
{
    // aufräumen nicht vergessen...
}

void cTOPOLOGIE::start()
{   // Topologieverbindung hergestellt
    pLog->log("cTOPOLOGIE::start> called for module=\"%s\"\n",pModule->getName().c_str());
    pConn->runData();
//    pCommand->start(); // einmalig
    pMessage->set(getModuleName(),"on"); // eigenene Verfügbarkeit speichern
    pMessage->sendMsg(); // eigene oder die anderen Verfügbarkeiten melden
}


void cTOPOLOGIE::run()
{   // Topologieverbindung hergestellt
    pLog->log("cTOPOLOGIE::run> called for module=\"%s\"\n",pModule->getName().c_str());
    pConn->runData();
//    pCommand->start(); // einmalig
    pMessage->set(getModuleName(),"on"); // eigenene Verfügbarkeit speichern
    pMessage->sendMsg(); // eigene oder die anderen Verfügbarkeiten melden
}

void cTOPOLOGIE::stop()
{
   pLog->log("cTOPOLOGIE::stop> called for module=\"%s\"\n",pModule->getName().c_str());
   pConn->stopData();
   pMessage->set(getModuleName(),"off"); // eigenene Verfügbarkeit speichern
   pMessage->sendMsg(); // eigene oder die anderen Verfügbarkeiten melden
}

int cTOPOLOGIE::receive(CTelegramm telegram)
{
    switch(reinterpret_cast<TOPOTGR*>(&telegram)->Type)
    {
    case TOPOTGR::nCommand:
        break;
    case TOPOTGR::nConfig:
        break;
    case TOPOTGR::nData:
    {   // von Layer empfangenes Daten Telegramm
        pLog->log("cTOPOLOGIE::receive> pConn->isRunning() = %d module=\"%s\"\n",pConn->isRunning(),pModule->getName().c_str());
        if(!pConn->isRunning()) return 0;
        DATATGR &tgr=*reinterpret_cast<DATATGR*>(&telegram);
        pCommand->sendCmd(tgr);   // u.U. als kommando bearbeiten
        pConn->receiveData(tgr);  // weiterleiten. Als Kommando hier nur noch protokollieren
        break;
    }
    default:;
    }

    return 0;
}
int cTOPOLOGIE::sendState(DATATGR &tgr)
{   // umleiten??
    return pConn->receiveData(tgr);
}

int cTOPOLOGIE::writeTopoData(DATATGR &tgr)
{   // an Layer zu sendendes Telegramm
    if(!pConn->isRunning()) return -1;
    if(tgr.partnerAddress.value==0)
    {
        tgr.partnerAddress.value = handlerRemoteIp.addressnum();
    }
    if(tgr.partnerSubAddr==0)
    {
        tgr.partnerSubAddr = handlerRemoteIp.portnum();
    }
    return send(*CTelegramm_PTR(&tgr));
}

// ================================================================================

int cTOPOLOGIE::main( int argc, char *argv[])
{
    string logfile = "";
    bool log_to_file = false;
    char level[80] = "LOG_DEBUG:LOG_DEBUG";

    while (1)
    {
        int option_index = 0;
        static struct option long_options[] = {
            {"version",0,NULL,'V'},
            {"help", 0,NULL,'H'},
            {"verbose",0,NULL,'v'},
            {"noempty",0,NULL,'n'},
            {"command", 1, NULL, 'c'},
            {"Console", 1, NULL, 'C'},
            {"path", 1, NULL, 'p'},
            {"debuglevel", 1, NULL, 'd'},
            {NULL,0,NULL,0}
        };
        int option = getopt_long( argc, argv, "VvHnc:Cp:d:", long_options, &option_index);
        if (EOF == option) break;
        switch (option)
        {
        case 'V':
            print_version();
            break;
        case 'H':
            usage(topEthernet_usage);
            break;
        case'v':
            verbose++;
            break;
        case 'n':
            noEmptyMessages=true;
            break;
        case 'd':
            if (!optarg)
                usage(topEthernet_usage);
            strcpy(level, optarg);
            break;
        case 'i':
            ignoreAttributes=true;
            break;
        case 'c':
            if (!optarg)
                usage(topEthernet_usage);
            if(strcmp(optarg,"on")) useCommandInterface=true;
            if(strcmp(optarg,"ON")) useCommandInterface=true;
            if(strcmp(optarg,"On")) useCommandInterface=true;
            if(strcmp(optarg,"off")) useCommandInterface=false;
            if(strcmp(optarg,"OFF")) useCommandInterface=false;
            if(strcmp(optarg,"Off")) useCommandInterface=false;
            break;
        case 'p':
            if (!optarg)
                usage(topEthernet_usage);
            logfile = optarg;
            log_to_file = true;
            break;
        default:
            usage(topEthernet_usage);
            break;
        }

    }
    if (optind == argc)
    {
        usage(topEthernet_usage);
        exit( 1);
    }
    if(argv[optind])
    {
        component=argv[optind];
    }
    if (!checklevel(level, filelevel, consolelevel))
    {
        usage(topEthernet_usage);
        exit( 1);
    }

    if (log_to_file)
    {
        if (logfile == "")
            logfile += ".";
        logfile += "/#c_#s.txt"; // Komponente + Datum
    }

    cTopoComponentInit topo_component(argv[0], component.c_str(), logfile, filelevel, consolelevel);
    if (topo_component.firstStageInit())
    {
        pEventloop = topo_component.getEventloopPtr();
    } else
    {
        exit(-1);
    }

    pLog = topo_component.getLogPtr();
    if (pLog == NULL)
    {
        fprintf(stderr, PREFIX "*** fatal *** cannot instantiate logbase.\n");
        exit(-1);
    }

#ifdef _WINDOWS
    char Title[128] = { 0 };
    sprintf( Title, "%s TopEthernet Adapter v%s/%s (c) Siemens Mobility GmbH",
        topo_component.getLockId().c_str(),
             VERSION_MSG, TOPMAN_VERSION_NUMBER);
    SetConsoleTitle( Title );
#endif

    // --------------------------------------------------------------------------------
    const MAPMODULES& modules(topo_component.getModules());
    cPIPE_CLIENT::setTimer(topo_component.getTimerPtr());
    cMESSAGE_INTERFACE* pMessage=&cTOPOLOGIE::messageDummy;
    for (MAPMODULES::const_iterator it(modules.begin()); it != modules.end(); it++)
    {
        cTOPMANMODULE *pModule(it->second);
        string moduleName(pModule->getModuleName());
        string destCompName(pModule->getDestCompName());
        if(compModRemoteList.find(destCompName)==compModRemoteList.end())
        {   // remote komponente noch unbekannt
            cTOPMAN remoteTopman(cTopManAddress::getTopManIp().c_str(),cTopManAddress::getTopManPort().c_str(),pModule->getDestCompName().c_str(),false);
            remoteTopman.setEventInterface(NULLPTR); // Hiermit werden die Daten vom TopServer abgerufen
            MAPMODULES modMap=remoteTopman.getModules();
            for(auto itMod:modMap)
            {
                string value;
                itMod.second->getAttribute("Remote", value);
                compModRemoteList[pModule->getDestCompName()][itMod.first]=value;
            }
        }
        string type = "";
        pModule->getAttribute("ModuleType", type);
        if (type == "typeOBSERVER")
        {
            // Observer-Modul gefunden
            if (pObserver != NULL)
                cTOPMANLOG::error(PREFIX "*** warning *** another observer defined, ignoring\n");
            else
            {
                cTOPMANLOG::error(PREFIX "found observer\n");
                pObserver = new cOBSERVER(moduleName);
            }
        } else
        {   // nicht Observer Modul gefunden
            cTOPOLOGIE *pTopologie=new cTOPOLOGIE(pModule);
            if(!pTopologie->isConnValid())
            {
                cTOPOLOGIE::error(PREFIX, "*** fatal *** connection of module=%s not valid.\n",pModule->getModuleName().c_str());
                goto undTschuess;
            }
            modLocalList[moduleName]=pTopologie;
//            if(0==moduleName.find("Command_"))
            if(moduleName=="Command")
            {   // über dieses modul sollen zustände zwischengepuffert werden
                pMessage = new cMESSAGE(pTopologie);
                pTopologie->pMessage = pMessage;
                pTopologie->pMessage->sendMsg();
                pCommand=new cCOMMAND(pTopologie);
            }
        }
    }
    if(useCommandInterface)
    {
        if(pCommand==&commandDummy)
        {   // noch nicht definiert. daher muss eine pipeschnittstelle her
            string pipeName("\\\\.\\pipe\\Command_");
            pipeName += component;
            cPIPE_SERVER *ps= new cPIPE_SERVER(pipeName);
            pCommand=new cCOMMAND(ps);
            pMessage=new cMESSAGE(ps);
        }
        for(auto iTop:modLocalList)
        {
            iTop.second->pMessage=pMessage; // für zustandsmeldugnen
        }
    }

    if (pObserver == NULL)
    {
        cTOPOLOGIE::error(PREFIX "*** warning *** no connection to observer defined.\n");
    }

    // ...und nun ab in die zentrale Eventloop...
    topo_component.start();

undTschuess:

    if (pObserver != NULL)
    {
        delete pObserver;
    }
#ifdef _WINDOWS
    SetConsoleTitle("");
#endif

    return 0;
}

void cTOPOLOGIE::usage(const char *err)
{
    fprintf(stderr, "usage: %s\n", err);
    exit(129);
}

void cTOPOLOGIE::print_version()
{
    printf("topEthernet - Software zum Anschluss von Ethernet-Schnittstellen an TopMan v%s/%s\n", VERSION_MSG, TOPMAN_VERSION_NUMBER);
    printf("Alias topPipe - Software zum Anschluss von Windows Named Pipes\n");
    printf("Alias topCommand - Software zum Protokollieren\n");
    printf("(c) 2021 Siemenes Mobility GmbH\n");
    exit(0);
}
//void cTOPOLOGIE::doCommandAll(string command)
//{
//    // "off", "on";
//    for(auto it:modLocalList)
//    {
//        if(it.second->isCommand()) continue; // nicht sich selber kommandieren
//        doCommand(it.first, command);
//    }
//}
int cTOPOLOGIE::recvCmd(DATATGR &dataTgr)
{
    std::string s(PCHAR(dataTgr.data),dataTgr.getDataLen());
    std::regex rex("([^\\s,]+)\\s*,\\s*([^\\s,]+)");
    std::smatch sm;
    while(regex_search(s, sm, rex))
    {
        cTOPOLOGIE::doCommand(sm.str(1),sm.str(2));
        s=sm.suffix();
    }
    return 0;
}

int cTOPOLOGIE::recvMsg(DATATGR &dataTgr)
{
    return writeTopoData(dataTgr);
//    std::string s(PCHAR(dataTgr.data),dataTgr.getDataLen());
//    std::regex rex("([^\\s,]+)\\s*,\\s*([^\\s,]+)");
//    std::smatch sm;
//    while(regex_search(s, sm, rex))
//    {
////        cTOPOLOGIE::doCommand(sm.str(1),sm.str(2));
//        s=sm.suffix();
//    }
//    return 0;
}
void cTOPOLOGIE::doCommand(string module, string command)
{
//    if((command=="off") && isComponentCommand())
//    {
//        doMessage(module);
//        return; // nciht sich selber deaktivieren
//    }
    auto itOwn=modLocalList.find(module);
    if(itOwn != modLocalList.end())
    {
        itOwn->second->getTopMan()->setBlockedByUser(module.c_str(),command=="off");
    }
}

//void cTOPOLOGIE::doMessageAll()
//{
//    for(auto it:modLocalList)
//    {
////        if(it.second->isCommand()) continue; // nicht sich selber melden
//        doMessage(it.first);
//    }

//}
//void cTOPOLOGIE::doMessage(string module)
//{
//    string answer   =module+",";
//    auto it=modLocalList.find(module);
//    if(it==modLocalList.end()) return;
//    answer += it->second->isConnected()?"on\n":"off\n";
//    DATATGR tgr(2);
//    tgr.setData(answer.c_str(),WORD(answer.size()));
//    // it.second->lastMessageReturn =
//    int ret=pCommandServer->message(tgr); // bekanntmachen
//    error("cTOPOLOGIE> mod '%s' : '%s' MessageReturn=%d\n",module.c_str(), answer.c_str(),ret);
//}


//// ================================================================================



cOBSERVER::cOBSERVER(const string& p_Modulename)
    : cTOPMANUSER(p_Modulename)
    , started(false)
    , running(false)
    , modulename(p_Modulename)
{
}

void cOBSERVER::start()
{
    if (!started)
    {
        started = true;
        cTOPMANLOG::error("cOBSERVER::start> module=\"%s\"\n", modulename.c_str());
    }
    else
        cTOPMANLOG::error("cOBSERVER::start> *** warning *** ignoring additional start-command\n");
}

void cOBSERVER::run()
{
    running = true;
    cTOPMANLOG::error("cOBSERVER::run> was called for module=\"%s\"\n", modulename.c_str());
}

void cOBSERVER::stop()
{
    running = false;
    cTOPMANLOG::error("cOBSERVER::stop> was called for module=\"%s\"\n", modulename.c_str());
}

int cOBSERVER::receive(CTelegramm)
{
    cTOPMANLOG::error("cOBSERVER::receive> *** fatal *** not implemented.\n");
    return 0;
}

int cOBSERVER::send(CTelegramm telegramm)
{
    cTOPOLOGIE::dump("cOBSERVER::send:",PCSTR(telegramm.getBuffer()), telegramm.getLength());
    if (running)
    {
        return cTOPMANUSER::send(telegramm);
    }
    return 0;
}

/**
* Datenformat zum Observer:
*
* TUROUTINGHEADER routing_header
*   Byte 0         == 0x40
*   Byte 1..11     == 0
*   Byte 12..15    Sendezeitstempel (Sekunden)
*   Byte 16..17    Sendezeitstempel (Millisekunden)
* TUNETADDRESS sourceAddress
*   Byte 18        compType (CompTypeId)
*   Byte 19        compIndex (CompId)
*   Byte 20        moduleSlot (ModuleTypeId)
*   Byte 21        modulePort (== 0)
* TUNETADDRESS destinationAddress
*   Byte 22        compType (CompTypeId)
*   Byte 23        compIndex (CompId)
*   Byte 24        moduleSlot (ModuleTypeId)
*   Byte 25        modulePort (== 0)
* Data
*   Byte 26..      Telegramm
*/
void cOBSERVER::sendToObserver(const cTOPO_ADDRESS &src, const cTOPO_ADDRESS &dst, CTelegramm telegramm)
{
    TUTGRNETHEADER net_header={};
    src.set_netaddress(&net_header.sourceAddress);
    dst.set_netaddress(&net_header.destinationAddress);

    net_header.routing_header.tgr_art = 0x40; // *FIX*
    struct timeval time_now;
    struct timezone tz;
    gettimeofday(&time_now, &tz);
    net_header.routing_header.timestamp_seconds = time_now.tv_sec;
    net_header.routing_header.timestamp_milliseconds = WORD(time_now.tv_usec / 1000);

    telegramm.fuege_Kopf_ein(sizeof(TUTGRNETHEADER), (BYTE *)&net_header);

    if(send(telegramm) != 0)
    {
        cTOPMANLOG::error(PREFIX "*** fatal *** cannot send telegram to observer module=\"%s\"\n", modulename.c_str());
    }
}

// ================================================================================

int main(int argc, char *argv[])
{
    return cTOPOLOGIE::main(argc, argv);
}

// =eof=
