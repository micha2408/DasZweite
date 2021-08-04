#ifndef TOPCOMMAND_HPP
#define TOPCOMMAND_HPP
/**
* TOPPIPE Header
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#include "Interfaces.hpp"
#include "Topologie.hpp"
#include "TopPipe.hpp"
#include <string>
// ================================================================================


class cCOMMAND : public cCOMMAND_INTERFACE
{
public:
    cCOMMAND(cSEND_INTERFACE *t);
    int sendCmd(DATATGR &dataTgr) override;
private:
    cSEND_INTERFACE *sendInterface;
};

class cMESSAGE : public cMESSAGE_INTERFACE
{
public:
    cMESSAGE(cRECV_INTERFACE *si);
    // Verfügbarkeit speichern
    bool set(std::string modName, std::string cmd) override;
    // Verfügbarkeit speichern
    bool set(DATATGR &tgr) override;
//    message.send(); // eigene oder die anderen Verfügbarkeiten melden
    int sendMsg() override;
private:
    cRECV_INTERFACE *recvInterface;
    std::map<std::string,std::string> lastModState;
};



#endif

// =eof=
