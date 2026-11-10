// No game launch or network access. Run on the macOS build runner.
#define SLITHER_UPDATER_TEST
#include "../tools/mac/updater.m"

static int checks = 0;
static void check(BOOL ok, NSString *message) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message.UTF8String); exit(1); }
    ++checks;
}
static void rejects(void (^action)(void), NSString *message) {
    BOOL rejected = NO;
    @try { action(); } @catch (NSException *error) { rejected = YES; }
    check(rejected, message);
}
static NSMutableDictionary *releaseFixture(void) {
    return [@{@"tag_name":@"v2.6-ntl0.102", @"draft":@NO, @"prerelease":@NO,
        @"assets":@[@{@"name":@"SlitherNative-2.6-ntl0.102-macOS-Universal.zip", @"size":@123,
          @"digest":[@"sha256:" stringByAppendingString:[@"a" stringByPaddingToLength:64 withString:@"a" startingAtIndex:0]],
          @"browser_download_url":@"https://github.com/channynet/slither.io-sad-mod/releases/download/v2.6-ntl0.102/SlitherNative-2.6-ntl0.102-macOS-Universal.zip"}]} mutableCopy];
}
static void makeZip(NSString *path, NSString *entryName, BOOL link) {
    struct archive *writer = archive_write_new();
    archive_write_set_format_zip(writer);
    check(archive_write_open_filename(writer, path.fileSystemRepresentation) == ARCHIVE_OK, @"create fixture ZIP");
    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, entryName.UTF8String);
    archive_entry_set_filetype(entry, link ? AE_IFLNK : AE_IFREG);
    archive_entry_set_perm(entry, 0755);
    archive_entry_set_size(entry, link ? 0 : 3);
    if (link) archive_entry_set_symlink(entry, "../../outside");
    check(archive_write_header(writer, entry) == ARCHIVE_OK, @"write fixture header");
    if (!link) archive_write_data(writer, "abc", 3);
    archive_entry_free(entry); archive_write_close(writer); archive_write_free(writer);
}
int main(int argc, const char *argv[]) {
    @autoreleasepool {
        check(newer(@"v2.6-ntl0.100", @"2.6-ntl0.99"), @"numeric version comparison");
        check(!newer(@"v2.6-ntl0.99", @"2.6-ntl0.100"), @"no downgrade");
        check(!newer(@"v2.6-ntl0.102", @"2.6-ntl0.102"), @"no reinstall");
        rejects(^{ newer(@"v2.6-ntl0.102-beta", @"2.6-ntl0.101"); }, @"reject unofficial versions");
        NSMutableDictionary *release = releaseFixture();
        check(selectAsset(release, @"2.6-ntl0.101") != nil, @"accept official Mac package");
        release[@"draft"] = @YES;
        check(!selectAsset(release, @"2.6-ntl0.101"), @"ignore draft");
        release = releaseFixture(); release[@"prerelease"] = @YES;
        check(!selectAsset(release, @"2.6-ntl0.101"), @"ignore prerelease");
        release = releaseFixture(); release[@"assets"] = @[];
        check(!selectAsset(release, @"2.6-ntl0.101"), @"ignore Windows-only release");
        for (NSString *field in @[@"digest", @"browser_download_url", @"size"]) {
            NSMutableDictionary *fixture = releaseFixture();
            NSMutableDictionary *asset = [fixture[@"assets"][0] mutableCopy];
            asset[field] = [field isEqual:@"size"] ? (id)@0 : (id)@"invalid";
            fixture[@"assets"] = @[asset];
            rejects(^{ selectAsset(fixture, @"2.6-ntl0.101"); }, @"reject invalid metadata");
        }
        NSMutableDictionary *duplicate = releaseFixture();
        duplicate[@"assets"] = @[duplicate[@"assets"][0], duplicate[@"assets"][0]];
        rejects(^{ selectAsset(duplicate, @"2.6-ntl0.101"); }, @"reject duplicate package");
        for (NSString *path in @[@"../escape", @"/absolute", @"root/../../escape", @"root\\escape", @"root/./file"])
            check(!safeEntry(path), @"reject unsafe path");
        NSString *temporary = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        NSFileManager *fm = NSFileManager.defaultManager;
        [fm createDirectoryAtPath:temporary withIntermediateDirectories:YES attributes:nil error:nil];
        @try {
            NSString *zip = [temporary stringByAppendingPathComponent:@"fixture.zip"];
            NSString *out = [temporary stringByAppendingPathComponent:@"out"];
            NSString *file = [out stringByAppendingPathComponent:@"root/program"];
            makeZip(zip, @"root/program", NO);
            extract(zip, out, @"root");
            check([[NSString stringWithContentsOfFile:file encoding:NSUTF8StringEncoding error:nil] isEqual:@"abc"], @"extract contents");
            check(access(file.fileSystemRepresentation, X_OK) == 0, @"preserve executable permissions");
            check([sha256(file) isEqual:@"sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"], @"known SHA-256 vector");
            [fm removeItemAtPath:out error:nil];
            makeZip(zip, @"../escape", NO);
            rejects(^{ extract(zip, out, @"root"); }, @"reject archive traversal");
            makeZip(zip, @"root/link", YES);
            rejects(^{ extract(zip, out, @"root"); }, @"reject archive symlink");
            makeZip(zip, @"wrong/program", NO);
            rejects(^{ extract(zip, out, @"root"); }, @"reject wrong package root");
            NSString *current = [temporary stringByAppendingPathComponent:@"current.app"];
            NSString *backup = [temporary stringByAppendingPathComponent:@"backup.app"];
            [fm createDirectoryAtPath:current withIntermediateDirectories:YES attributes:nil error:nil];
            [fm createDirectoryAtPath:backup withIntermediateDirectories:YES attributes:nil error:nil];
            [@"old" writeToFile:[current stringByAppendingPathComponent:@"version"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
            [@"new" writeToFile:[backup stringByAppendingPathComponent:@"version"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
            check(renamex_np(current.fileSystemRepresentation, backup.fileSystemRepresentation, RENAME_SWAP) == 0, @"atomic installation");
            check([[NSString stringWithContentsOfFile:[current stringByAppendingPathComponent:@"version"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"new"], @"new app installed");
            check(renamex_np(current.fileSystemRepresentation, backup.fileSystemRepresentation, RENAME_SWAP) == 0, @"atomic rollback");
            check([[NSString stringWithContentsOfFile:[current stringByAppendingPathComponent:@"version"] encoding:NSUTF8StringEncoding error:nil] isEqual:@"old"], @"old app restored");
            if (argc == 3) {
                NSString *package = [NSString stringWithUTF8String:argv[1]];
                NSString *version = [NSString stringWithUTF8String:argv[2]];
                NSString *root = [NSString stringWithFormat:@"SlitherNative-%@-macOS-Universal", version];
                NSString *unpacked = [temporary stringByAppendingPathComponent:@"release"];
                extract(package, unpacked, root);
                NSString *app = [[unpacked stringByAppendingPathComponent:root] stringByAppendingPathComponent:@"SlitherNative.app"];
                validateBundle(app, version);
                check(YES, @"real release ZIP passes updater extraction and signature validation");
                rejects(^{ validateBundle(app, @"0.0-ntl0.0"); }, @"reject wrong bundled version");
            }
        } @finally { [fm removeItemAtPath:temporary error:nil]; }
        printf("PASS: %d Mac updater checks (no game launch or network)\n", checks);
    }
    return 0;
}
