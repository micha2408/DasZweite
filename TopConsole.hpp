#ifndef TOPCONSOLE_HPP
#define TOPCONSOLE_HPP
/**
* TOPCONSOLE Header
*/

/**
* @author Michael Bartels <michael.bartels@siemens.com>
*
* @copyright (c) 2021 Siemens Mobility GmbH
*/


#include "topEthernet.hpp"
#include <topmanuser.hpp>

class cCONSOLE
    : public TopoDataReceive
{
private:
    bool m_justStarted;
    string m_Modulename;
public:
    cCONSOLE(const string& p_Modulename);
    virtual ~cCONSOLE() {}
    int receiveData(DATATGR &tgr) override;
    void runData() override {setRunning();}
    void stopData() override {setPaused();}
};

#endif

// =eof=
