/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#import "Socket.h"
#include "Util/logger.h"

#if defined (OS_IPHONE)
#import <Foundation/Foundation.h>
#endif //OS_IPHONE

namespace toolkit {

#if defined (OS_IPHONE)
bool SockNum::setSocketOfIOS(int sock){
    
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
void SockNum::unsetSocketOfIOS(int sock){
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



}  // namespace toolkit
