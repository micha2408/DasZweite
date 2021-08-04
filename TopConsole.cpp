/**
 * Console für Ausgaben von Topologie
 *
 * @copyright (c) 2006, 2007, 2008, 2009, 2010, 2021 Siemens AG
 */

#include "TopConsole.hpp"


// ================================================================================

cCONSOLE::cCONSOLE(const string& moduleName)
    : m_Modulename(moduleName)
{
    cTOPMANLOG::error("cCONSOLE::cCONSOLE module=%s\n",m_Modulename.c_str());
}
int cCONSOLE::receiveData(DATATGR &tgr)
{
    if (tgr.getDataLen()>0)
    {        
        cTOPOLOGIE::trace("%s> cnt=%d ", m_Modulename.c_str(), tgr.getDataLen());
        cTOPOLOGIE::dump(tgr);
    }
    else
    {
        cTOPOLOGIE::trace("cCONSOLE::receive called module=%s with len 0\n",m_Modulename.c_str());
    }
    return 0;
}
 

// =eof=
