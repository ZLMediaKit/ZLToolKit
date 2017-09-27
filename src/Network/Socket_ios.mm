/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
