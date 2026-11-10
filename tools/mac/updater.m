// Standalone, Universal macOS updater. Requires only libraries shipped with macOS 11+.
// The client holds its installation lock and waits before opening a game window.
#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/fat.h>
#include <mach/machine.h>
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static NSString *const repository = @"channynet/slither.io-sad-mod";
static NSString *const bundleID = @"io.github.slithernative.client";
static const long long archiveLimit = 512LL * 1024 * 1024;
static const long long expandedLimit = 1024LL * 1024 * 1024;

static void require(BOOL condition, NSString *message) {
    if (!condition) @throw [NSException exceptionWithName:@"UpdateError" reason:message userInfo:nil];
}
static BOOL matches(NSString *value, NSString *pattern) {
    if (![value isKindOfClass:NSString.class]) return NO;
    return [value rangeOfString:pattern options:NSRegularExpressionSearch].location != NSNotFound;
}
static NSArray<NSNumber *> *versionParts(NSString *version) {
    require(matches(version, @"^v?[0-9]{1,6}\\.[0-9]{1,6}-ntl[0-9]{1,6}\\.[0-9]{1,6}$"), @"Invalid version");
    NSString *plain = [version hasPrefix:@"v"] ? [version substringFromIndex:1] : version;
    NSArray *parts = [[plain stringByReplacingOccurrencesOfString:@"-ntl" withString:@"."] componentsSeparatedByString:@"."];
    return @[@([parts[0] integerValue]), @([parts[1] integerValue]), @([parts[2] integerValue]), @([parts[3] integerValue])];
}
static BOOL newer(NSString *candidate, NSString *installed) {
    NSArray *a = versionParts(candidate), *b = versionParts(installed);
    for (NSUInteger i = 0; i < a.count; ++i) {
        NSComparisonResult result = [a[i] compare:b[i]];
        if (result != NSOrderedSame) return result == NSOrderedDescending;
    }
    return NO;
}
static NSDictionary *selectAsset(NSDictionary *release, NSString *installed) {
    require([release isKindOfClass:NSDictionary.class], @"Invalid release response");
    if ([release[@"draft"] boolValue] || [release[@"prerelease"] boolValue]) return nil;
    NSString *tag = release[@"tag_name"];
    if (!newer(tag, installed)) return nil;
    require([tag hasPrefix:@"v"], @"Invalid release tag");
    NSString *name = [NSString stringWithFormat:@"SlitherNative-%@-macOS-Universal.zip", [tag substringFromIndex:1]];
    NSString *url = [NSString stringWithFormat:@"https://github.com/%@/releases/download/%@/%@", repository, tag, name];
    require([release[@"assets"] isKindOfClass:NSArray.class], @"Missing release assets");
    NSDictionary *found = nil;
    for (NSDictionary *asset in release[@"assets"]) {
        require([asset isKindOfClass:NSDictionary.class], @"Invalid release asset");
        if (![asset[@"name"] isEqual:name]) continue;
        require(!found, @"Duplicate Mac package");
        require([asset[@"browser_download_url"] isEqual:url], @"Unexpected package URL");
        require(matches(asset[@"digest"], @"^sha256:[a-fA-F0-9]{64}$"), @"Missing SHA-256 digest");
        require([asset[@"size"] isKindOfClass:NSNumber.class] && [asset[@"size"] longLongValue] > 0 &&
                [asset[@"size"] longLongValue] <= archiveLimit, @"Invalid package size");
        found = asset;
    }
    return found; // Older Windows-only releases do not interrupt Mac startup.
}
static BOOL run(NSString *command, NSArray<NSString *> *arguments) {
    NSTask *task = [NSTask new];
    task.executableURL = [NSURL fileURLWithPath:command];
    task.arguments = arguments;
    task.standardOutput = [NSFileHandle fileHandleWithNullDevice];
    task.standardError = [NSFileHandle fileHandleWithNullDevice];
    if (![task launchAndReturnError:nil]) return NO;
    [task waitUntilExit];
    return task.terminationReason == NSTaskTerminationReasonExit && task.terminationStatus == 0;
}
static void download(NSString *url, NSString *path, int seconds, long long limit) {
    require(run(@"/usr/bin/curl", @[@"--disable", @"--fail", @"--silent", @"--show-error", @"--location",
        @"--proto", @"=https", @"--proto-redir", @"=https", @"--connect-timeout", @"5",
        @"--max-time", [@(seconds) stringValue], @"--max-filesize", [@(limit) stringValue],
        @"--user-agent", @"SlitherNative-Updater", @"--output", path, url]), @"Download failed");
    NSDictionary *attributes = [NSFileManager.defaultManager attributesOfItemAtPath:path error:nil];
    require(attributes && [attributes fileSize] > 0 && [attributes fileSize] <= (unsigned long long)limit, @"Download size invalid");
}
static NSString *sha256(NSString *path) {
    FILE *file = fopen(path.fileSystemRepresentation, "rb");
    require(file != NULL, @"Cannot read package");
    CC_SHA256_CTX context; CC_SHA256_Init(&context);
    unsigned char bytes[65536], digest[CC_SHA256_DIGEST_LENGTH];
    size_t count;
    while ((count = fread(bytes, 1, sizeof(bytes), file))) CC_SHA256_Update(&context, bytes, (CC_LONG)count);
    BOOL ok = !ferror(file); fclose(file);
    require(ok, @"Cannot hash package");
    CC_SHA256_Final(digest, &context);
    NSMutableString *hex = [NSMutableString stringWithString:@"sha256:"];
    for (NSUInteger i = 0; i < sizeof(digest); ++i) [hex appendFormat:@"%02x", digest[i]];
    return hex;
}
static BOOL safeEntry(NSString *name) {
    if (!name.length || [name hasPrefix:@"/"] || [name containsString:@"\\"] || [name containsString:@":"]) return NO;
    for (NSString *part in [name componentsSeparatedByString:@"/"])
        if ([part isEqual:@".."] || [part isEqual:@"."]) return NO;
    return YES;
}
static void extract(NSString *zip, NSString *destination, NSString *rootName) {
    struct archive *reader = archive_read_new();
    require(reader != NULL, @"Cannot create archive reader");
    @try {
        archive_read_support_format_zip(reader);
        require(archive_read_open_filename(reader, zip.fileSystemRepresentation, 65536) == ARCHIVE_OK, @"Invalid ZIP");
        struct archive_entry *entry;
        long long total = 0; NSUInteger entries = 0;
        NSMutableSet *seen = [NSMutableSet new];
        int status;
        while ((status = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
            const char *raw = archive_entry_pathname(entry);
            NSString *name = raw ? [NSString stringWithUTF8String:raw] : nil;
            require(safeEntry(name) && ++entries <= 30000, @"Unsafe ZIP path or too many entries");
            mode_t type = archive_entry_filetype(entry);
            require((type == AE_IFREG || type == AE_IFDIR) && !archive_entry_symlink(entry) && !archive_entry_hardlink(entry), @"ZIP links are not allowed");
            long long length = archive_entry_size(entry);
            require(length >= 0 && length <= expandedLimit - total, @"Expanded package too large");
            total += length;
            if ([name hasPrefix:@"__MACOSX/"]) { archive_read_data_skip(reader); continue; }
            require([name isEqual:rootName] || [name hasPrefix:[rootName stringByAppendingString:@"/"]], @"Unexpected ZIP root");
            NSString *path = [destination stringByAppendingPathComponent:name];
            NSString *key = path.stringByStandardizingPath.lowercaseString;
            require(![seen containsObject:key], @"Duplicate ZIP path"); [seen addObject:key];
            NSString *directory = type == AE_IFDIR ? path : path.stringByDeletingLastPathComponent;
            require([NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil], @"Cannot create staging directory");
            if (type == AE_IFDIR) continue;
            int fd = open(path.fileSystemRepresentation, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
            require(fd >= 0, @"Cannot create staged file");
            @try {
                char buffer[65536]; la_ssize_t amount; long long actual = 0;
                while ((amount = archive_read_data(reader, buffer, sizeof(buffer))) > 0) {
                    actual += amount; require(actual <= length, @"ZIP size mismatch");
                    la_ssize_t offset = 0;
                    while (offset < amount) {
                        ssize_t written = write(fd, buffer + offset, (size_t)(amount - offset));
                        if (written < 0 && errno == EINTR) continue;
                        require(written > 0, @"Cannot write staged file"); offset += written;
                    }
                }
                require(amount == 0 && actual == length, @"Incomplete ZIP data");
                require(fchmod(fd, (archive_entry_perm(entry) & 0111) ? 0755 : 0644) == 0, @"Cannot set file permissions");
            } @finally { close(fd); }
        }
        require(status == ARCHIVE_EOF, @"Corrupt ZIP");
    } @finally { archive_read_free(reader); }
}
static NSDictionary *bundleInfo(NSString *app) {
    return [NSDictionary dictionaryWithContentsOfFile:[app stringByAppendingPathComponent:@"Contents/Info.plist"]];
}
static BOOL universalBinary(NSString *path) {
    FILE *file = fopen(path.fileSystemRepresentation, "rb");
    if (!file) return NO;
    struct fat_header header;
    BOOL arm = NO, intel = NO;
    if (fread(&header, sizeof(header), 1, file) == 1 && OSSwapBigToHostInt32(header.magic) == FAT_MAGIC) {
        uint32_t count = OSSwapBigToHostInt32(header.nfat_arch);
        if (count <= 16) for (uint32_t i = 0; i < count; ++i) {
            struct fat_arch arch;
            if (fread(&arch, sizeof(arch), 1, file) != 1) break;
            cpu_type_t cpu = (cpu_type_t)OSSwapBigToHostInt32((uint32_t)arch.cputype);
            arm |= cpu == CPU_TYPE_ARM64;
            intel |= cpu == CPU_TYPE_X86_64;
        }
    }
    fclose(file);
    return arm && intel;
}
static void validateBundle(NSString *app, NSString *version) {
    NSDictionary *info = bundleInfo(app);
    require([info[@"CFBundleIdentifier"] isEqual:bundleID] && [info[@"CFBundleVersion"] isEqual:version] &&
            [info[@"CFBundleExecutable"] isEqual:@"SlitherNative"], @"Wrong application or version");
    NSString *minimum = info[@"LSMinimumSystemVersion"];
    require(matches(minimum, @"^[0-9]+\\.[0-9]+(\\.[0-9]+)?$"), @"Missing minimum macOS version");
    NSArray *parts = [minimum componentsSeparatedByString:@"."];
    NSOperatingSystemVersion os = {[parts[0] integerValue], [parts[1] integerValue], parts.count > 2 ? [parts[2] integerValue] : 0};
    require([NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:os], @"New version requires a newer macOS");
    for (NSString *binary in @[@"SlitherNative", @"SlitherUpdater"]) {
        NSString *path = [app stringByAppendingPathComponent:[@"Contents/MacOS" stringByAppendingPathComponent:binary]];
        require(access(path.fileSystemRepresentation, X_OK) == 0 && universalBinary(path), @"Invalid Universal binary");
    }
    require(run(@"/usr/bin/codesign", @[@"--verify", @"--deep", @"--strict", app]), @"Application signature verification failed");
}
static void writeStatus(NSString *message) {
    NSString *dir = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/SlitherNative/.update"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil];
    [message writeToFile:[dir stringByAppendingPathComponent:@"status.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
}
#ifndef SLITHER_UPDATER_TEST
int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSString *stage = nil;
        @try {
            require(argc == 4, @"Invalid updater arguments");
            NSString *app = [NSString stringWithUTF8String:argv[1]];
            NSString *installed = [NSString stringWithUTF8String:argv[2]];
            NSString *mode = [NSString stringWithUTF8String:argv[3]];
            require([app isAbsolutePath] && [app.pathExtension isEqual:@"app"] &&
                    [app isEqual:app.stringByResolvingSymlinksInPath] && ![app containsString:@"/AppTranslocation/"], @"Unsupported installation location");
            require([bundleInfo(app)[@"CFBundleIdentifier"] isEqual:bundleID], @"Wrong installed application");
            NSString *backup = [app stringByAppendingString:@".update-backup"];
            NSFileManager *fm = NSFileManager.defaultManager;
            if ([mode isEqual:@"--rollback"]) {
                require([backup isEqual:backup.stringByResolvingSymlinksInPath], @"Unsafe backup path");
                validateBundle(backup, installed);
                require(renamex_np(app.fileSystemRepresentation, backup.fileSystemRepresentation, RENAME_SWAP) == 0, @"Rollback failed");
                writeStatus(@"Previous version restored after restart failure.");
                return 0;
            }
            require([mode isEqual:@"--update"], @"Invalid update mode");
            // Another startup may have finished updating before this process took the lock.
            if (![bundleInfo(app)[@"CFBundleVersion"] isEqual:installed]) return 10;
            NSString *parent = app.stringByDeletingLastPathComponent;
            require(access(parent.fileSystemRepresentation, W_OK) == 0 && access(app.fileSystemRepresentation, W_OK) == 0, @"Installation is not writable; move the app to a writable Applications folder.");
            char temporary[PATH_MAX];
            require(snprintf(temporary, sizeof(temporary), "%s/.slither-update.XXXXXX", parent.fileSystemRepresentation) < (int)sizeof(temporary), @"Installation path too long");
            require(mkdtemp(temporary) != NULL, @"Cannot create update directory");
            stage = [NSString stringWithUTF8String:temporary];
            NSString *metadata = [stage stringByAppendingPathComponent:@"release.json"];
            download([NSString stringWithFormat:@"https://api.github.com/repos/%@/releases/latest", repository], metadata, 8, 2 * 1024 * 1024);
            NSDictionary *release = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:metadata] options:0 error:nil];
            NSDictionary *asset = selectAsset(release, installed);
            if (!asset) { writeStatus(@"No newer compatible Mac package."); return 0; }
            NSString *zip = [stage stringByAppendingPathComponent:@"update.zip"];
            download(asset[@"browser_download_url"], zip, 120, [asset[@"size"] longLongValue]);
            require([[fm attributesOfItemAtPath:zip error:nil] fileSize] == [asset[@"size"] unsignedLongLongValue] &&
                    [sha256(zip) isEqual:[asset[@"digest"] lowercaseString]], @"Package size or SHA-256 mismatch");
            NSString *version = [release[@"tag_name"] substringFromIndex:1];
            NSString *root = [NSString stringWithFormat:@"SlitherNative-%@-macOS-Universal", version];
            extract(zip, stage, root);
            NSString *candidate = [[stage stringByAppendingPathComponent:root] stringByAppendingPathComponent:@"SlitherNative.app"];
            validateBundle(candidate, version);
            // A backup is only touched if it is a real bundle from a previous update.
            if ([fm fileExistsAtPath:backup]) {
                require([backup isEqual:backup.stringByResolvingSymlinksInPath] &&
                        [bundleInfo(backup)[@"CFBundleIdentifier"] isEqual:bundleID], @"Unsafe backup path");
                require([fm removeItemAtPath:backup error:nil], @"Cannot remove previous backup");
            }
            require([fm moveItemAtPath:candidate toPath:backup error:nil], @"Cannot stage replacement");
            // Both names remain valid even if the machine loses power during the exchange.
            // Unsupported filesystems fail here without modifying the installed application.
            require(renamex_np(app.fileSystemRepresentation, backup.fileSystemRepresentation, RENAME_SWAP) == 0, @"Atomic application replacement failed");
            writeStatus([NSString stringWithFormat:@"Installed %@. Previous app: %@", version, backup]);
            return 10; // Parent replaces itself with the new executable, skipping one check.
        } @catch (NSException *error) {
            writeStatus([NSString stringWithFormat:@"Update skipped: %@", error.reason]);
            return 1;
        } @finally {
            if (stage) [NSFileManager.defaultManager removeItemAtPath:stage error:nil];
        }
    }
}
#endif
