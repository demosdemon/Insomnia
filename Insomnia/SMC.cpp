//
//  SMC.cpp
//  Insomnia
//
//  Created by Brandon LeBlanc on 11/8/16.
//  Copyright © 2016 PabuCo. All rights reserved.
//

#include "SMC.hpp"

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>

enum ValueResult {
    ValueResultFailure,
    ValueResultInteger,
    ValueResultLong,
    ValueResultFloat,
    ValueResultDouble,
    ValueResultString
};

ValueResult transcodeValue(const SMCVal_t * value, char * buffer, int bufferLen);

double SMC::getTemperature()
{
    SMCVal_t value;
    memset(&value, 0, sizeof(SMCVal_t));

    if (readKey(CPU_0_DIE, &value) == kIOReturnSuccess) {
        char buffer[256] = "";
        if (transcodeValue(&value, buffer, 256) != ValueResultFailure) {
            int result = (int)(*buffer);
            return result / 256.0;
        }
    }

    return 0.0;
}

kern_return_t SMC::open()
{
    OSDictionary * matchingDictionary = IOService::serviceMatching("AppleSMC");
    if (matchingDictionary == NULL) return kIOReturnNotFound;

    OSIterator * iterator = IOService::getMatchingServices(matchingDictionary);
    IOService * first = (IOService *) iterator->getNextObject();
    iterator->release();

    if (first)
    {
        connection = first;
        connection->retain();
        return kIOReturnSuccess;
    }

    return kIOReturnNotFound;
}

kern_return_t SMC::close()
{
    connection->release();
    return kIOReturnSuccess;
}

kern_return_t SMC::call(int index, struct KeyData *input, struct KeyData *output)
{
    
}
