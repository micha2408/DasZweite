/**
* TopPipe - NamedPipe zu TopMan
*/

/**
* @author Stefan-W. Hahn <stefan.hahn@siemens.com>
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2006-2009 Siemens AG
*/

#include "topEthernet.hpp"
#include "TopPipe.hpp"
#include <regex>
// ================================================================================

CTimer* cPIPE::pTimer=0;

cPIPE::cPIPE(cTOPOLOGIE *_topologie_, const string &_pipeName_)
    : pipeMode('B')
    , dataTgr(0)
    , topologie(_topologie_)
    , pipeName(_pipeName_)
    , pipeHandle(INVALID_HANDLE_VALUE)
    , pipeOverlapped()
{
//    memset(&pipeOverlapped,0,sizeof(OVERLAPPED));
    pipeOverlapped.hEvent=CreateEvent(NULL,false,false,NULL);

    string pipeModeString;    
    if(topologie && topologie->getModule()->getAttribute("PipeMode",pipeModeString)>0)
    {
        if(pipeModeString[0]=='M')
        {
            pipeMode='M';
        }
        if(pipeModeString[0]=='m')
        {
            pipeMode='M';
        }
    }
}

cPIPE_CLIENT::cPIPE_CLIENT(cTOPOLOGIE *_topologie_, const string &pipeName)
: cPIPE(_topologie_, pipeName), reconnectTime(0)
        
{
    string sReconnectTime;
    if(topologie->getModule()->getAttribute("ReconnectTime", sReconnectTime)>0)
    {
        reconnectTime=atoi(sReconnectTime.c_str());
    }
    if(reconnectTime==0)
    {
        reconnectTime=500;
    }
    pTimerInterface=new CTimerInterface(pTimer,this,0);
    setPaused();
}

