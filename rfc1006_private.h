/******************************************************************************
	
	@file     PCB_Dev\src\VoterTestPC\src\RFC1006.h
      @ident    P25184-A1-B13-*-SD
      @author   Swati.Khale@bom4.siemens.co.in</A>
      $Revision: /main/1 $
	
@copyright  COPYRIGHT (C) SIEMENS AG 2002 - ALL RIGHTS RESERVED - CONFIDENTIAL

@since 2004-08-19 Stefan.Schildener@siemens.com
			Anpassungen an MS VS6.0

*******************************************************************************

summary:
	Funktionen, um das RFC1006-Protokoll, so wie die NCU
	es benutzt, auf einen Datenstrom aufzusetzen.
	

boundary conditions:
	none

******************************************************************************/

#ifndef __RFC1006_PRIVATE_H__
#define __RFC1006_PRIVATE_H__

// include "system.h"

/*
	Konstanten:
*/
#define cMaxTSAPLength          5

#define cTPDU_CR		        0xe0				// TPDU Connection Request
#define cTPDU_CC		        0xd0				// TPDU Connection Confirm
#define cTPDU_DT		        0xf0				// TPDU Datagram
#define cTPDU_ED		        0x10				// TPDU Datagram expedited
#define cTPDU_ER		        0x70				// TPDU Error

#define cParamCode_TPDUSIZE     0xc0
#define cParamCode_SrcTSAP      0xc1
#define cParamCode_DstTSAP      0xc2
#define cParamCode_AddOptionSel 0xc6
#define cParamValue_AosExpedited 0x01

#define ciLTSAP_len			    4		            // index of local-TSAP-len in parameters[]

/*
	Datenstrukturen:
*/

// define RFC1006_MAX_USER_DATA 240 Willkürliche Annahme. Ersetzt durch 1024 (ebenso willkürlich)
#define RFC1006_MAX_USER_DATA 1024

/*
	Datagramm nach RFC1006
*/
struct rfc1006_datagram_header
{
	unsigned char      version;
	unsigned char      reserved;
	unsigned short int length;
};

struct rfc1006_user_datagram_subheader
{
	unsigned char LI;  // 0x02
	unsigned char CO;  // 0xf0
	unsigned char ET;  // 0x80
};

union rfc1006_datagram
{
	struct
	{
		struct rfc1006_datagram_header header;
		unsigned char                  LI;
		unsigned char                  CO;             // 0xe0
		unsigned short int             destination;    // 0
		unsigned short int             source;         // != 0
		unsigned char                  CL;             // 0
		unsigned char                  parameters[23]; // 23 = Max. Laenge CO, C1 and C2 fuer das CR/CC datagramm
	} connection_request;

	struct
	{
		struct rfc1006_datagram_header header;
		unsigned char                  LI;
		unsigned char                  CO;             // 0xd0
		unsigned short int             destination;    // 0
		unsigned short int             source;         // !=0
		unsigned char                  CL;             // 0
		unsigned char                  parameters[23]; // 23 = Max. Laenge CO, C1 and C2 fuer das CR/CC datagramm
	} connection_confirmation;

	struct
	{
		struct rfc1006_datagram_header         header;
		struct rfc1006_user_datagram_subheader subheader;
		unsigned char                          data[RFC1006_MAX_USER_DATA];
	} user_data;
};

typedef union rfc1006_datagram rfc1006_datagram_t;

#endif // ifndef __RFC1006_PRIVATE_H__
