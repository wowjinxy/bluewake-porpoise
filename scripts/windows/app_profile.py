"""Immutable app PGO inputs for compiler-command based build caches.

Clang does not add its profile to the object depfile. Qualifying the input's
filename with its complete content digest makes a changed profile change every
affected compiler command, even when its original filename and timestamp match.
"""
import hashlib
import os
from pathlib import Path
import stat
import tempfile


class ProfileCacheError(RuntimeError):
    """A cached compiler input no longer matches its content-qualified name."""


def _digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


class AppProfileCache:
    def __init__(self, folder):
        self.folder = Path(folder).resolve()
        self._readable = {}

    @staticmethod
    def _verify(path, digest):
        info = path.lstat()
        if not stat.S_ISREG(info.st_mode) or path.is_symlink() or _digest(path) != digest:
            raise ProfileCacheError(f"cached app profile is corrupt: {path}")

    def _snapshot(self, source):
        # Failure to read this optional input selects the unprofiled build.
        # Cache write failures propagate instead of silently hiding disk errors.
        try:
            stream = Path(source).open("rb")
        except OSError:
            return None
        temporary = None
        try:
            with stream:
                self.folder.mkdir(parents=True, exist_ok=True)
                fd, name = tempfile.mkstemp(prefix=".app-profile-", suffix=".tmp", dir=self.folder)
                temporary = Path(name)
                digest = hashlib.sha256()
                with os.fdopen(fd, "wb") as output:
                    while True:
                        try:
                            block = stream.read(1 << 20)
                        except OSError:
                            return None
                        if not block:
                            break
                        output.write(block)
                        digest.update(block)
                identity = digest.hexdigest()
                target = self.folder / f"app-{identity}.profdata"
                # A hard link atomically publishes a complete file without
                # overwriting another writer's result or its stable timestamp.
                try:
                    os.link(temporary, target)
                except FileExistsError:
                    pass
                self._verify(target, identity)
                return target, identity
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)

    def select(self, source, *, compiler_key, readable):
        """Return a compatible immutable input, or None for an unreadable one.

        `compiler_key` identifies the selected compiler/profdata tool; `readable`
        probes the snapshot, so the probe and subsequent compiler use the same
        bytes. A changed profile or compiler is always probed again.
        """
        snapshot = self._snapshot(source)
        if snapshot is None:
            return None
        path, digest = snapshot
        key = digest, compiler_key
        if key not in self._readable:
            try:
                self._readable[key] = bool(readable(path))
            except OSError:
                self._readable[key] = False
        return path if self._readable[key] else None