cPIPE_SERVER::cPIPE_SERVER(const string &pipeName)
    : cPIPE(NULLPTR, pipeName)
{
    DWORD openMode=
         PIPE_TYPE_BYTE |      // BYTE-Type Schreibrichtung
         PIPE_READMODE_BYTE |  // BYTE-Type Leserichtung
         PIPE_WAIT;               // blocking mode ( WAIT/NOWAIT macht keinen Unterschied....)

    if(isPipeModeMessage())
    {
        openMode |= PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE;
    }

    pipeHandle = CreateNamedPipe(
         pipeName.c_str(),   // pipe name
         PIPE_ACCESS_DUPLEX |     // read/write access
         FILE_FLAG_OVERLAPPED,    // overlapped mode
         openMode,
         1,               // number of instances
         MAX_TELEGRAMM_LAENGE,   // output buffer size
         MAX_TELEGRAMM_LAENGE,   // input buffer size
         NMPWAIT_USE_DEFAULT_WAIT,            // client time-out
         NULL);                   // default security attributes
    if(pipeHandle==INVALID_HANDLE_VALUE)
    {
        cTOPOLOGIE::error("cPIPE_SERVER::module=%s ServerPipe %s not created err=%d\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str(), GetLastError());
    } else
    {
        cTOPOLOGIE::eventloop()->bitRegisterCallback(this,pipeOverlapped.hEvent,8); // aktivieren
    }
}

cPIPE_SERVER::cPIPE_SERVER(cTOPOLOGIE *_topologie_, const string &pipeName)
    : cPIPE(_topologie_, pipeName)
{
    DWORD openMode=
         PIPE_TYPE_BYTE |      // BYTE-Type Schreibrichtung 
         PIPE_READMODE_BYTE |  // BYTE-Type Leserichtung 
         PIPE_WAIT;               // blocking mode ( WAIT/NOWAIT macht keinen Unterschied....)

    if(isPipeModeMessage())
    {
        openMode |= PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE;
    }
    
    pipeHandle = CreateNamedPipe(
         pipeName.c_str(),   // pipe name 
         PIPE_ACCESS_DUPLEX |     // read/write access 
         FILE_FLAG_OVERLAPPED,    // overlapped mode 
         openMode,
         1,               // number of instances 
         MAX_TELEGRAMM_LAENGE,   // output buffer size 
         MAX_TELEGRAMM_LAENGE,   // input buffer size 
         NMPWAIT_USE_DEFAULT_WAIT,            // client time-out 
         NULL);                   // default security attributes 
    if(pipeHandle==INVALID_HANDLE_VALUE)
    {
        static char tmp[100];
        sprintf(tmp,"cPIPE_SERVER::module=%s ServerPipe %s not created err=%d\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str(), GetLastError());
        cTOPOLOGIE::error(tmp);
    } else
    {
        cTOPOLOGIE::eventloop()->bitRegisterCallback(this,pipeOverlapped.hEvent,8); // aktivieren
        cTOPOLOGIE::error("cPIPE_SERVER::module=%s created ServerPipe %s\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str());
        setPaused();
    }
}

cPIPE::~cPIPE()
{
    CloseHandle(pipeHandle);
    CloseHandle(pipeOverlapped.hEvent);
}

// versuchen, wieder mit dem Server zu verbinden
void cPIPE_CLIENT::stopData() // disconnectPipe()
{
    setPaused();
    if(pipeHandle!=INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(pipeHandle);
        cTOPMANLOG::log("cPIPE_CLIENT::module=%s closed ClientPipe %s\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str());
        CloseHandle(pipeHandle); // falls geöffnet -> schliessen
        pipeHandle=INVALID_HANDLE_VALUE;
    }
}
// versuchen, wieder mit dem Server zu verbinden
void cPIPE_CLIENT::runData() // connectPipe()
{
    pipeHandle = CreateFile(
        pipeName.c_str(),   // pipe name 
        GENERIC_READ |  // read and write access 
        GENERIC_WRITE , 
        0,              // no sharing 
        NULL,           // default security attributes
        OPEN_EXISTING,  // opens existing pipe 
        FILE_FLAG_OVERLAPPED, // default attributes 
        NULL);          // no template file 
    if(pipeHandle!=INVALID_HANDLE_VALUE)
    {
        if(isPipeModeMessage())
        {
            // BYTE -> MESSAGE umschaltung
            DWORD state=0;
            GetNamedPipeHandleState(pipeHandle,&state,NULL,NULL,NULL,NULL,0);
            state |= PIPE_READMODE_MESSAGE;
            SetNamedPipeHandleState(pipeHandle,&state,NULL,NULL);
        }
        cTOPOLOGIE::error("cPIPE_CLIENT::module=%s created ClientPipe %s\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str());
        ReadFile(pipeHandle,dataTgr.data,sizeof(dataTgr.data),NULL,&pipeOverlapped);
        cTOPOLOGIE::eventloop()->bitRegisterCallback(this,pipeOverlapped.hEvent,CEventInterface::nEventDataWaiting);
        setRunning();
    } else
    {
        pTimerInterface->StartTimer(reconnectTime);
    }
}

// zyklisch versuchen wieder mit dem Server zu verbinden
void cPIPE_CLIENT::TimerCallback(void* /*pParam*/)
{
    runData();
}

void cPIPE::doReadEvent()
{
    DWORD transfered=0;
    GetOverlappedResult(pipeHandle,&pipeOverlapped,&transfered,false);
    if(transfered==0)
    {   // Keine Daten empfangen. Verbindung wurde beendet.
        stopData(); // disconnectPipe(); // client/server trennen
        runData(); // connectPipe();  // Client/Server wieder verbinden
    } else
    {
        while(transfered)
        {
            dataTgr.setDataLen(WORD(transfered));
            cTOPMANLOG::trace("cPIPE> received from %s:", pipeName.c_str());
            cTOPOLOGIE::dump(dataTgr);
            transfered=0;
            if(isRunning())
            {
                if(topologie)
                {
                    topologie->writeTopoData(dataTgr);
                } else
                {
                    std::string s(PCHAR(dataTgr.data),dataTgr.getDataLen());
                    std::regex rex("([^\\s,]+)\\s*,\\s*([^\\s,]+)");
                    std::smatch sm;
                    while(regex_search(s, sm, rex))
                    {
                        cTOPOLOGIE::doCommand(sm.str(1),sm.str(2));
                        s=sm.suffix();
                    }
                }
            }
            if(!ReadFile(pipeHandle,dataTgr.data,sizeof(dataTgr.data),&transfered,&pipeOverlapped))
            {   // keine weiteren Daten verfügbar
                break;
            }
        }
    }
}

// alte Clientpipe abtrennen und auf neue warten
void cPIPE_SERVER::stopData() // disconnectPipe()
{
    if(isRunning())
    {
        cTOPMANLOG::log("cPIPE_SERVER::module=%s disconnected ClientPipe %s\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str());
        FlushFileBuffers(pipeHandle);
        DisconnectNamedPipe(pipeHandle);
    }
    setPaused();
}
// auf neuen Client warten
void cPIPE_SERVER::runData()
{
    connectPipe();
}
void cPIPE_SERVER::connectPipe() // connectPipe()
{
    if(0==ConnectNamedPipe(pipeHandle,&pipeOverlapped))
    {   
        cTOPOLOGIE::error("cPIPE_SERVER::module '%s' ServerPipe '%s' Handle: %d  LastError:%d\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str(),pipeHandle, GetLastError());
        if(GetLastError()==ERROR_PIPE_CONNECTED)
        {   // bereits konnektiert. Gleich auf daten warten
            doWriteEvent();
        } else
        {   // auf konnekt warten
            cTOPOLOGIE::eventloop()->bitRegisterCallback(this,pipeOverlapped.hEvent,CEventInterface::nEventBufferEmpty);
        }
    } else
    {   // Konnektiert. damit ist aber bei asynchonem aufbau nicht zu rechnen
        doWriteEvent();
    }
}
void cPIPE_SERVER::doStartEvent()
{
    connectPipe();
}

// Client hat sich an Serverpipe connektiert
void cPIPE_SERVER::doWriteEvent()
{
    setRunning();
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,pipeOverlapped.hEvent,CEventInterface::nEventDataWaiting);
    cTOPMANLOG::log("cPIPE_SERVER::module %s ServerPipe %s Client Connected\n",topologie?topologie->getModuleName().c_str():"<none>", pipeName.c_str());
    ReadFile(pipeHandle,dataTgr.data,sizeof(dataTgr.data),NULL,&pipeOverlapped); // lese-Ereignis anfordern
}

int cPIPE::receiveData(DATATGR &tgr)
{   // von Layer empfangenes Telegramm
    DWORD written=0;
    WriteFile(pipeHandle,tgr.data,tgr.getDataLen(),&written,NULL);
    if(written!=tgr.getDataLen())
    {
        // eventuell fehlende daten in einer for-Schleife nachträglich in die Pipe schreiben ??
        cTOPMANLOG::log("cPIPE::module=%s length %d != written %d\n",topologie?topologie->getModuleName().c_str():"<none>",tgr.getDataLen(),written);
    }
    cTOPMANLOG::trace("cPIPE> send to %s:", pipeName.c_str());
    cTOPOLOGIE::dump(tgr);

    topologie->writeObserver(tgr);
    return 0;
}

// =eof=
