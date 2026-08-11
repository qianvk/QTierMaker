#import <Foundation/Foundation.h>

#include <cerrno>
#include <csignal>
#include <chrono>
#include <thread>

#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr auto kBundleIdentifier = "org.qtiermaker.app";

NSString* logFilePath() {
    NSString* logsDirectory = [NSHomeDirectory() stringByAppendingPathComponent:
        @"Library/Logs/QTierMaker"];
    [[NSFileManager defaultManager] createDirectoryAtPath:logsDirectory
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
    return [logsDirectory stringByAppendingPathComponent:@"update.log"];
}

void logMessage(NSString* message) {
    NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSSZZZZZ";
    NSString* line = [NSString stringWithFormat:@"%@ %@\n",
                                                [formatter stringFromDate:[NSDate date]], message];
    NSData* data = [line dataUsingEncoding:NSUTF8StringEncoding];
    NSString* path = logFilePath();
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        [data writeToFile:path atomically:YES];
        return;
    }
    NSFileHandle* handle = [NSFileHandle fileHandleForWritingAtPath:path];
    [handle seekToEndOfFile];
    [handle writeData:data];
    [handle closeFile];
}

NSString* argumentValue(NSArray<NSString*>* arguments, NSString* name) {
    const NSUInteger index = [arguments indexOfObject:name];
    if (index == NSNotFound || index + 1 >= arguments.count) {
        return nil;
    }
    return arguments[index + 1];
}

bool runTool(NSString* executable, NSArray<NSString*>* arguments) {
    NSTask* task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:executable];
    task.arguments = arguments;
    NSPipe* output = [NSPipe pipe];
    task.standardOutput = output;
    task.standardError = output;

    NSError* launchError = nil;
    if (![task launchAndReturnError:&launchError]) {
        logMessage([NSString stringWithFormat:@"update.helper.tool.launch.failed tool=%@ error=%@",
                                              executable, launchError.localizedDescription]);
        return false;
    }
    [task waitUntilExit];
    NSData* data = [[output fileHandleForReading] readDataToEndOfFile];
    NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (task.terminationStatus != 0) {
        logMessage([NSString stringWithFormat:
            @"update.helper.tool.failed tool=%@ status=%d output=\"%@\"", executable,
            task.terminationStatus, [text stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]]]);
        return false;
    }
    return true;
}

