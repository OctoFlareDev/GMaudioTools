//
//  GMaudioTools.cpp
//  GMaudioTools
//
//  Created by Flare ​ on 8/7/25.
//

#include <iostream>
#include "GMaudioTools.hpp"
#include "GMaudioToolsPriv.hpp"

void GMaudioTools::HelloWorld(const char * s)
{
    GMaudioToolsPriv *theObj = new GMaudioToolsPriv;
    theObj->HelloWorldPriv(s);
    delete theObj;
};

void GMaudioToolsPriv::HelloWorldPriv(const char * s) 
{
    std::cout << s << std::endl;
};

