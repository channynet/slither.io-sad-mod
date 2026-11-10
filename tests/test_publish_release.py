import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('publish_release', Path(__file__).resolve().parents[1] / 'tools/publish_release.py')
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class PublishTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'tools').mkdir()
        (self.root / 'tools/build_windows.py').write_text('APP_VERSION="2.6-ntl0.102"')
        (self.root / 'release').mkdir()
        self.names = ['SlitherNative-2.6-ntl0.102-Windows-x64.zip',
                      'SlitherNative-2.6-ntl0.102-macOS-Universal.zip', 'SlitherNative-source.zip']
        for name in self.names:
            (self.root / 'release' / name).write_bytes(b'fixture')
        self.calls = []
        self.fail_upload = False
        self.published = False
        self.incomplete = False

    def run_gh(self, args, **kwargs):
        self.calls.append(args)
        if args[2] == 'view' and args[-1] == 'isDraft':
            return subprocess.CompletedProcess(args, 0 if self.published else 1, json.dumps({'isDraft': False}))
        if args[2] == 'upload' and self.fail_upload:
            raise subprocess.CalledProcessError(1, args)
        names = self.names[:1] if self.incomplete else self.names
        return subprocess.CompletedProcess(args, 0, json.dumps({'assets': [{'name': name, 'size': 7} for name in names]}))

    def test_publish_only_after_both_assets_uploaded(self):
        publisher.publish(self.root, 'v2.6-ntl0.102', self.run_gh)
        self.assertEqual([call[2] for call in self.calls], ['view', 'create', 'upload', 'view', 'edit'])
        self.assertIn('--draft', self.calls[1])
        self.assertIn('--draft=false', self.calls[-1])

    def test_missing_mac_package_prevents_any_publication(self):
        (self.root / 'release' / self.names[1]).unlink()
        with self.assertRaises(ValueError):
            publisher.publish(self.root, 'v2.6-ntl0.102', self.run_gh)
        self.assertFalse(self.calls)

    def test_upload_failure_leaves_draft(self):
        self.fail_upload = True
        with self.assertRaises(subprocess.CalledProcessError):
            publisher.publish(self.root, 'v2.6-ntl0.102', self.run_gh)
        self.assertNotIn('edit', [call[2] for call in self.calls])

    def test_incomplete_remote_upload_leaves_draft(self):
        self.incomplete = True
        with self.assertRaises(ValueError):
            publisher.publish(self.root, 'v2.6-ntl0.102', self.run_gh)
        self.assertNotIn('edit', [call[2] for call in self.calls])

    def test_existing_public_release_is_not_overwritten(self):
        self.published = True
        with self.assertRaises(ValueError):
            publisher.publish(self.root, 'v2.6-ntl0.102', self.run_gh)
        self.assertEqual(len(self.calls), 1)

    def test_wrong_tag_prevents_any_publication(self):
        with self.assertRaises(ValueError):
            publisher.publish(self.root, 'v2.6-ntl0.101', self.run_gh)
        self.assertFalse(self.calls)


if __name__ == '__main__':
    unittest.main()
