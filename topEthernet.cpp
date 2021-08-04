/**
* TopEthernet Source
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "topEthernet.hpp"
#include "Topologie.hpp"

map<string,cTCP_ETHSERVER*> cTCP_ETHSERVER::pServer_map;


void cTCP_ETHCONN::runData()
{   // topologie verbunden
    if(isRunning()) return; // bereits voll in Betrieb
    if(TopoDataReceive::isValid())
    {   //Initialisiert, aber pausiert
        setRunning();
        return;
    }
    cTOPOLOGIE::log("cTCP_ETHCONN::start> module=%s \"Local\" ip=%s \"Remote\" ip=%s\n",topologie.getModuleName().c_str(),remoteIp.address_and_port().c_str(),localIp.address_and_port().c_str());
    // für jeden "remote"-port einen Server starten, der Verbindungen von zu adaptierender Hardware annimmt
    cTCP_ETHSERVER *pServer=new cTCP_ETHSERVER(localIp,this);
    if(pServer->valid())
    {
        cTOPOLOGIE::log("cTCP_ETHCONN> new TCP server for %s started\n",localIp.address_and_port().c_str());
    } else
    {
        cTOPOLOGIE::log("cTCP_ETHCONN> TCP server for %s not startet\n",localIp.address_and_port().c_str());
        delete pServer;
    }
    setRunning();
}

void cISOTP_ETHCONN::runData()
{   // topologie verbunden
    if(isRunning()) return; // bereits voll in Betrieb
    if(TopoDataReceive::isValid())
    {   //Initialisiert, aber pausiert
        setRunning();
        return;
    }
    cTOPOLOGIE::log("cISOTP_ETHCONN::start> module=%s \"Local\" ip=%s \"Remote\" ip=%s tsap %s\n",topologie.getModuleName().c_str(),isoLocalIp.address().c_str(),isoRemoteIp.address().c_str(),local_tsap.c_str());
    // für jede IP-Adresse einen Server starten, der Verbindungen von zu adaptierender Hardware annimmt
    cTCP_ETHSERVER *pServer=cTCP_ETHSERVER::getServer(isoLocalIp.address());
    if(pServer)
    {
        cTOPOLOGIE::log("cISOTP_ETHCONN> (ISO-)TCP server %s for tsap %s is already running\n",isoLocalIp.address_and_port().c_str(),local_tsap.c_str());
        pServer->setConnISO(this); // in liste eintragen
    } else
    {
        pServer=new cTCP_ETHSERVER(isoLocalIp);
        if(pServer->valid())
        {
            cTOPOLOGIE::log("cISOTP_ETHCONN> new (ISO-)TCP server %s for tsap %s started\n",isoLocalIp.address_and_port().c_str(),local_tsap.c_str());
            pServer->setConnISO(this); // in liste eintragen
            cTCP_ETHSERVER::setServer(isoLocalIp.address(),pServer);
        } else
        {
            cTOPOLOGIE::log("cISOTP_ETHCONN> (ISO-)TCP server %s for tsap %s not started\n",isoLocalIp.address_and_port().c_str(),local_tsap.c_str());
            delete pServer;
        }
    }
    setRunning();
}

void cUDP_ETHCONN::stopData()
{
    setPaused();
}

void cUDP_ETHCONN::runData()
{
    if(isRunning()) return; // bereits voll in Betrieb
    if(TopoDataReceive::isValid())
    {   //Initialisiert, aber pausiert
        setRunning();
        return;
    }
    cTOPOLOGIE::log("cUDP_ETHCONN::start> module=%s \"Local\" ip=%s \"Remote\" ip=%s\n",topologie.getModuleName().c_str(),remoteIp.address_and_port().c_str(),localIp.address_and_port().c_str());
    if((topologie.getModule()->getMode()[0]=='w') || (topologie.getModule()->getMode()[1]=='w'))
    {   // für jeden "remote"-port einen Server starten, der Verbindungen von zu adaptierender Hardware annimmt
        if (remoteIp.is_valid())
        {
            altAddr.sin_family = AF_INET;
            altAddr.sin_port = htons(remoteIp.portnum());
            altAddr.sin_addr.s_addr = inet_addr(remoteIp.address().c_str());
            cTOPOLOGIE::log("cTOPOLOGIE> initialize dest addr to local addr %s:%d\n",inet_ntoa(altAddr.sin_addr), htons(altAddr.sin_port));
        }
        cUDP_ETHHANDLER *pHandler=new cUDP_ETHHANDLER(localIp,*this);
        if(pHandler->valid())
        {
            cTOPOLOGIE::log("cUDP_ETHCONN> new UDP handler for %s started\n",localIp.address_and_port().c_str());
        } else
        {
            cTOPOLOGIE::log("cUDP_ETHCONN> UDP handler for %s is already running\n",localIp.address_and_port().c_str());
            delete pHandler;
        }
    } else
    {   // Kein Empfang. Trotzdem wird ein socket zum senden ben?tigt
        int sock=socket (AF_INET, SOCK_DGRAM, 0);
        setFD(sock); // w?re eigentlich im Handler erfolgt...
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(localIp.portnum());
        addr.sin_addr.s_addr=inet_addr(localIp.address().c_str());
        makeAsynchron(sock);
        int doedel = 1;
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&doedel,sizeof(int));
        if(bind(sock, (sockaddr *) &addr, sizeof(sockaddr)) == SOCKET_ERROR)
        {
            cTOPOLOGIE::error("cUDP_ETHCONN> cUDP_ETHCONN bind ERROR rc=%d\n", geterrno());
        }
    }
    setRunning();
}

void cISOTP_ETHCONN::stopData()
{
    iso_abbau();
    setPaused();
}

void cTCP_ETHCONN::stopData()
{
    abbau();
    setPaused();
}

int cISOTP_ETHCONN::receiveData(DATATGR &tgr)
{
        // da senddata auch nach akzeptierten Verbindungen sendet, hier schon aufbauen
        rfc1006_datagram_t rfc1006_datagram;
        int wlength=eERR;
        cTOPOLOGIE::log("rcv(0):isotp_cmd=%d\n",tgr.cmd);
        switch(tgr.cmd)
        {
        case cISOTP_ETHCONN::nBroadcast:
        case cISOTP_ETHCONN::nUnicast:
            // Das telegramm aufbauen
            wlength=iso_make_DT(tgr.data,tgr.getDataLen(),rfc1006_datagram,false);
            break;
        case cISOTP_ETHCONN::nUnicast_beschleunigt:
            // Das telegramm aufbauen
            wlength=iso_make_DT(tgr.data,tgr.getDataLen(),rfc1006_datagram,true);
            break;
        case cISOTP_ETHCONN::nVerbindungsabbau:
            iso_abbau();
            break;
        case cISOTP_ETHCONN::nVerbindung_hergestellt:
        case cISOTP_ETHCONN::nVerbindungsaufbau:
            if(check_acceptance())
            {   // verbindung ist aufgebaut
                DATATGR tgr(cISOTP_ETHCONN::nVerbindung_hergestellt);
                writeTopoData(tgr);
            } else
            {
                wlength=iso_make_CR(rfc1006_datagram,false);
            }
            break;
        case cISOTP_ETHCONN::nVerbindung_hergestellt_beschleunigt:
        case cISOTP_ETHCONN::nVerbindungsaufbau_beschleunigt:
            akzeptiere_beschleunigt(true);
            if(check_acceptance())
            {   // verbindung ist aufgebaut
                DATATGR tgr(cISOTP_ETHCONN::nVerbindung_hergestellt_beschleunigt);
                writeTopoData(tgr);
            } else
            {
                wlength=iso_make_CR(rfc1006_datagram,true);
            }
            break;
        case cISOTP_ETHCONN::nAkzeptiere_beschleunigt:
            // vorgabe durch tusimdevcomm
            akzeptiere_beschleunigt(true);
            break;
        case cISOTP_ETHCONN::nAkzeptiere_beschleunigt_nicht:
            // vorgabe durch tusimdevcomm
            akzeptiere_beschleunigt(false);
            break;
        default:
            cTOPOLOGIE::error("cTOPOLOGIE> *** warning *** unknown ISO-TP command CO=%d\n",tgr.cmd);
        }
        if(wlength>=eOK)
        {   // Das Datagramm senden.
            senddata(&rfc1006_datagram,wlength);
        }
    return 0;
}

int cTCP_ETHCONN::receiveData(DATATGR &tgr)
{
    // da senddata auch nach akzeptierten Verbindungen sendet, hier schon aufbauen
    if(isDynamic())
    {   // Verbindungsaufnahme zu beliebiger Adresse.
        //
        // Hinweis zur Fehlersuche :-)
        //    es kann nur eine Verbindung von Topologie aus aufgebaut werden.
        //    weitere werden ignoriert.
        //    nach einem Abbau ist ein Aufbau zu einer anderen Adresse moeglich
        //
        setDynamicAddress(tgr.partnerAddress,tgr.partnerSubAddr);
    }
    switch(tgr.cmd)
    {
    case cTCP_ETHCONN::nBroadcast: case cTCP_ETHCONN::nUnicast:
        senddata(tgr.data,tgr.getDataLen());
        break;
    case cTCP_ETHCONN::nVerbindung_beendet:
    case cTCP_ETHCONN::nVerbindungsabbau:
        cTOPOLOGIE::logVerbose("cTOPOLOGIE> TCP command CO=Verbindung_abgebaut from %s\n",topologie.getModuleName().c_str());
        abbau();
        break;
    case cTCP_ETHCONN::nVerbindung_hergestellt:
        cTOPOLOGIE::error("cTOPOLOGIE> TCP command CO=Verbindung_hergestellt from %s\n",topologie.getModuleName().c_str());
        if(isAutoconnecting())
        {
            if(check_acceptance()  || (isConnected()))
            { // nix tun
            } else
            {   // verbindung aufbauen
                connect();
            }
        }
        break;
    case cTCP_ETHCONN::nVerbindungsaufbau:
        cTOPOLOGIE::error("cTOPOLOGIE> TCP command CO=nVerbindungsaufbau from %s\n",topologie.getModuleName().c_str());
        if(check_acceptance() || (isConnected()))
        {   // nix tun. verbindung ist bereits aufgebaut und gemeldet
        } else
        {   // verbindung aufbauen
            connect();
        }
        break;
    default:;
        cTOPOLOGIE::error("cTOPOLOGIE> *** warning *** unknown TCP command CO=%d\n",tgr.cmd);
    }
    return 0;
}

int cUDP_ETHCONN::receiveData(DATATGR &tgr)
{
    senddata(tgr.data,tgr.getDataLen());
    return 0;
}


// ================================================================================


//void cTOPOLOGIE::printd(char *frmt, sockaddr_in& addr, int len, unsigned char *buffer)
//{
//    fprintf(stderr,frmt,inet_ntoa(addr.sin_addr),htons(addr.sin_port),len);
//    for(int i=0; i<len;i++)
//    {
//        fprintf(stderr,"%02X ",buffer[i]);
//    }
//    fprintf(stderr,"\n");
//}

ISORECEIVE::ISORECEIVE(int p_fd,sockaddr_in p_addr, /*char *p_rcv_buffer, */cTCP_ETHSERVER* p_pServer)
    : len_received(0)
