#ifndef TOPPIPE_HPP
#define TOPPIPE_HPP
/**
* TOPPIPE Header
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#include <topmanuser.hpp>
#include "topEthernet.hpp"

// ================================================================================

class cTOPOLOGIE;
class cPIPE
    : /*public cTOPMANUSER, */public cTOPMANCALLBACK, public TopoDataReceive
{
private:
    CHAR pipeMode;
    void doReadEvent() override;
public:
    int receiveData(DATATGR &tgr) override;
protected:
    DATATGR dataTgr;
    cTOPOLOGIE *topologie;
    static CTimer* pTimer;
    string pipeName;
    HANDLE pipeHandle;
    OVERLAPPED pipeOverlapped;

public:
    static void setTimer(CTimer* _pTimer_) { pTimer = _pTimer_; }
    bool isPipeModeMessage(){ return pipeMode=='M';}
    cPIPE(cTOPOLOGIE *_topologie_, const string &_pipeName_);
    virtual ~cPIPE();
};
class cPIPE_CLIENT
    : public cPIPE, /* public cTOPMANCALLBACK, */ public CTimerObject
{
public:
    cPIPE_CLIENT(cTOPOLOGIE *_topologie_, const string &pipeName);
    virtual ~cPIPE_CLIENT(){};
private:
    CTimerInterface* pTimerInterface;
    void TimerCallback(void* pParam) override;
    void KillTimerCallback(void* pParam) override {}
    void runData() override;
    void stopData() override;
    int reconnectTime;
};

class cPIPE_SERVER
    : public cPIPE , public cRECV_INTERFACE , public cSEND_INTERFACE
{
public:
    cPIPE_SERVER(const string &pipeName);
    cPIPE_SERVER(cTOPOLOGIE *_topologie_, const string &pipeName);
    virtual ~cPIPE_SERVER(){}
    void connectPipe(); // connectPipe()
    int recvCmd(DATATGR &tgr) override
    {
        return 0;
//        return receiveData(tgr); // kommandieren
    }
    int recvMsg(DATATGR &tgr) override
    {
        return receiveData(tgr); // nach pipe schreiben
    }
private:
    void runData() override;
    void doWriteEvent() override;
    void doStartEvent() override;
    void stopData() override;

};

#endif

// =eof=
