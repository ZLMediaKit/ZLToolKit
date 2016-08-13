//
//  Socket_ios.m
//  G平台
//
//  Created by boyo on 16/4/18.
//  Copyright © 2016年 boyo. All rights reserved.
//
#import "Socket.hpp"
#include "Util/logger.h"

#if defined (__APPLE__)
#import <Foundation/Foundation.h>
#endif

using namespace ZL::Util;

namespace ZL {
namespace Network {



#if defined (__APPLE__)
bool Socket::setSocketOfIOS(int m_socket){
    
    CFStreamCreatePairWithSocket(NULL, (CFSocketNativeHandle)m_socket, (CFReadStreamRef *)(&readStream), (CFWriteStreamRef*)(&writeStream));
    if (readStream)
        CFReadStreamSetProperty((CFReadStreamRef)readStream, kCFStreamPropertyShouldCloseNativeSocket, kCFBooleanFalse);
    if (writeStream)
        CFWriteStreamSetProperty((CFWriteStreamRef)writeStream, kCFStreamPropertyShouldCloseNativeSocket, kCFBooleanFalse);
    if ((readStream == NULL) || (writeStream == NULL))
    {
        WarnL<<"Unable to create read and write stream...";
        if (readStream)
        {
            CFReadStreamClose((CFReadStreamRef)readStream);
            CFRelease(readStream);
            readStream = NULL;
        }
        if (writeStream)
        {
            CFWriteStreamClose((CFWriteStreamRef)writeStream);
            CFRelease(writeStream);
            writeStream = NULL;
        }
        return false;
    }
    
    
    Boolean r1 = CFReadStreamSetProperty((CFReadStreamRef)readStream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    Boolean r2 = CFWriteStreamSetProperty((CFWriteStreamRef)writeStream, kCFStreamNetworkServiceType, kCFStreamNetworkServiceTypeVoIP);
    
    if (!r1 || !r2)
    {
        return false;
    }
    
    CFStreamStatus readStatus = CFReadStreamGetStatus((CFReadStreamRef)readStream);
    CFStreamStatus writeStatus = CFWriteStreamGetStatus((CFWriteStreamRef)writeStream);
    
    if ((readStatus == kCFStreamStatusNotOpen) || (writeStatus == kCFStreamStatusNotOpen))
    {
        BOOL r1 = CFReadStreamOpen((CFReadStreamRef)readStream);
        BOOL r2 = CFWriteStreamOpen((CFWriteStreamRef)writeStream);
        
        if (!r1 || !r2)
        {
            WarnL<<"Error in CFStreamOpen";
            return false;
        }
    }
    //NSLog(@"setSocketOfIOS:%d",m_socket);
    return true;
}
void Socket::unsetSocketOfIOS(int m_socket){
    //NSLog(@"unsetSocketOfIOS:%d",m_socket);
    if (readStream) {
        CFReadStreamClose((CFReadStreamRef)readStream);
        readStream=NULL;
    }
    if (writeStream) {
        CFWriteStreamClose((CFWriteStreamRef)writeStream);
        writeStream=NULL;
    }
}
#endif



}  // namespace Network
}  // namespace ZL
