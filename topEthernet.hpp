#ifndef TOP_ETHERNET_HPP
#define TOP_ETHERNET_HPP
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

#include "Topologie.hpp"
#include "rfc1006_private.h"
#include <TUTgrHeader.h>

#include <EthernetTgrDefinitions.h>

using namespace std;


#ifdef _WINDOWS

#ifdef EINPROGRESS
#undef EINPROGRESS
#endif
#define EINPROGRESS WSAEWOULDBLOCK
#define socklen_t int

inline void makeAsynchron(int) {}
inline DWORD geterrno(void) { return GetLastError(); }

// --------------------------------------------------------------------------------
#else // LINUX:

#define INVALID_SOCKET (int)(~0)
#define SOCKET_ERROR -1
#define SO_MAX_MSG_SIZE 1500

inline void makeAsynchron(int sock)
{
    int flags=fcntl(sock,F_GETFL,0);
    fcntl(sock,F_SETFL,O_NONBLOCK|flags);
}

inline void closesocket(int sock)
{
    shutdown(sock,SHUT_RDWR);
    close(sock);
}

inline int geterrno(void) { return errno; }

#endif

// ================================================================================


class cOBSERVER;
class cETHCONNECTBASE;
class TopoDataReceive;

#define VA(fmt,tmp)\
    char tmp[1024];\
    va_list args;\
    va_start(args, fmt);\
    vsprintf(tmp,fmt,args);\
    va_end(args);



class cTCP_ETHSERVER;
class cTOPOLOGIE;
class cTCP_ETHCONN;
class cISOTP_ETHCONN;

class ISORECEIVE
{
public:
    ISORECEIVE(int p_fd,sockaddr_in p_addr, cTCP_ETHSERVER* p_pServer=NULL);
    ~ISORECEIVE(){}
    int receive();
    void setFD(int p_fd){fd=p_fd;}
    rfc1006_datagram_t *telCompleted();
private:
    cTCP_ETHSERVER* pServer;
    int fd;
    bool iso_verbindung_hergestellt;
    sockaddr_in addr;
    int len_received;
    int len_to_receive;
    rfc1006_datagram_t isoData;
};

// ================================================================================
/**
* Stellt eine angenommene TCP-Verbindung dar.
*/
class cETH_ACCEPTANCE
    : public cTCP_ACCEPTBASE
{
private:
    cTCP_ETHCONN *pConn;
    cISOTP_ETHCONN *pConnISO;
    cTCP_ETHSERVER *pServer;
//    char rcv_buffer[506];
    ISORECEIVE isoreceive;

    class BufferElem
    {
    public:
        char *buffer;
        int len;
        BufferElem(char *p_buffer,int p_len) : len(p_len)
        {
            buffer=new char[len];
            memcpy(buffer,p_buffer,len);
        }
        ~BufferElem(){ delete[] buffer;}
    };
    queue<BufferElem*> bufQueue;

    int sendBuffered(int fd, char *pBuffer, int len, int flags);

    // prevent making copies of cTCP_ACCEPTANCE:
    cETH_ACCEPTANCE(const cTCP_ACCEPTANCE&);
    cETH_ACCEPTANCE& operator=(const cTCP_ACCEPTANCE&);
public:
    cETH_ACCEPTANCE(int sockServer,cTCP_ETHSERVER* p_pServer);
    inline UINT64 getAddrHash(){ return (UINT64(htons(addr.sin_port))<<32)+ UINT64(addr.sin_addr.S_un.S_addr); }
    virtual ~cETH_ACCEPTANCE();

    virtual void doReadEvent();
    virtual void doWriteEvent();
    void iso_received(rfc1006_datagram_t &isoData);
    void senddata(const void* pBuffer,size_t len);
};

// ================================================================================