bool waitForProcessToExit(pid_t pid) {
    constexpr auto timeout = std::chrono::seconds(60);
    constexpr auto pollInterval = std::chrono::milliseconds(100);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        errno = 0;
        if (::kill(pid, 0) != 0 && errno == ESRCH) {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    return false;
}

NSString* numericVersion(NSString* version) {
    NSRange separator = [version rangeOfCharacterFromSet:
        [NSCharacterSet characterSetWithCharactersInString:@"-+"]];
    return separator.location == NSNotFound ? version : [version substringToIndex:separator.location];
}

bool validateApplication(NSString* applicationPath, NSString* expectedVersion) {
    NSString* infoPath = [applicationPath stringByAppendingPathComponent:@"Contents/Info.plist"];
    NSDictionary* info = [NSDictionary dictionaryWithContentsOfFile:infoPath];
    NSString* expectedIdentifier = [NSString stringWithUTF8String:kBundleIdentifier];
    if (![info[@"CFBundleIdentifier"] isEqualToString:expectedIdentifier]) {
        logMessage(@"update.helper.validation.failed reason=bundle-identifier");
        return false;
    }
    NSString* bundleVersion = info[@"CFBundleShortVersionString"];
    if (expectedVersion.length > 0 &&
        ![bundleVersion isEqualToString:numericVersion(expectedVersion)]) {
        logMessage([NSString stringWithFormat:
            @"update.helper.validation.failed reason=version expected=%@ actual=%@",
            numericVersion(expectedVersion), bundleVersion]);
        return false;
    }
    NSString* executable = [applicationPath stringByAppendingPathComponent:
        @"Contents/MacOS/QTierMaker"];
    if (![[NSFileManager defaultManager] isExecutableFileAtPath:executable]) {
        logMessage(@"update.helper.validation.failed reason=missing-executable");
        return false;
    }
    return runTool(@"/usr/bin/codesign", @[@"--verify", @"--deep", @"--strict", applicationPath]);
}

bool removeIfPresent(NSFileManager* manager, NSString* path) {
    if (![manager fileExistsAtPath:path]) {
        return true;
    }
    NSError* error = nil;
    if ([manager removeItemAtPath:path error:&error]) {
        return true;
    }
    logMessage([NSString stringWithFormat:@"update.helper.remove.failed path=\"%@\" error=%@",
                                          path, error.localizedDescription]);
    return false;
}

} // namespace

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        (void)argc;
        (void)argv;
        NSArray<NSString*>* arguments = [[NSProcessInfo processInfo] arguments];
        NSString* pidText = argumentValue(arguments, @"--pid");
        NSString* archivePath = argumentValue(arguments, @"--archive");
        NSString* applicationPath = argumentValue(arguments, @"--application");
        NSString* expectedVersion = argumentValue(arguments, @"--expected-version");
        const pid_t parentPid = static_cast<pid_t>(pidText.intValue);
        if (parentPid <= 0 || archivePath.length == 0 || applicationPath.length == 0 ||
            ![applicationPath.pathExtension.lowercaseString isEqualToString:@"app"]) {
            logMessage(@"update.helper.arguments.invalid");
            return 2;
        }

        logMessage([NSString stringWithFormat:
            @"update.helper.start pid=%d archive=\"%@\" application=\"%@\" version=%@",
            parentPid, archivePath, applicationPath,
            expectedVersion != nil ? expectedVersion : @""]);
        if (!waitForProcessToExit(parentPid)) {
            logMessage(@"update.helper.wait.failed reason=timeout");
            return 3;
        }

        NSFileManager* manager = [NSFileManager defaultManager];
        NSString* parentDirectory = [applicationPath stringByDeletingLastPathComponent];
        NSString* token = [NSString stringWithFormat:@"%d", getpid()];
        NSString* stagingDirectory = [parentDirectory stringByAppendingPathComponent:
            [NSString stringWithFormat:@".QTierMaker.update.%@", token]];
        NSString* backupPath = [parentDirectory stringByAppendingPathComponent:
            [NSString stringWithFormat:@".QTierMaker.backup.%@.app", token]];
        if (!removeIfPresent(manager, stagingDirectory) || !removeIfPresent(manager, backupPath)) {
            return 4;
        }

        NSError* error = nil;
        if (![manager createDirectoryAtPath:stagingDirectory
                 withIntermediateDirectories:YES attributes:nil error:&error]) {
            logMessage([NSString stringWithFormat:@"update.helper.stage.failed error=%@",
                                                  error.localizedDescription]);
            return 5;
        }
        if (!runTool(@"/usr/bin/ditto", @[@"-x", @"-k", archivePath, stagingDirectory])) {
            removeIfPresent(manager, stagingDirectory);
            return 6;
        }

        NSString* stagedApplication = [stagingDirectory stringByAppendingPathComponent:
            applicationPath.lastPathComponent];
        if (!validateApplication(stagedApplication, expectedVersion)) {
            removeIfPresent(manager, stagingDirectory);
            return 7;
        }

        error = nil;
        if (![manager moveItemAtPath:applicationPath toPath:backupPath error:&error]) {
            logMessage([NSString stringWithFormat:@"update.helper.backup.failed error=%@",
                                                  error.localizedDescription]);
            removeIfPresent(manager, stagingDirectory);
            return 8;
        }
        error = nil;
        if (![manager moveItemAtPath:stagedApplication toPath:applicationPath error:&error]) {
            logMessage([NSString stringWithFormat:@"update.helper.replace.failed error=%@",
                                                  error.localizedDescription]);
            [manager moveItemAtPath:backupPath toPath:applicationPath error:nil];
            removeIfPresent(manager, stagingDirectory);
            return 9;
        }

        removeIfPresent(manager, stagingDirectory);
        if (!runTool(@"/usr/bin/open", @[@"-n", applicationPath])) {
            removeIfPresent(manager, applicationPath);
            [manager moveItemAtPath:backupPath toPath:applicationPath error:nil];
            runTool(@"/usr/bin/open", @[@"-n", applicationPath]);
            logMessage(@"update.helper.relaunch.failed action=rollback");
            return 10;
        }

        removeIfPresent(manager, backupPath);
        removeIfPresent(manager, archivePath);
        logMessage(@"update.helper.finish result=success");
        return 0;
    }
}