//    , rcv_buffer(p_rcv_buffer)
    , len_to_receive(sizeof(rfc1006_datagram_header))
    , fd(p_fd)
    , iso_verbindung_hergestellt(false)
    , addr(p_addr)
    , pServer(p_pServer)
    , isoData()
{
}

// ================================================================================


int cETH_ACCEPTANCE::sendBuffered(int fd, char *pBuffer, int len, int flags)
{
    // sollten noch ungesendete telegramme in der queue liegen, diese erst abarbeiten. Sonst kommt es zu vertauschter sende Reihenfolge
    if(bufQueue.size())
    {   // da liegen noch daten in der queue, diese muessten zuerst gesendet werden.
        bufQueue.push(new BufferElem((char*)pBuffer,len)); // hinten dran h?ngen
        cTOPMANLOG::log("cETH_ACCEPTANCE> %d telegrams buffered, not sent to %s:%d rc=%d\n",bufQueue.size(), inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
        doWriteEvent();
        return len; // weiter mit n?chstem write event oder topologie telegramm
    }

    int rc=send(fd,pBuffer,len,flags); // jetzt noch das eigentliche Telegramm.
    if(( rc<=0) && (geterrno() == EINPROGRESS))
    {
        bufQueue.push(new BufferElem((char*)pBuffer,len));
        cTOPMANLOG::log("cETH_ACCEPTANCE> %d telegrams buffered, not sent to %s:%d rc=%d\n",bufQueue.size(), inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
        return len;
    } else
    {
        if(rc>0)
        {
            cTOPOLOGIE::trace("cETH_ACCEPTANCE> sent to %s:%d socket %d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),getFD());
            cTOPOLOGIE::dump((const char*)pBuffer,len);
        }
        return rc;
    }

}

void cETH_ACCEPTANCE::doWriteEvent()
{
    while(bufQueue.size())
    {   // queue leerlesen
        BufferElem *bElem=bufQueue.front();
        int ret=send(getFD(),bElem->buffer,bElem->len,0);
        if(ret<=0)
        {
            if(geterrno() == EINPROGRESS)
            {
                return; // damit es weiter geht
            }
            cTOPMANLOG::error("cETH_ACCEPTANCE> ***warning *** not sent to %s:%d rc=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
        } else
        {
            cTOPMANLOG::log("cETH_ACCEPTANCE> from %d buffered, sent to %s:%d\n",bufQueue.size(), inet_ntoa(addr.sin_addr),htons(addr.sin_port));
            cTOPOLOGIE::dump((const char*)bElem->buffer,bElem->len);
        }
        delete bElem;
        bufQueue.pop();
    }
}



void cETHCONNECTBASE::writeObserver(DATATGR &tgr)
{
  topologie.writeObserver(tgr);
}

cETHCONNECTBASE::cETHCONNECTBASE(cIP &ip, cTOPOLOGIE &top)
    : cCONNECTBASE(ip.address_and_port(),"rw")
    , remoteIp(ip)
    , topologie(top)
{
}


// ================================================================================
cTCP_ETHCONN::cTCP_ETHCONN(cIP &ip, cIP &remoteIp, cTOPOLOGIE &ethernet, bool _autoconnecting)
    : cETHCONNECTBASE(remoteIp, ethernet)
    , localIp(ip)
    , autoconnecting(_autoconnecting)
{
    memset(&dynamic_addr,0,sizeof(sockaddr_in));
    conSndState=C_CLOSED;
    setValid();
}

void cTCP_ETHCONN::connect()
{
    int fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
    {
        cTOPMANLOG::error("cTCP_ETHCONN> *** warning *** connect: socket ERROR,rc=%d\n",geterrno());
        return;
    }
    setFD(fd);
    makeAsynchron(getFD());
    conSndState=C_CONNECTING;
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventBufferEmpty); // socket wird asynchron
    if(dynamic_addr.sin_addr.S_un.S_addr) memcpy(&addr,&dynamic_addr,sizeof(sockaddr_in));
    if(::connect(getFD(), (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
    {
        int err=geterrno();
        if(EINPROGRESS!=err)
        {
            cTOPMANLOG::error("cTCP_ETHCONN> *** warning *** connect: connect ERROR,rc=%d\n",err);
            abbau();
        }
    }
}

// alle verbindungen zum abbauen veranlassen
void cTCP_ETHCONN::abbau()
{
    // Abbau muss gemeldet werden wenn irgendeine Verbindung aktuell aufgebaut ist.
    bool abbauAnTopologieMelden=false;
//    typedef cETH_ACCEPTANCE * cETH_ACCEPTANCE_PTR;
//    cETH_ACCEPTANCE **aList=new cETH_ACCEPTANCE_PTR[acceptance.size()];
//    int anzahl=0;
    for(map<UINT64,cETH_ACCEPTANCE*>::iterator iAcc=acceptance.begin(); iAcc!=acceptance.end(); iAcc++)
    {
        if(iAcc->second)
        {
//            aList[anzahl++]=iAcc->second;
//        }
          abbauAnTopologieMelden=true; // Abbau wegen aufgebauter acceptance
          delete iAcc->second; // kann man auch gleich hier entfernen
        }
//    }
//    while(anzahl--)
//    {
//        cTOPMANLOG::log("cTCP_ETHCONN> del: %d from  %s disconnect\n",aList[anzahl]->getPort(), remoteIp.address_and_port().c_str());
//        delete aList[anzahl];
    }
    acceptance.clear();
    if(conSndState!=C_CLOSED)
    {
        cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventDataWaiting|CEventInterface::nEventBufferEmpty);
        cTOPMANLOG::log("cTCP_ETHCONN> %s disconnect\n",remoteIp.address_and_port().c_str());
        conSndState=C_CLOSED;
        closesocket(getFD());
        abbauAnTopologieMelden=true; // Abbau wegen aufgebauter connection
    }
    if(abbauAnTopologieMelden && isRunning())
    {
        // abbautelegramm beantworten

        DATATGR tgr(nVerbindung_beendet);
        if(isDynamic())
        {
            tgr.partnerAddress.value=addr.sin_addr.S_un.S_addr;
            tgr.partnerSubAddr=htons(addr.sin_port);
        }
        writeTopoData(tgr);
    }
}

void cTCP_ETHCONN::doWriteEvent()
{
    cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventBufferEmpty);
    int socketError = -1;
    socklen_t socketErrorLength = sizeof(socketError);
    getsockopt(getFD(),SOL_SOCKET,SO_ERROR,(char *)&socketError,&socketErrorLength);
    if (socketError != 0)
    {
        // connect fehlgeschlagen. wieder zur Ruhe setzen
        cTOPMANLOG::error("cTCP_ETHCONN> %s connect failed rc=%d\n",remoteIp.address_and_port().c_str(), geterrno());
        conSndState=C_CLOSED;
        closesocket(getFD());
        while(bufQueue.size())
        {   // queue leerlesen
            delete bufQueue.front();
            bufQueue.pop();
        }
        DATATGR tgr(nVerbindung_beendet);
        if(isDynamic())
        {
            tgr.partnerAddress.value=addr.sin_addr.S_un.S_addr;
            tgr.partnerSubAddr=htons(addr.sin_port);
        }
        writeTopoData(tgr);
    }
    else
    {
        // nur noch auf empfangsdaten warten
        if(isDynamic())
        {
            cTOPMANLOG::log("cTCP_ETHCONN> %s:%d connected\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
        } else
        {
            cTOPMANLOG::log("cTCP_ETHCONN> %s connected\n",remoteIp.address_and_port().c_str());
        }
        cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventDataWaiting);
        conSndState=C_CONNECTED;
        while(bufQueue.size())
        {   // queue leerlesen
            BufferElem *bElem=bufQueue.front();
            senddata(bElem->buffer,bElem->len);
            delete bElem;
            bufQueue.pop();
        }
        DATATGR tgr(nVerbindung_hergestellt);
        if(isDynamic())
        {
            tgr.partnerAddress.value=addr.sin_addr.S_un.S_addr;
            tgr.partnerSubAddr=htons(addr.sin_port);
        }
        writeTopoData(tgr);
    }

}

// ================================================================================
void cTCP_ETHCONN::doReadEvent()
{
    DATATGR tgr(nUnicast);
    // Telegramm von jedem Partner akzeptieren
    int data_len=recv(getFD(), PCHAR(tgr.data),sizeof(tgr.data),0);
    if(data_len<1)
    {
        // socket wurde geschlossen. nicht mehr warten
        abbau();
    }
    else
    {
        tgr.setDataLen(data_len);
        cTOPOLOGIE::trace("cTCP_ETHCONN> received from %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
        cTOPOLOGIE::dump(tgr);
        topologie.writeObserver(tgr);
        if(isDynamic())
        {
            tgr.partnerAddress.value =  addr.sin_addr.S_un.S_addr;
            tgr.partnerSubAddr = htons(addr.sin_port);
        }
        writeTopoData(tgr);
    }
}
void cTCP_ETHCONN::senddata(const void* pBuffer,size_t len)
{
    if(conSndState==C_CONNECTED)
    {
        int rc=send(getFD(),(char*)pBuffer,len,0);
        if(rc<=0)
        {
            cTOPMANLOG::log("cTCP_ETHCONN> *** warning *** not sent to %s:%d rc=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
        } else
        {
            cTOPOLOGIE::trace("cTCP_ETHCONN> sent to %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
            cTOPOLOGIE::dump((const char*)pBuffer,len);
        }
    } else
    {   // nur wenn keine selbst aufgebaute Verbindung besteht!!!
        if(isDynamic())
        {   // geziel an adresse senden
            UINT64 hash=(UINT64(htons(dynamic_addr.sin_port))<<32) + (UINT64(dynamic_addr.sin_addr.S_un.S_addr));
            cETH_ACCEPTANCE *pAcc=acceptance[hash];
            if(pAcc)
            {
                pAcc->senddata(pBuffer,len);
            } else
            {
                cTOPMANLOG::log("cTCP_ETHCONN> *** warning *** ACCEPTANCE for %s:%d not found\n",inet_ntoa(dynamic_addr.sin_addr),htons(dynamic_addr.sin_port));
            }

        } else
        {   // alle werden bedient (auch wenn es keinen tieferen Sinn hat)
            for(map<UINT64,cETH_ACCEPTANCE*>::iterator iAcc=acceptance.begin(); iAcc!=acceptance.end(); iAcc++)
            {
                iAcc->second->senddata(pBuffer,len);
            }
        }
    }
    if(conSndState==C_CONNECTING)
    {   // verbindungsaufbau l?uft bereits. merken
        bufQueue.push(new BufferElem((char*)pBuffer,len));
    }
}


// ================================================================================

cUDP_ETHCONN::cUDP_ETHCONN(cIP &ip, cIP &remoteIp, cTOPOLOGIE &top)
    : cETHCONNECTBASE(remoteIp, top)
    , localIp(ip)
{
    setValid();

    memcpy(&altAddr, &addr, sizeof(sockaddr_in));
    cTOPMANLOG::log("cUDP_ETHCONN> initialize dest addr to %s:%d\n",inet_ntoa(altAddr.sin_addr), htons(altAddr.sin_port));
}

//=========================================================================================
void cUDP_ETHCONN::senddata(const void* pBuffer,size_t len)
{
    int rc=0;
    if(isDynamic())
    {
        sockaddr_in dynamic_addr={};
        dynamic_addr.sin_family=AF_INET;
        dynamic_addr.sin_addr.S_un.S_addr=PDWORD(pBuffer)[0];
        dynamic_addr.sin_port=htons(PWORD(pBuffer)[2]);
        rc=sendto(getFD(),(char*)&PBYTE(pBuffer)[6],len-6,0,(sockaddr*)&dynamic_addr,sizeof(dynamic_addr));
    } else
    {
        rc=sendto(getFD(),PCHAR(pBuffer),len,0,PSOCKADDR(&altAddr),sizeof(altAddr));
    }
    if(rc<=0)
    {
        cTOPOLOGIE::log("cUDP_ETHCONN> *** warning *** not sent to %s:%d rc=%d\n",inet_ntoa(altAddr.sin_addr),htons(altAddr.sin_port),geterrno());
    } else
    {
        cTOPOLOGIE::trace("cUDP_ETHCONN> sent to %s:%d\n",inet_ntoa(altAddr.sin_addr),htons(altAddr.sin_port));
        cTOPOLOGIE::dump((const char*)pBuffer,len);
    }
}


rfc1006_datagram_t *ISORECEIVE::telCompleted()
{
    if(len_received<len_to_receive)
    {
        return NULLPTR;
    }
    len_received=0;
    len_to_receive=sizeof(rfc1006_datagram_header);
    return &isoData;
}

/*========================================================================================================== */

int ISORECEIVE::receive()
{
    if(len_received<len_to_receive)
    {   // es ist noch was einzulesen
//        int data_len=recv(fd,rcv_buffer+len_received,len_to_receive-len_received,0);
        int data_len=recv(fd,PCHAR(&isoData)+len_received,len_to_receive-len_received,0);
        if(data_len<1)
        {
            len_received=0;
            len_to_receive=sizeof(struct rfc1006_datagram_header);
            return eERR;
        }
        cTOPMANLOG::trace("ISORECEIVE> received from %s:%d, socket %d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),fd);
        cTOPOLOGIE::dump(PCHAR(&isoData)+len_received,data_len);
        len_received+=data_len;
        if(len_received == sizeof(rfc1006_datagram_header))
        {   // es ist noch immer was einzulesen. Längeninfo anpassen
            len_to_receive = ntohs( isoData.user_data.header.length );
            return eOK;
        }
        if(len_received<len_to_receive)
        {
            return eOK;
        }
    }
    return eOK;
}
/*========================================================================================================== */
void cISOTP_ETHCONN::iso_received(rfc1006_datagram_t &isoData)
{
    // das Telegramm ist vollstaendig zusammengesammelt
    switch(isoData.connection_confirmation.CO)
    {
    case cTPDU_CC:
        {    // Connection confirm
            DATATGR tgr(cISOTP_ETHCONN::nVerbindung_hergestellt);
            expedited_eingeschaltet=false;

            BYTE *parameters=isoData.connection_request.parameters;
            int dataLen=htons(isoData.connection_request.header.length)-sizeof(rfc1006_datagram_header) - 7;
            while(dataLen>0)
            {
                switch(parameters[0])
                {
                case cParamCode_AddOptionSel:
                    if(parameters[2]==cParamValue_AosExpedited)
                    {
                        tgr.cmd=cISOTP_ETHCONN::nVerbindung_hergestellt_beschleunigt;
                        expedited_eingeschaltet=true;
                    }
                    /*FALLTHROUGH*/
                case cParamCode_TPDUSIZE:
                case cParamCode_SrcTSAP:
                case cParamCode_DstTSAP:
                    dataLen-=2+parameters[1];
                    parameters+=2+parameters[1];
                    break;
                default:
                    dataLen=0;
                }
            }
            writeTopoData(tgr);
            cTOPMANLOG::trace("cISOTP_ETHCONN> cmd CO=cTPDU_CC expedited=%d\n",expedited_eingeschaltet);
        }   break;
    case cTPDU_CR:
        // Connection request nicht f?r connection
        cTOPMANLOG::trace("cISOTP_ETHCONN> cmd CO=cTPDU_CR ignored\n");
        break;
    case cTPDU_DT:
        {   // datentelegramm
            int wlength=0;
            if(isRunning())
            {   // Verbindung über Topologie ist aufgebaut
                int len = ntohs( isoData.user_data.header.length ) - sizeof(rfc1006_datagram_header) - sizeof(rfc1006_user_datagram_subheader);
                if(!len)
                {
                    break; // kurzquittung
                }
                DATATGR tgr(cISOTP_ETHCONN::nUnicast);
                tgr.setData(isoData.user_data.data,len);
                topologie.writeObserver(tgr);
                writeTopoData(tgr);
                if(cTOPOLOGIE::emptyMessagesForbidden()) return;
                wlength=iso_make_DT0(isoData,false); // kurzquittung aufbauen
            } else
            {
                wlength=iso_make_ER(isoData); // fehlerquittung aufbauen
            }
            senddata(&isoData,wlength);     // und senden
        } break;
    case cTPDU_ED:
        {   // datentelegramm
            int wlength=0;
            if(isRunning())
            {   // Verbindung ?ber Topologie ist aufgebaut
                int len = ntohs( isoData.user_data.header.length ) - sizeof(rfc1006_datagram_header) - sizeof(rfc1006_user_datagram_subheader);
                if(!len)
                {
                    break; // kurzquittung
                }
                DATATGR tgr(cISOTP_ETHCONN::nUnicast_beschleunigt);
                tgr.setData(isoData.user_data.data,len);
                writeObserver(tgr);
                writeTopoData(tgr);
                if(cTOPOLOGIE::emptyMessagesForbidden()) return;
                wlength=iso_make_DT0(isoData,true); // kurzquittung aufbauen
            } else
            {
                wlength=iso_make_ER(isoData); // fehlerquittung aufbauen
            }
            senddata(&isoData,wlength);     // und senden
        } break;
    case cTPDU_ER:
    {
        // negative quittung
        DATATGR tgr(cISOTP_ETHCONN::nVerbindung_beendet);
        writeTopoData(tgr);
        cTOPMANLOG::trace("cISOTP_ETHCONN> cmd CO = cTPDU_ER\n");
    }   break;
    default:
        // error
        cTOPMANLOG::trace("cISOTP_ETHCONN> *** warning *** unknown cmd CO=%d\n",isoData.user_data.subheader.CO);
    }
}
void cETH_ACCEPTANCE::iso_received(rfc1006_datagram_t &isoData)
{
    // das Telegramm ist vollstaendig zusammengesammelt
    switch(isoData.connection_confirmation.CO)
    {
    case cTPDU_CC:
        // Connection confirm nicht f?r acceptance
        break;
    case cTPDU_CR:
        {   // Connection request
            char *sap_string=(char*)(isoData.connection_request.parameters+5);
            int sap_len=int(sap_string[-1]);
            string calling_sap(sap_string,sap_len);

            sap_string+=(sap_len+2);
            sap_len=int(sap_string[-1]);
            string called_sap(sap_string,sap_len);

            if(pConnISO=pServer->getConnISO(called_sap,calling_sap))
            {   // cr wird nach accept als erstes telegramm gesendet
                pServer->setConnISO(pConnISO);
                pConnISO->addAcceptance(this);
                DATATGR tgr(cISOTP_ETHCONN::nVerbindung_hergestellt);

                BYTE *pData=isoData.connection_request.parameters;
                int dataLen=htons(isoData.connection_request.header.length)-sizeof(rfc1006_datagram_header) - 7;
                while(dataLen>0)
                {
                    switch(pData[0])
                    {
                    case cParamCode_AddOptionSel:
                        if(pConnISO->is_beschleunigt_akzeptiert() && pData[2]==cParamValue_AosExpedited)
                        {
                            tgr.cmd = cISOTP_ETHCONN::nVerbindung_hergestellt_beschleunigt;
                        } else
                        {   // nicht weiter auserten. Quittung ohne expedited
                            isoData.connection_request.header.length=ntohs(sizeof(rfc1006_datagram_header) + 7);
                            isoData.connection_request.LI-=3;
                            dataLen=0;
                            break;
                        }
                        /*FALLTHROUGH*/
                    case cParamCode_TPDUSIZE:
                    case cParamCode_SrcTSAP:
                    case cParamCode_DstTSAP:
                        dataLen-=2+pData[1];
                        pData+=2+pData[1];
                        break;
                    default:
                        dataLen=0;
                    }
                }
                cTOPMANLOG::trace("cETH_ACCEPTANCE> cmd received, CO=cTPDU_CR, calling sap=%s called sap=%s socket=%d expedited=%d\n",calling_sap.c_str(),called_sap.c_str(),getFD(), tgr.cmd == cISOTP_ETHCONN::nVerbindung_hergestellt_beschleunigt);

                if(pConnISO->writeTopoData(tgr)==0)
                {
                    int wlength=pConnISO->iso_make_CC(isoData.connection_request.source,isoData); // RFC1006 Quittung (CC) zur?ckgeben
                    if(wlength>=eOK)
                    {
                        senddata(&isoData,wlength);
                    }
                } else
                {
                    cTOPMANLOG::error("cETH_ACCEPTANCE> *** warning *** calling sap=%s called sap=%s NOT CONFIRMED\n",calling_sap.c_str(),called_sap.c_str());
                    delete this;
                }
            } else
            {
                cTOPMANLOG::error("cETH_ACCEPTANCE> cmd received, CO=cTPDU_CR, invalid sap received calling=%s called=%s socket=%d\n",calling_sap.c_str(),called_sap.c_str(),getFD());
                int wlength=cISOTP_ETHCONN::iso_make_ER(isoData); // RFC1006 Error generieren
                senddata(&isoData,wlength); // und senden
            }
        } break;
    case cTPDU_DT:
        {   // datentelegramm
            if(pConnISO && pConnISO->isRunning())
            {   // Verbindung ?ber Topologie ist vorhanden und aufgebaut
                int len = ntohs( isoData.user_data.header.length ) - sizeof(rfc1006_datagram_header) - sizeof(rfc1006_user_datagram_subheader);
                if(!len)
                {   // kurzquittung
                    break;
                }
                DATATGR tgr(cISOTP_ETHCONN::nUnicast);
                tgr.setData(isoData.user_data.data,len);
                pConnISO->writeObserver(tgr);
                pConnISO->writeTopoData(tgr);
                if(cTOPOLOGIE::emptyMessagesForbidden()) return;
                int wlength=cISOTP_ETHCONN::iso_make_DT0(isoData,false); // kurzquittung aufbauen
                senddata(&isoData,wlength);     // und senden
            } else
            {
                cTOPMANLOG::error("cETH_ACCEPTANCE> cmd received, CO=cTPDU_DT, no (running) connection found socket=%d\n",getFD());
                delete this; // existiert die Topologieverbindung nicht wird die aktzepierte Verbindung wieder geschlossen.
                return;
            }
        } break;
    case cTPDU_ED:
        {   // datentelegramm
            if(pConnISO && pConnISO->isRunning())
            {   // Verbindung ?ber Topologie ist vorhanden und aufgebaut
                int len = ntohs( isoData.user_data.header.length ) - sizeof(rfc1006_datagram_header) - sizeof(rfc1006_user_datagram_subheader);
                if(!len)
                {   // kurzquittung
                    break;
                }
                DATATGR tgr(pConnISO->is_beschleunigt_akzeptiert()?cISOTP_ETHCONN::nUnicast_beschleunigt:cISOTP_ETHCONN::nUnicast);
                tgr.setData(isoData.user_data.data,len);
                pConnISO->writeObserver(tgr);
                pConnISO->writeTopoData(tgr);
                if(cTOPOLOGIE::emptyMessagesForbidden()) return;
                int wlength=cISOTP_ETHCONN::iso_make_DT0(isoData,true); // kurzquittung aufbauen
                senddata(&isoData,wlength);     // und senden
            } else
            {
                cTOPMANLOG::error("cETH_ACCEPTANCE> cmd received, CO=cTPDU_ED, no (running) connection found socket=%d\n",getFD());
                delete this; // wg. Gnats#5850: existiert die Topologieverbindung nicht wird die aktzepierte Verbindung
                // wieder geschlossen.
            }
        } break;
    case cTPDU_ER:
        // negative quittung
        if(pConnISO)
        {   // fehler an urspr?nglichen telegrammversender weiterleiten
            DATATGR tgr(cISOTP_ETHCONN::nVerbindung_beendet);
            pConnISO->writeTopoData(tgr);
        }  // else: Fehler vor verbindungsaufbau...
        cTOPMANLOG::error("cETH_ACCEPTANCE> cmd CO = cTPDU_ER\n");
    default:
        // error
        cTOPMANLOG::error("cETH_ACCEPTANCE> *** warning *** unknown cmd CO=%d\n",isoData.user_data.subheader.CO);
    }
}

/*========================================================================================================== */


cETH_ACCEPTANCE::cETH_ACCEPTANCE(int sockServer,cTCP_ETHSERVER* p_pServer)
    : cTCP_ACCEPTBASE(sockServer)
    , pServer(p_pServer)
    , pConn(p_pServer->getConn())
    , pConnISO(NULL)
    , isoreceive(getFD(),addr,p_pServer)
{
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventDataWaiting | CEventInterface::nEventBufferEmpty);
    if(pConn)
    {
        cTOPMANLOG::log("cETH_ACCEPTANCE::cETH_ACCEPTANCE> added to connection for remote address %s:%d\n",inet_ntoa(addr.sin_addr),pServer->getPort());
        pConn->addAcceptance(this);
        setValid();
        if(!pConn->isConnected())
        {   // dieses Ereignis muss gemeldet werden
            DATATGR tgr(cTCP_ETHCONN::nVerbindung_hergestellt);
            tgr.partnerAddress.value=addr.sin_addr.S_un.S_addr;
            tgr.partnerSubAddr=htons(addr.sin_port);
            pConn->writeTopoData(tgr);
        }
    } else
    {
        // pConnISO wird erst durch ersten Connect Request festgelegt
        setValid();
    }
}
cETH_ACCEPTANCE::~cETH_ACCEPTANCE()
{
    if(pConn)
    {
        cTOPMANLOG::log("cETH_ACCEPTANCE::~cETH_ACCEPTANCE> removed from connection for remote address %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
        pConn->removeAcceptance(this);
    } else
    {
        if(pConnISO)
        {
            cTOPMANLOG::log("cETH_ACCEPTANCE::~cETH_ACCEPTANCE> removed from connection for remote address %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
            pConnISO->removeAcceptance(this);
        } else
        {
            cTOPMANLOG::error("cETH_ACCEPTANCE::~cETH_ACCEPTANCE> no connection found for remote address %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
        }
    }

    cTOPMANLOG::log("cETH_ACCEPTANCE::~cETH_ACCEPTANCE> socket %d closed\n",getFD());
    cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventDataWaiting|CEventInterface::nEventBufferEmpty);// getEventMask());
    closesocket(getFD());
}

void cETH_ACCEPTANCE::doReadEvent()
{
    if(pConn)
    {
        DATATGR tgr(cTCP_ETHCONN::nUnicast);
        int data_len=recv(getFD(),PCHAR(tgr.data),sizeof(tgr.data),0); // offset 8 statt 2 um platz f?r dynamische adresse zu lassen
        if(data_len<1)
        {
            tgr.cmd = cTCP_ETHCONN::nVerbindung_beendet;
            cTOPOLOGIE::logVerbose("cETH_ACCEPTANCE> send partner not reachable\n");
            if(pConn->isDynamic())
            {
                tgr.partnerAddress.value = addr.sin_addr.S_un.S_addr;
                tgr.partnerSubAddr = htons(addr.sin_port);
            }
            pConn->writeTopoData(tgr);
            delete this;
        }
        else
        {
            tgr.setDataLen(data_len);
            cTOPMANLOG::trace("cETH_ACCEPTANCE> received from %s:%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
            cTOPOLOGIE::dump(tgr);
            pConn->writeObserver(tgr);
            if(pConn->isDynamic())
            {
                tgr.partnerAddress.value = addr.sin_addr.S_un.S_addr;
                tgr.partnerSubAddr = htons(addr.sin_port);
            }
            pConn->writeTopoData(tgr);
        }
    } else
    {   // ist eine ISO-Verbindung
        // pConnISO ist vor dem CR noch nicht gesetzt
        if(isoreceive.receive()==eERR)
        {   // socket wurde geschlossen. nicht mehr warten. Partner informieren
            if(pConnISO)
            {   // verbindung war bereits aufgebaut
                DATATGR tgr(cTCP_ETHCONN::nVerbindung_beendet);
                pConnISO->writeTopoData(tgr);
            }
            delete this;
            return;
        }
        if(rfc1006_datagram_t *isoTel=isoreceive.telCompleted())
        {
            iso_received(*isoTel);
        }
    }
}

void cETH_ACCEPTANCE::senddata(const void* pBuffer,size_t len)
{
    int rc=sendBuffered(getFD(),(char*)pBuffer,len,0);

    if(rc<=0)
    {
        cTOPMANLOG::error("cETH_ACCEPTANCE> *** warning *** not sent to %s:%d socket %d rc=%d\n",
                    inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno(),getFD());
    }
}

cTCP_ETHSERVER::cTCP_ETHSERVER(cIP &ip, cTCP_ETHCONN *p_pConn)
    : cTCP_SERVERBASE(ip.portnum(),ip.address().c_str())
    , pConn(p_pConn)
{
    if(getFD()==0) return;
    if(WSAGetLastError()!=0) return;
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventDataWaiting);
    setValid();
}

cISOTP_ETHCONN *cTCP_ETHSERVER::getConnISO(const string &calling, const string &called)
{
    // verbindung aus dem mapping "pConnISO_map" suchen
    return pConnISO_map[calling+called];
}
void cTCP_ETHSERVER::setConnISO(cISOTP_ETHCONN *pConnISO)
{
    pConnISO_map[pConnISO->sap_kennung()]=pConnISO;

}


// f?r eine akzeptierte Verbindung wird ein neues "cETH_ACCEPTANCE" Objekt erzeugt
void cTCP_ETHSERVER::doReadEvent()
{
    cETH_ACCEPTANCE *pAcc=new cETH_ACCEPTANCE(/*pLog, pEventinterface,*/getFD(),this);
    if(!pAcc->valid())
    {   // z.B. ung?ltige Adresse (Aufbauwunsch widerspricht Topologie)
        delete pAcc;
    }
}

// die akzeptierte verbindung einer vorhandenen Verbindung zuordnen
void cTCP_ETHCONN::addAcceptance(cETH_ACCEPTANCE* p_pAccept)
{
    acceptance[p_pAccept->getAddrHash()]=p_pAccept;
}

// die Zuordnung der akzeptierten verbindung zur vorhandenen Verbindung aufheben
void cTCP_ETHCONN::removeAcceptance(cETH_ACCEPTANCE* p_pAccept)
{
    acceptance.erase(p_pAccept->getAddrHash());
}
bool cTCP_ETHCONN::check_acceptance()
{
    return (acceptance.size()>0);
}

void cTCP_ETHCONN::setDynamicAddress(IPADDRESS addr, WORD port)
{
    dynamic_addr.sin_family=AF_INET;
    dynamic_addr.sin_addr.S_un.S_addr=addr.value;
    dynamic_addr.sin_port=htons(port);
}

// die akzeptierte verbindung einer vorhandenen Verbindung zuordnen
void cISOTP_ETHCONN::addAcceptance(cETH_ACCEPTANCE* p_pAccept)
{
    for (vector<cETH_ACCEPTANCE*>::iterator it = acceptance.begin();
        it != acceptance.end();
        it++)
    {
        if ((*it) == p_pAccept)
        {
            cTOPMANLOG::trace("cISO_ETHCONN> *** warning *** %s:%d already member of acceptance vector\n",
                        inet_ntoa(addr.sin_addr), htons(addr.sin_port));
            return;
        }
    }
    acceptance.push_back(p_pAccept);
}

// die Zuordnung der akzeptierten verbindung zur vorhandenen Verbindung aufheben
void cISOTP_ETHCONN::removeAcceptance(cETH_ACCEPTANCE* p_pAccept)
{
    for (vector<cETH_ACCEPTANCE*>::iterator it=acceptance.begin();it!=acceptance.end();it++)
    {
        if ((*it)==p_pAccept)
        {
            acceptance.erase(it);
            return;
        }
    }
}



// ================================================================================
// Handler f?r UDP-Verbindungen
// Zust?ndig f?r den Empfang von Telegrammen
//
cUDP_ETHHANDLER::cUDP_ETHHANDLER(cIP &ip, cUDP_ETHCONN &p_pConn)
    : cUDP_HANDLERBASE(ip.portnum(),ip.address().c_str())
    , pConn(p_pConn)
{
    pConn.setFD(getFD());
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventDataWaiting);
    setValid();
}

// alle Empfangsereignisse des UDP-Handlers werden hier behandelt
void cUDP_ETHHANDLER::doReadEvent()
{
    sockaddr_in rcv_addr={};
    int sockaddr_len=sizeof(sockaddr_in);

    DATATGR tgr(cUDP_ETHCONN::nUnicast);
    // Telegramm von jedem Partner akzeptieren
    DWORD rcverror = 0;
    int data_len=recvfrom(getFD(),PCHAR(tgr.data),sizeof(tgr.data),0,(sockaddr*)&rcv_addr,(socklen_t *)&sockaddr_len);
    /* Im Fehlerfall sofort errno abfragen. */
    if (SOCKET_ERROR == data_len)
    {
        rcverror = geterrno();
#ifdef _WINDOWS

        /**
         * CFX00193652 TopETH: Gesendetes UDP Telegramm wird auf eigenem Port wieder empfangen
         * see http://stackoverflow.com/questions/14681378/wsasend-for-udp-socket-triggers-fd-read-when-no-destination-available
         *
         * If an UDP datagram gets dropped by a router (host not found) or
         * rejected by a server (socket closed); an ICMP message is sent
         * back to inform about the failure. This generates a read
         * event. Since there is no actual user data, underlying stack
         * translates ICMP message and provides you an appropriate error
         * message via WSARecv() return code. The error WSAECONNRESET is
         * provieded in this case.
         */
        if (rcverror == WSAECONNRESET)
            cTOPMANLOG::error("cUDP_ETHHANDLER> *** warning *** %s:%d has been sent to unreachable destination\n",
                        inet_ntoa(rcv_addr.sin_addr), htons(addr.sin_port));
        else
            cTOPMANLOG::trace("cUDP_ETHHANDLER> *** warning *** %s:%d socket=%d rc=%d\n",
                        inet_ntoa(rcv_addr.sin_addr), htons(addr.sin_port),
                        getFD(), rcverror);
#else
        cTOPOLOGIE::trace("cUDP_ETHHANDLER> received 0 from %s:%d socket=%d rc=%d\n",
                    inet_ntoa(rcv_addr.sin_addr), htons(addr.sin_port),
                    getFD(), rcverror);
#endif
    }
    else
    {
        tgr.setDataLen(data_len);
        cTOPMANLOG::trace("cUDP_ETHHANDLER> received %d from %s:%d\n",data_len,inet_ntoa(rcv_addr.sin_addr),htons(rcv_addr.sin_port));
        cTOPOLOGIE::dump(tgr);

        if(pConn.isDynamic())
        {
                tgr.partnerAddress.value = rcv_addr.sin_addr.S_un.S_addr;
                tgr.partnerSubAddr = htons(rcv_addr.sin_port);
        }
        pConn.altAddr= sockaddr_in(rcv_addr);
    }

    if(data_len<1)
    {
        // weggeloescht, weil ein close beim partner ein 0 byte langes telegramm generiert wird, dieses aber nicht den Handler
        // beenden darf.
    }
    else
    {
        // weiterleiten auf die zugeordnete Verbindung
        pConn.writeObserver(tgr);
        pConn.writeTopoData(tgr);
    }
}

// ================================================================================
// Konstruktor f?r ISO-TP - Verbindungen
//
// zusätzlich zu TCP müssen jeweils 5stellige Kennungen für local bzw. remote SAP (Port) verwaltet werden
// 09.06.2021: local und remote wurden in der Bedeutung vertaucht
//
cISOTP_ETHCONN::cISOTP_ETHCONN(cIP &localIp, cIP &remoteIp, cTOPOLOGIE &ethernet, const char *pLocal_tsap, const char *pRemote_tsap)
    : cETHCONNECTBASE(remoteIp, ethernet)
    , remote_tsap(pRemote_tsap) // 09.05.2021, nicht mehr getauscht // für CR getauscht
    , local_tsap(pLocal_tsap) // 09.05.2021, nicht mehr getauscht// für CR getauscht
    , isoreceive(0,addr/*,rcv_buffer*/) // handle noch unbekannt
    , isoLocalIp(localIp) // 09.05.2021, nicht mehr getauscht// für CR getauscht
    , isoRemoteIp(remoteIp) // 09.05.2021, nicht mehr getauscht// für CR getauscht
    , expedited_erlaubt(false)
    , expedited_eingeschaltet(false)
{
    conSndState=C_CLOSED;
    setValid();
}

// Konnektierung des Remote-Rechners einleiten
void cISOTP_ETHCONN::connect()
{
    int fd=socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
    {
        cTOPMANLOG::error("cISOTP_ETHCONN> *** warning *** connect: socket ERROR,rc=%d\n",geterrno());
        return;
    }
    isoreceive.setFD(fd);
    setFD(fd);

    sockaddr_in addrBind = { AF_INET };
    addrBind.sin_addr.S_un.S_addr=inet_addr(isoLocalIp.address().c_str());
    // eigene Adresse binden (lokalip mit port 0
    int ret=::bind(getFD(),(sockaddr*)&addrBind, sizeof(sockaddr));
    ret=geterrno();

    makeAsynchron(getFD());
    conSndState=C_CONNECTING;
    cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventBufferEmpty); // socket wird asynchron
    if(::connect(getFD(), (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
    {
        int err=geterrno();
        if(EINPROGRESS!=err)
        {
            cTOPMANLOG::error("cISOTP_ETHCONN> *** warning *** connect ERROR,rc=%d\n",err);
        }
    }
}
bool cISOTP_ETHCONN::check_acceptance()
{
    return (acceptance.size()>0);
}

// Ein Connection Request Datagram nach RFC1006 erzeugen.
int cISOTP_ETHCONN::iso_make_CR(rfc1006_datagram_t &datagram, bool expedited)
{
    memset(&datagram,0,sizeof(rfc1006_datagram_t));

    datagram.connection_request.header.version  = 3;
    datagram.connection_request.header.reserved = 0;
    datagram.connection_request.CO              = cTPDU_CR;
    datagram.connection_request.destination     = 0;
    datagram.connection_request.source          = 0x0100;
    datagram.connection_request.CL              = 0;

    int i=0;
    datagram.connection_request.parameters[i++] = cParamCode_TPDUSIZE;
    datagram.connection_request.parameters[i++] = 0x01;
    datagram.connection_request.parameters[i++] = 0x0a;

    datagram.connection_request.parameters[i++] = cParamCode_SrcTSAP;
    datagram.connection_request.parameters[i++] = BYTE(local_tsap.size());                 // cParamCode_SrcTSAP-Parameter-length
    memcpy( &datagram.connection_request.parameters[i], local_tsap.c_str(), local_tsap.size() );
    i+= local_tsap.size();

    datagram.connection_request.parameters[i++] = cParamCode_DstTSAP;
    datagram.connection_request.parameters[i++] = BYTE(remote_tsap.size());                // cParamCode_DstTSAP-Parameter-length
    memcpy( &datagram.connection_request.parameters[i], remote_tsap.c_str(), remote_tsap.size() );
    i+= remote_tsap.size();

    if(expedited)
    {
        datagram.connection_request.parameters[i++] = cParamCode_AddOptionSel;
        datagram.connection_request.parameters[i++] = 1;
        datagram.connection_request.parameters[i++] = cParamValue_AosExpedited;
    }
    datagram.connection_request.LI              = i+6;

    // Die Datagrammlaenge ist in Network Byte Order abzulegen.
    int length = sizeof(struct rfc1006_datagram_header) + datagram.connection_request.LI + 1;
    datagram.connection_request.header.length = htons(length);
    return length;
}

// Abschluss der Konnektierung bzw. Timeout bearbeiten
void cISOTP_ETHCONN::doWriteEvent()
{
    cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventBufferEmpty);
    int socketError = -1;
    socklen_t socketErrorLength = sizeof(socketError);
    getsockopt(getFD(),SOL_SOCKET,SO_ERROR,(char *)&socketError,&socketErrorLength);
    if (socketError != 0)
    {
        // connect fehlgeschlagen. wieder zur Ruhe setzen
        cTOPMANLOG::error("cISOTP_ETHCONN> *** warning *** %s connect failed\n",isoRemoteIp.address_and_port().c_str());
        conSndState=C_CLOSED;
        closesocket(getFD());
        while(bufQueue.size())
        {   // queue leerlesen
            delete bufQueue.front();
            bufQueue.pop();
        }
        DATATGR tgr(nVerbindung_beendet);
        writeTopoData(tgr);
    }
    else
    {
        cTOPMANLOG::log("cISOTP_ETHCONN> %s:%d socket %d connected\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),getFD());
        cTOPOLOGIE::eventloop()->bitRegisterCallback(this,getFD(),CEventInterface::nEventDataWaiting);
        conSndState=C_CONNECTED;

        // jetzt die gepufferten Daten senden. Das ist z.B das
        // Connection Request Datagram nach RFC1006
        while(bufQueue.size())
        {   // queue leerlesen
            BufferElem *bElem=bufQueue.front();
            int ret=send(getFD(),bElem->buffer,bElem->len,0);
            if(ret<=0)
            {
                cTOPMANLOG::error("cISOTP_ETHCONN> *** warning *** not sent to %s:%d rc=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
            } else
            {
                cTOPMANLOG::trace("cISOTP_ETHCONN> sent to %s:%d socket %d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),getFD());
                cTOPOLOGIE::dump(bElem->buffer,bElem->len);
            }
            delete bElem;
            bufQueue.pop();
        }
    }
}

// ================================================================================
// neue Daten sind verf?gbar und k?nnen vom Socket gelesen werden
// Die Datenl?nge der RF1006 Telegramme ist dabei zu ber?cksichtigen. D.H.
// es muss St?ckweise ausgelesen werden.
void cISOTP_ETHCONN::doReadEvent()
{
    if(isoreceive.receive()==eERR)
    {
        // socket wurde geschlossen. nicht mehr warten
        cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventDataWaiting|CEventInterface::nEventBufferEmpty); // CEventInterface::nEventDataWaiting);
        closesocket(getFD());
        cTOPMANLOG::log("cISOTP_ETHCONN> %s:%d connection closed socket %d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),getFD());
        conSndState=C_CLOSED;
    }

    if(rfc1006_datagram_t *isoTel=isoreceive.telCompleted())
    {
        iso_received(*isoTel);
    }

    return;
}

// Funktion zum senden eines Telegrammes. Dieses Telegramm ist bereits in RFC1006 Format
// konvertiert. Es wird an alle verbundenen Schnittstellen verteilt. D.H an alle
// Verbindungen die aktzepiert wurden. In der Regel solte nur eine solche Verbindung
// existieren.
// Existiert keine solche Verbindung erfolgt die Sendung ?ber die "konnektierte" Verbindung
// Hat sich auch noch keiner konnktiert, so wird das Telegramm zwischengespeichert und
// die Verbindung aufgebaut.
//
// Beispiel: Es trifft von der Topologie ein Verbindungsaufbauwunsch ein. Da noch keine
// Verbindung (z.B. Bedienplatz) vorhanden ist, erfolgt der verbindungsaufbau mittels connect().
// Solbald das Connect-Ereignis (doWriteEvent()) eingetroffen ist, erolfgt die
// Sendung des RFC1006 Verbindungsaufbautelegrammes zum Bedienplatz.
//
void cISOTP_ETHCONN::senddata(const void * pIsoData,size_t len)
{
    bool versendet=false;
    for(auto i:acceptance)
    {
        i->senddata(pIsoData,len);
        versendet = true;
    }
    if(conSndState==C_CONNECTED)
    {
        int rc=send(getFD(),PCHAR(pIsoData),len,0);
        if(rc<=0)
        {
            cTOPMANLOG::trace("cISOTP_ETHCONN> *** warning *** not sent to %s:%d rc=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),geterrno());
        } else
        {
            cTOPMANLOG::trace("cISOTP_ETHCONN> sent to %s:%d socket %d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port),getFD());
            cTOPOLOGIE::dump(PCHAR(pIsoData),len);
        }
        versendet=true;
    }
    if(versendet)
    {
        return; // auftrag erf?llt
    }
    if(conSndState==C_CONNECTING)
    {   // verbindungsaufbau l?uft bereits. merken
        bufQueue.push(new BufferElem(PCHAR(pIsoData),len));
    }
    if(conSndState==C_CLOSED)
    {   // keine Verbindung. merken
        bufQueue.push(new BufferElem(PCHAR(pIsoData),len));
        connect();
    }
}

void cISOTP_ETHCONN::akzeptiere_beschleunigt(bool expedited)
{	// einstellung für den Server
    expedited_erlaubt=expedited;
}
bool cISOTP_ETHCONN::is_beschleunigt_akzeptiert()
{	// einstellung f?r den Server
    return expedited_erlaubt;
}
// Konverierung eines telegrammes von Topologie in RFC1006 und senden
//
int cISOTP_ETHCONN::iso_make_DT(const BYTE *buffer, int length, rfc1006_datagram_t &isoData, bool expedited)
{
    // Ein User Datagram nach RFC1006 erzeugen
    isoData.user_data.header.version   = 3;
    isoData.user_data.header.reserved  = 0;
    isoData.user_data.subheader.LI     = 2;
    isoData.user_data.subheader.CO     = (expedited&&expedited_eingeschaltet)?cTPDU_ED:cTPDU_DT;
    isoData.user_data.subheader.ET     = 0x80;

    // Die Datagrammlaenge ist in Network Byte Order abzulegen.
    unsigned int wlength = sizeof(struct rfc1006_datagram_header) + sizeof(struct rfc1006_user_datagram_subheader) + length;
    isoData.user_data.header.length = htons(wlength);
    // Konsistenzcheck
    if(length > RFC1006_MAX_USER_DATA )
    {
        cTOPMANLOG::error("cISOTP_ETHCONN> *** warning *** send, wrong length %d (max %d allowed)\n",
                    length, RFC1006_MAX_USER_DATA);
        return eERR;
    }
    // Nun die Daten in das Telegram kopieren
    memcpy( &isoData.user_data.data, buffer, length);
    return wlength;
}
// fehlerquittung senden
int cISOTP_ETHCONN::iso_make_ER(rfc1006_datagram_t &isoData)
{
    // Ein User Datagram nach RFC1006 erzeugen
    isoData.user_data.header.version   = 3;
    isoData.user_data.header.reserved  = 0;
    isoData.user_data.subheader.LI     = 2;
    isoData.user_data.subheader.CO     = cTPDU_ER;
    isoData.user_data.subheader.ET     = 0x80;

    unsigned int wlength = sizeof(struct rfc1006_datagram_header) + sizeof(struct rfc1006_user_datagram_subheader);
    isoData.user_data.header.length = htons(wlength);
    return wlength;
}

// Kurzquittung senden (Nutzdatenl?nge 0)
int cISOTP_ETHCONN::iso_make_DT0(rfc1006_datagram_t &datagram, bool expedited)
{
    // Ein User Datagram nach RFC1006 erzeugen
    datagram.user_data.header.version   = 3;
    datagram.user_data.header.reserved  = 0;
    datagram.user_data.subheader.LI     = 2;
    datagram.user_data.subheader.CO     = expedited?cTPDU_ED:cTPDU_DT;
    datagram.user_data.subheader.ET     = 0x80;

    unsigned int wlength = sizeof(struct rfc1006_datagram_header) + sizeof(struct rfc1006_user_datagram_subheader);
    datagram.user_data.header.length = htons(wlength);
    return wlength;
}

// meldung verbindung beendet
void cISOTP_ETHCONN::iso_beendet()
{
    cTOPMANLOG::log("cISOTP_ETHCONN> beendet...\n");
}

// quittung verbindung hergestellt weiterleiten
int cISOTP_ETHCONN::iso_make_CC(int source, rfc1006_datagram_t &isoData)
{
    isoData.connection_request.header.version  = 3;
    isoData.connection_request.header.reserved = 0;
    isoData.connection_request.CO              = cTPDU_CC;
    isoData.connection_request.destination     = source;
    isoData.connection_request.source          = 0x4312;
    BYTE *pData=isoData.connection_request.parameters;
    int dataLen=htons(isoData.connection_request.header.length)-sizeof(rfc1006_datagram_header) - 7;
    while(dataLen>0)
    {
        switch(pData[0])
        {
        case cParamCode_AddOptionSel:
            expedited_eingeschaltet = expedited_erlaubt && (pData[2]==cParamValue_AosExpedited);
            /*FALLTHROUGH*/
        case cParamCode_TPDUSIZE:
        case cParamCode_SrcTSAP:
        case cParamCode_DstTSAP:
            dataLen-=2+pData[1];
            pData+=2+pData[1];
            break;
        default:
            dataLen=0;
        }
    }
    // Die Datagrammlaenge ist in Network Byte Order abzulegen.
    int length = sizeof(rfc1006_datagram_header) + isoData.connection_request.LI + 1;
    isoData.connection_request.header.length = htons(length);
    return length;
}

// alle verbindungen zum abbauen veranlassen
void cISOTP_ETHCONN::iso_abbau()
{
    for(auto i:acceptance)
    {
        delete i;
    }
    cTOPOLOGIE::eventloop()->bitUnregisterCallback(this,getFD(),CEventInterface::nEventDataWaiting|CEventInterface::nEventBufferEmpty);
    cTOPMANLOG::log("cISOTP_ETHCONN> %s disconnect\n",isoRemoteIp.address_and_port().c_str());
    conSndState=C_CLOSED;
    closesocket(getFD());
    // abbautelegramm beantworten
    DATATGR tgr(nVerbindung_beendet);
    writeTopoData(tgr);
}

// =eof=