class cTCP_ETHSERVER
    : public cTCP_SERVERBASE
{
private:
    cTCP_ETHCONN *pConn;
    map<string,cISOTP_ETHCONN*> pConnISO_map;
    static map<string,cTCP_ETHSERVER*> pServer_map;
public:
    cTCP_ETHSERVER(cIP &ip, cTCP_ETHCONN *p_pConn=NULL);  // NULL bei ISO-TP
    virtual ~cTCP_ETHSERVER(){}
    virtual void doReadEvent();
    cTCP_ETHCONN *getConn(){return pConn;}
    cISOTP_ETHCONN *getConnISO(const string &calling, const string &called);
    void setConnISO(cISOTP_ETHCONN *p_pConnISO);
    static cTCP_ETHSERVER *getServer(const string &adresse){ return pServer_map[adresse];}
    static void setServer(const string &adresse, cTCP_ETHSERVER *p_server){ pServer_map[adresse]=p_server; }
};

// ================================================================================
//
// Zwischenklasse zum kapseln von pEthernet und ip;
//
class cETHCONNECTBASE : public cCONNECTBASE, public TopoDataReceive
{
//private:
//    virtual void runData() override;
//    virtual void stopData() override;
protected:
    cTOPOLOGIE &topologie;
    cIP &remoteIp;
public:
    virtual int receiveData(DATATGR &tgr) override = 0;

    virtual void senddata(const void* pBuffer,size_t len)=0;
    cETHCONNECTBASE(cIP &ip, cTOPOLOGIE &top);
    virtual ~cETHCONNECTBASE(){}
    void writeObserver(DATATGR &tgr);
};


// ================================================================================

class BufferElem
{
public:
    char *buffer;
    int len;
    BufferElem(char *p_buffer,int p_len) : len(p_len)
    {
        buffer=new char[len];
        memcpy(buffer,p_buffer,len);
    }
    ~BufferElem(){ delete[] buffer;}
};

// ================================================================================
//
// Klasse f?r alle tcp-connections -> je tcp-modul eine instanz!
//
class cTCP_ETHCONN : public cETHCONNECTBASE
{
private:
    void runData() override;
    void stopData() override;
    bool autoconnecting;
    cIP localIp;
    sockaddr_in dynamic_addr;
    map<UINT64,cETH_ACCEPTANCE*> acceptance;
    queue<BufferElem*> bufQueue;
public:
    virtual int receiveData(DATATGR &tgr) override;

    // Kommandos die das Ethernet-MModul versteht
    enum { nBroadcast =1, nUnicast=2, nVerbindung_beendet=3, nVerbindung_hergestellt=4, nVerbindungsaufbau=5, nVerbindungsabbau=6};
    cTCP_ETHCONN(cIP &ip, cIP &remoteIp, cTOPOLOGIE &ethernet, bool _autoconnecting);
    void doReadEvent() override;
    void doWriteEvent() override;
    void senddata(const void* pBuffer,size_t len) override;
    void addAcceptance(cETH_ACCEPTANCE* p_pAccept);
    void removeAcceptance(cETH_ACCEPTANCE* p_pAccept);
    bool check_acceptance();
    bool isAutoconnecting()
    {
        return autoconnecting;
    }
    bool isConnected()
    {
        return conSndState==C_CONNECTED;
    }
    void abbau();
    void connect();
    void writeTopoData(DATATGR &tgr)
    {
        topologie.writeTopoData(tgr);
    }
    bool isDynamic()
    {
        return remoteIp.isDynamic();
    }
    void setDynamicAddress(IPADDRESS addr, WORD port);
};

// ================================================================================
// ================================================================================
//
// Klasse f?r alle iso-tp-connections -> je iso_tp-modul eine instanz!
//
class cISOTP_ETHCONN : public cETHCONNECTBASE
{
private:
    void stopData() override;
    void runData() override;
    string local_tsap;
    string remote_tsap;
    cIP &isoLocalIp;
    cIP &isoRemoteIp; // fürs bind vor dem connect...
    bool expedited_erlaubt;
    bool expedited_eingeschaltet;
    ISORECEIVE isoreceive;

