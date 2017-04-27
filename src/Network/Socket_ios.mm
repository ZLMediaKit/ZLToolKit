//
//  Socket_ios.m
//  xzl
//
//  Created by xzl on 16/4/18.
//
#import "Socket.h"
#include "Util/logger.h"

#if defined (OS_IPHONE)
#import <Foundation/Foundation.h>
#endif //OS_IPHONE

using namespace ZL::Util;

namespace ZL {
namespace Network {



#if defined (OS_IPHONE)
bool SockFD::setSocketOfIOS(int sock){
    
    CFStreamCreatePairWithSocket(NULL, (CFSocketNativeHandle)sock, (CFReadStreamRef *)(&readStream), (CFWriteStreamRef*)(&writeStream));
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
    //NSLog(@"setSocketOfIOS:%d",sock);
    return true;
}
void SockFD::unsetSocketOfIOS(int sock){
    //NSLog(@"unsetSocketOfIOS:%d",sock);
    if (readStream) {
        CFReadStreamClose((CFReadStreamRef)readStream);
        readStream=NULL;
    }
    if (writeStream) {
        CFWriteStreamClose((CFWriteStreamRef)writeStream);
        writeStream=NULL;
    }
}
#endif //OS_IPHONE



}  // namespace Network
}  // namespace ZL
