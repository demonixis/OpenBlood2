// NSURLSession transport (macOS, iOS): the system TLS stack and trust store. Built with ARC.
#include "app/https_download.h"
#include "engine/config.h"
#include <SDL3/SDL_iostream.h>
#import <Foundation/Foundation.h>

@interface OB2DownloadDelegate : NSObject <NSURLSessionDataDelegate>
@end

@implementation OB2DownloadDelegate
{
  @public
    SDL_IOStream* io;
    const std::function<bool(std::uint64_t)>* progress;
    std::uint64_t limit, written;
    long status;
    std::string location, failure;
    bool cancelled, overflow;
    dispatch_semaphore_t done;
}
- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler
{
    // Redirects are returned to the caller, which validates every hop.
    if (NSString* target = request.URL.absoluteString)
        location = target.UTF8String;
    completionHandler(nil);
}
- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)task
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler
{
    if ([response isKindOfClass:[NSHTTPURLResponse class]])
        status = long(((NSHTTPURLResponse*)response).statusCode);
    completionHandler(NSURLSessionResponseAllow);
}
- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)task didReceiveData:(NSData*)data
{
    if (cancelled || overflow)
        return;
    __block bool ok = true;
    [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
      if (range.length > limit - written || SDL_WriteIO(io, bytes, range.length) != range.length)
      {
          ok = false;
          *stop = YES;
          return;
      }
      written += range.length;
    }];
    overflow = !ok;
    cancelled = ok && !(*progress)(written);
    if (overflow || cancelled)
        [task cancel];
}
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
    if (error && error.localizedDescription)
        failure = error.localizedDescription.UTF8String;
    else if (error)
        failure = "Download failed";
    dispatch_semaphore_signal(done);
}
@end

namespace OpenBlood2
{
    bool https_get(std::string_view url, const std::filesystem::path& file, std::uint64_t limit,
                   const std::function<bool(std::uint64_t)>& progress, HttpsResponse& response, std::string& error)
    {
        response = {};
        @autoreleasepool
        {
            NSString* text = [[NSString alloc] initWithBytes:url.data()
                                                      length:url.size()
                                                    encoding:NSUTF8StringEncoding];
            NSURL* address = text ? [NSURL URLWithString:text] : nil;
            if (!address || ![address.scheme isEqualToString:@"https"])
            {
                error = "Download requires an HTTPS address";
                return false;
            }
            auto* io = SDL_IOFromFile(path_text(file).c_str(), "wb");
            if (!io)
            {
                error = "Cannot create download file: " + std::string(SDL_GetError());
                return false;
            }
            OB2DownloadDelegate* delegate = [[OB2DownloadDelegate alloc] init];
            delegate->io = io;
            delegate->progress = &progress;
            delegate->limit = limit;
            delegate->written = 0;
            delegate->status = 0;
            delegate->cancelled = delegate->overflow = false;
            delegate->done = dispatch_semaphore_create(0);
            NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
            configuration.timeoutIntervalForRequest = 30;
            configuration.timeoutIntervalForResource = 600;
            configuration.HTTPAdditionalHeaders = @{@"User-Agent" : @"OpenBlood2/1"};
            NSOperationQueue* queue = [[NSOperationQueue alloc] init];
            queue.maxConcurrentOperationCount = 1;
            NSURLSession* session = [NSURLSession sessionWithConfiguration:configuration
                                                                  delegate:delegate
                                                             delegateQueue:queue];
            [[session dataTaskWithURL:address] resume];
            dispatch_semaphore_wait(delegate->done, DISPATCH_TIME_FOREVER);
            // A session retains its delegate until it is invalidated.
            [session finishTasksAndInvalidate];
            response.status = delegate->status;
            response.bytes = delegate->written;
            if (response.status >= 300 && response.status < 400)
                response.location = delegate->location;
            const bool closed = SDL_CloseIO(io);
            if (delegate->cancelled)
                error = "Installation cancelled";
            else if (delegate->overflow)
                error = "Download exceeds the expected size";
            else if (!delegate->failure.empty() && !(response.status >= 300 && response.status < 400))
                error = "Download failed: " + delegate->failure;
            else if (!closed)
                error = "Cannot finish the download file";
            else
                return true;
            return false;
        }
    }
} // namespace OpenBlood2