    // hier auch die angenommenen Verbindungen merken...
    vector<cETH_ACCEPTANCE*> acceptance;
    queue<BufferElem*> bufQueue;
public:
    int receiveData(DATATGR &tgr) override;

    void senddata(const void* pIsoData,size_t len) override;
    void connect();
    string sap_kennung(){ return local_tsap+remote_tsap; }
    // Kommandos die das Ethernet-MModul versteht
    enum
    {
        nBroadcast =1,
        nUnicast=2,
        nVerbindung_beendet=3,
        nVerbindung_hergestellt=4,
        nVerbindungsaufbau=5,
        nVerbindungsabbau=6,
        nVerbindungsaufbau_beschleunigt=7,
        nVerbindung_hergestellt_beschleunigt=8,
        nUnicast_beschleunigt=9,
        nAkzeptiere_beschleunigt=98, // dieses Kommando muss vom tuSimDevcomm nach einrichten der BG gesendet werden
        nAkzeptiere_beschleunigt_nicht=99 // dieses Kommando muss vom tuSimDevcomm nach einrichten der BG gesendet werden
    };
    cISOTP_ETHCONN(cIP &localIp, cIP &remoteIp, cTOPOLOGIE &topo,const char *p_local_tsap, const char *p_remote_tsap);
    virtual void doReadEvent() override;
    virtual void doWriteEvent() override;
    void addAcceptance(cETH_ACCEPTANCE* p_pAccept);
    void removeAcceptance(cETH_ACCEPTANCE* p_pAccept);
    bool check_acceptance();
    int writeTopoData(DATATGR &tgr)
    {
        return topologie.writeTopoData(tgr);
    }
    void iso_received(rfc1006_datagram_t &isoData);
    void akzeptiere_beschleunigt(bool expedited);
    bool is_beschleunigt_akzeptiert();
    int iso_make_DT(const BYTE *buffer, int length, rfc1006_datagram_t &datagram, bool expedited);
    static int iso_make_DT0(rfc1006_datagram &datagram, bool expedited); // ben?tigt keine daten von this
    static int iso_make_ER(rfc1006_datagram &datagram); // ben?tigt keine daten von this
    int iso_make_CR(rfc1006_datagram &datagram, bool expedited);
    int iso_make_CC(int source,rfc1006_datagram &datagram);
    void iso_aufbau();
    void iso_beendet();
    void iso_abbau();
};

/*========================================================================================================== */

class cUDP_ETHCONN : public cETHCONNECTBASE
{
public:    
    void stopData() override;
    void runData() override;
    enum { nUnicast=2 };
    cUDP_ETHCONN(cIP &ip, cIP &remoteIp, cTOPOLOGIE &top);
    void senddata(const void* pBuffer,size_t len) override;
    void writeTopoData(DATATGR &tgr) { topologie.writeTopoData(tgr); }
    sockaddr_in altAddr;
    bool isDynamic(){return remoteIp.isDynamic();}
    virtual int receiveData(DATATGR &tgr) override;
private:
    cIP localIp;
};

// ================================================================================

/**
* Stellt die Instanz dar, an der alle einkommenden Daten von UDP-Telegrammen
* empfangen werden.
*/

class cUDP_ETHHANDLER
    : public cUDP_HANDLERBASE
{
private:
    cUDP_ETHCONN &pConn;

    // prevent making copies of cUDP_HANDLER:
    cUDP_ETHHANDLER(const cUDP_HANDLER&);
    cUDP_ETHHANDLER& operator=(const cUDP_HANDLER&);

public:
    void doReadEvent() override;
    cUDP_ETHHANDLER(cIP &ip, cUDP_ETHCONN &p_pConn);
    ~cUDP_ETHHANDLER() override {}
};


// =eof=
#endif
