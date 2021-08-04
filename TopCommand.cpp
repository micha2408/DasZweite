/**
* TopPipe - NamedPipe zu TopMan
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#include "TopCommand.hpp"
#include "TopPipe.hpp"
#include "Interfaces.hpp"
#include <regex>
#include <string>
// ================================================================================


cCOMMAND::cCOMMAND(cSEND_INTERFACE *t)
    : sendInterface(t)
{
}

int cCOMMAND::sendCmd(DATATGR &dataTgr)
{
    return sendInterface->recvCmd(dataTgr);
}

cMESSAGE::cMESSAGE(cRECV_INTERFACE *ri)
    : recvInterface(ri)
{
    printf("recvInterface %p\n",recvInterface);
}
// Verfügbarkeit speichern
bool cMESSAGE::set(std::string modName, std::string cmd)
{
    lastModState[modName]=cmd;
    return true;
}
// Verfügbarkeit speichern
bool cMESSAGE::set(DATATGR &tgr)
{
    std::string s(PCHAR(tgr.data),tgr.getDataLen());
    std::regex rex("([^\\s,]+)\\s*,\\s*([^\\s,]+)");
    std::smatch sm;
    while(regex_search(s, sm, rex))
    {   // letzten Zustand sichern
        set(sm.str(1),sm.str(2));
        s=sm.suffix();
    }
    return true;
}
int cMESSAGE::sendMsg()
{
    int count=0;
    for(auto itMod=lastModState.begin(); itMod!=lastModState.end(); itMod++)
    {        
        std::string cmd(itMod->first.c_str());
        cmd.append(",");
        cmd.append(itMod->second.c_str());
        cmd.append("\n");
        DATATGR tgr(2);
        tgr.setData(cmd.c_str(),WORD(cmd.size()));
        // an den hinterelgten Anschluss weitermelden Pipe/Topo
        recvInterface->recvMsg(tgr);
    }
    return count;
}



// =eof=
