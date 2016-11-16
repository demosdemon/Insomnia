//
//  SMC.hpp
//  Insomnia
//
//  Created by Brandon LeBlanc on 11/8/16.
//  Copyright © 2016 PabuCo. All rights reserved.
//

#ifndef SMC_hpp
#define SMC_hpp

#include <IOKit/IOTypes.h>
#include <machine/types.h>

#include "smc.h"

class IOService;

class SMC {
public:
    double getTemperature();

    ~SMC() { close(); }

private:
    
    IOService * connection;

    kern_return_t open();
    kern_return_t close();
    kern_return_t call(int index, SMCKeyData_t * input, SMCKeyData_t * output);
    kern_return_t readKey(const UInt32Char_t key, SMCVal_t * value);
};

#endif /* SMC_hpp */
