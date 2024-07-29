# 
# CBFS Connect 2024 Python Edition - Sample Project
# 
# This sample project demonstrates the usage of CBFS Connect in a 
# simple, straightforward way. It is not intended to be a complete 
# application. Error handling and other checks are simplified for clarity.
# 
# www.callback.com/cbfsconnect
# 
# This code is subject to the terms and conditions specified in the 
# corresponding product license agreement which outlines the authorized 
# usage and restrictions.
# 

import sys
import string
from cbfsconnect import *

input = sys.hexversion<0x03000000 and raw_input or input

def ensureArg(args, prompt, index):
  if len(args) <= index:
    while len(args) <= index:
      args.append(None)
    args[index] = input(prompt)
  elif args[index] is None:
    args[index] = input(prompt)


import ctypes
import time
from io import BytesIO
from typing import Optional, List

# Constants
S_IFMT = 0o170000  # Bit mask for the file type bit field
S_IFSOCK = 0o140000  # Socket
S_IFLNK = 0o120000  # Symbolic link
S_IFREG = 0o100000  # Regular file
S_IFBLK = 0o060000  # Block device
S_IFDIR = 0o040000  # Directory
S_IFCHR = 0o020000  # Character device
S_IFIFO = 0o010000  # FIFO
S_ISUID = 0o004000  # Set user ID bit
S_ISGID = 0o002000  # Set group ID bit
S_ISVTX = 0o001000  # Sticky bit
S_IRWXU = 0o0700  # Mask for file owner permissions
S_IRUSR = 0o0400  # Owner has read permission
S_IWUSR = 0o0200  # Owner has write permission
S_IXUSR = 0o0100  # Owner has execute permission
S_IRWXG = 0o0070  # Mask for group permissions
S_IRGRP = 0o0040  # Group has read permission
S_IWGRP = 0o0020  # Group has write permission
S_IXGRP = 0o0010  # Group has execute permission
S_IRWXO = 0o0007  # Mask for permissions for others (not in group)
S_IROTH = 0o0004  # Others have read permission
S_IWOTH = 0o0002  # Others have write permission
S_IXOTH = 0o0001  # Others have execute permission

DIR_MODE = S_IFDIR or S_IRWXU or S_IRGRP or S_IXGRP or S_IROTH or S_IXOTH
FILE_MODE = S_IFREG or S_IWUSR or S_IRUSR or S_IRGRP or S_IROTH

FILE_SYNC4 = 2
NFS4ERR_IO = 5
NFS4ERR_NOENT = 2
NFS4ERR_EXIST = 17

BASE_COOKIE = 0x12345678

################
### Contexts ###
################

class VirtualFile(object):

    def __init__(self, name: str, mode: int = FILE_MODE):
        try:
            self._stream = BytesIO()
            self._name = name
            self._creation_time = datetime.now()
            self._last_access_time = datetime.now()
            self._last_write_time = datetime.now()
            self._mode = mode
            self._parent: Optional[VirtualFile] = None
            self._subfiles: List[VirtualFile] = list()
        except Exception as ex:
            print("Cannot create virtual file. Error %s" % repr(ex))

    @property
    def name(self) -> str:
        return self._name

    @property
    def mode(self) -> int:
        return self._mode

    @mode.setter
    def mode(self, value: int):
        self._mode = value

    @property
    def size(self) -> int:
        return len(self._stream.getvalue())

    @size.setter
    def size(self, value: int) -> None:
        if value < self.size:
            self._stream.truncate(value)
        elif value > self.size:
            # store current position
            pos = self._stream.tell()
            # go to end
            self._stream.seek(self.size)
            # write zeroes
            self._stream.write(b"\x00" * (value - self.size))
            # restore previous file position
            self._stream.seek(pos)

    @property
    def creation_time(self) -> datetime:
        return self._creation_time

    @creation_time.setter
    def creation_time(self, value: datetime):
        self._creation_time = value

    @property
    def last_access_time(self) -> datetime:
        return self._last_access_time

    @last_access_time.setter
    def last_access_time(self, value: datetime):
        self._last_access_time = value

    @property
    def last_write_time(self) -> datetime:
        return self._last_write_time

    @last_write_time.setter
    def last_write_time(self, value: datetime):
        self._last_write_time = value

    @property
    def parent(self) -> 'VirtualFile':
        return self._parent

    @parent.setter
    def parent(self, value: 'VirtualFile') -> None:
        self._parent = value

    @property
    def subfiles(self) -> List['VirtualFile']:
        return self._subfiles

    def add_file(self, vfile: 'VirtualFile'):
        self._subfiles.append(vfile)
        vfile.parent = self

    def get_file(self, file_name: str) -> Optional['VirtualFile']:

        if os.name == "nt":
            file_name_upper = file_name.upper()
            for subfile in self._subfiles:
                if subfile.name.upper() == file_name_upper:
                    return subfile
        else:
            for subfile in self._subfiles:
                if subfile.name == file_name:
                    return subfile

        return None

    def rename(self, new_name: str):
        self._name = new_name

    def remove(self):
        if self._parent:
            self._parent._subfiles.remove(self)
            self._parent = None

    def write(self, write_buf: bytes, position: int, bytes_to_write: int) -> int:
        self._stream.seek(position, 0)
        if self.size - position < bytes_to_write:
            self.size = position + bytes_to_write
        pos = self._stream.tell()
        self._stream.write(write_buf)
        return int(self._stream.tell() - pos)

    def read(self, position: int, bytes_to_read: int) -> bytes:
        self._stream.seek(position, 0)
        read_buf = self._stream.read(bytes_to_read)
        return read_buf


########################
### Helper functions ###
########################

cbfs_nfs = NFS()
drive_root: VirtualFile = VirtualFile('', DIR_MODE)

def find_virtual_file(file_name: str) -> Optional[VirtualFile]:
    if not file_name or file_name == '/':
        return drive_root
    curr_dir = drive_root
    dirs = file_name.split('/')
    for i, subdir in enumerate(dirs):
        if subdir == "":
            continue
        vfile = curr_dir.get_file(subdir)
        if vfile:
            if i == len(dirs) - 1:
                return vfile
            elif vfile.mode or S_IFDIR != 0:
                curr_dir = vfile
            else:
                break
        else:
            break
    return None


def find_virtual_dir(dir_name: str) -> Optional[VirtualFile]:
    result = find_virtual_file(dir_name)
    if result:
        return result if result.mode or S_IFDIR != 0 else None
    else:
        return None


def get_parent_virtual_dir(file_name: str) -> Optional[VirtualFile]:
    try:
        if '/' in file_name:
            last_pos = file_name.rindex('/')
            dir_name = file_name[0: last_pos]
            return find_virtual_dir(dir_name)
        else:
            return None
    except Exception as ex:
        print(repr(ex))
        return None


def get_file_name(full_path: str) -> Optional[str]:
    parts = full_path.split('/')
    return parts[len(parts) - 1]
    
    
######################
### Event handlers ###
######################

def cb_error(params: NFSErrorEventParams) -> None:
    print("Error " + str(params.error_code) + " : " + params.description)


def cb_log(params: NFSLogEventParams) -> None:
    print("[" + str(params.connection_id) + "] " + params.message)


def cb_connected(params: NFSConnectedEventParams) -> None:
    print("Connected: " + str(params.connection_id) + " -> " + str(params.status_code) + " : "  + params.description)
    return


def cb_disconnected(params: NFSDisconnectedEventParams) -> None:
    print("Disconnected: " + str(params.connection_id) + " -> " + str(params.status_code) + " : "  + params.description)
    return
    

def cb_connection_request(params: NFSConnectionRequestEventParams) -> None:
    params.accept = True


def cb_get_attrs(params: NFSGetAttrEventParams) -> None:
    print("cb_get_attrs " + params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        params.mode = vfile.mode
        params.group = "0"
        params.user = "0"
        params.link_count = 1
        params.size = vfile.size
        params.a_time = vfile.last_access_time
        params.c_time = vfile.creation_time
        params.m_time = vfile.last_write_time
    else:
        params.result = NFS4ERR_NOENT


def cb_mk_dir(params: NFSMkDirEventParams) -> None:
    print("cb_mk_dir " + params.path)
    vdir = find_virtual_dir(params.path)
    if vdir:
        params.result = NFS4ERR_EXIST
    else:
        vdir = get_parent_virtual_dir(params.path)
        if not vdir:
            params.result = NFS4ERR_NOENT
        else:
            file_name = get_file_name(params.path)
            vdir.add_file(VirtualFile(file_name, DIR_MODE))


def cb_open(params: NFSOpenEventParams) -> None:
    print("cb_open " + params.path)
    if params.open_type == 1:
        vfile = find_virtual_file(params.path)
        if vfile:
            params.result = NFS4ERR_EXIST
            return
        
        vdir = get_parent_virtual_dir(params.path)
        if not vdir:
            params.result = NFS4ERR_NOENT
            return
            
        file_name = get_file_name(params.path)
        
        vfile = VirtualFile(file_name, FILE_MODE)
        vdir.add_file(vfile)
    else:
        vfile = find_virtual_file(params.path)
        if not vfile:
            params.result = NFS4ERR_NOENT
            
        vfile.last_access_time = datetime.now()


def cb_read(params: NFSReadEventParams) -> None:
    print("cb_read " + params.path)
    if params.count == 0:
        return
    
    vfile = find_virtual_file(params.path)
    if vfile:
        if params.offset >= vfile.size:
            params.count = 0
            params.eof = True
            return
    
        buffer = vfile.read(params.offset, params.count)
        params.buffer[:] = buffer
        
        if params.offset + len(buffer) == vfile.size:
            params.eof = True
        
        params.count = len(buffer)
    else:
        params.result = NFS4ERR_NOENT
        
    print("cb_read result for " + params.path + ": " + str(params.result))


def cb_rename(params: NFSRenameEventParams) -> None:
    print("cb_rename " + params.old_path)
    vfile = find_virtual_file(params.new_path)
    if vfile:
        params.result = NFS4ERR_EXIST
    else:
        vfile = find_virtual_file(params.old_path)
        if vfile:
            vdir = get_parent_virtual_dir(params.new_path)
            if vdir:
                vfile.rename(get_file_name(params.new_path))
                if vfile.parent != vdir:
                    vfile.remove()
                    vdir.add_file(vfile)
                params.result = 0
            else:
                params.result = NFS4ERR_NOENT
        else:
            params.result = NFS4ERR_NOENT


def cb_rm_dir(params: NFSRmDirEventParams) -> None:
    print("cb_rm_dir " + params.path)
    vdir = find_virtual_dir(params.path)
    if not vdir:
        params.result = NFS4ERR_NOENT
    else:
        vdir.remove()


def cb_truncate(params: NFSTruncateEventParams) -> None:
    print("cb_truncate " + params.path)
    vfile = find_virtual_file(params.path)
    if not vfile:
        params.result = NFS4ERR_NOENT
    else:
        vfile.size = params.size
        

def cb_unlink(params: NFSUnlinkEventParams) -> None:
    print("cb_unlink " + params.path)
    vfile = find_virtual_file(params.path)
    if not vfile:
        params.result = NFS4ERR_NOENT
    else:
        vfile.remove()


def cb_utime(params: NFSUTimeEventParams) -> None:
    print("cb_utime " + params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        vfile.last_access_time = params.a_time
        vfile.last_write_time = params.m_time
    else:
        params.result = NFS4ERR_NOENT


def cb_write(params: NFSWriteEventParams) -> None:
    print("cb_write " + params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        count = vfile.write(params.buffer, params.offset, params.count)
        params.count = count
        params.stable = FILE_SYNC4
    else:
        params.result = NFS4ERR_NOENT

    print("cb_write result for " + params.path + ": " + str(params.result))


def cb_read_dir(params: NFSReadDirEventParams) -> None:
    print("cb_read_dir " + params.path)
    
    read_offset = 0
    cookie = BASE_COOKIE  # just an offset to avoid a chance to have cookie set to 0, 1, 2

    # If Cookie != 0, continue listing entries from a specified cookie. Otherwise, start listing entries from the start.
    if params.cookie != 0:
        read_offset = params.cookie - BASE_COOKIE + 1
        cookie = params.cookie + 1
        
    vdir = find_virtual_dir(params.path)
    if vdir:
        for i in range(read_offset, len(vdir.subfiles)):
            vfile = vdir.subfiles[i]
            
            if cbfs_nfs.fill_dir(
                params.connection_id,
                vfile.name,
                0,
                cookie,
                vfile.mode,
                "0",
                "0",
                1,
                vfile.size,
                vfile.last_access_time,
                vfile.last_write_time,
                vfile.creation_time
            ) == 0:
                if i == len(vdir.subfiles) -1:
                    break
                cookie += 1
            else:
                break

    else:
        params.result = NFS4ERR_NOENT


def cb_lookup(params: NFSLookupEventParams) -> None:
    print("cb_lookup " + params.path)

    vfile = find_virtual_file(params.path)
    if vfile:
        return

    vdir = find_virtual_dir(params.path)
    if vdir:
        return

    params.result = NFS4ERR_NOENT
    
    
#################
### Main code ###
#################

def init_nfs():
    cbfs_nfs.on_error = cb_error
    cbfs_nfs.on_log = cb_log
    cbfs_nfs.on_connection_request = cb_connection_request
    cbfs_nfs.on_get_attr = cb_get_attrs
    cbfs_nfs.on_mk_dir = cb_mk_dir
    cbfs_nfs.on_open = cb_open
    cbfs_nfs.on_read = cb_read
    cbfs_nfs.on_read_dir = cb_read_dir
    cbfs_nfs.on_rename = cb_rename
    cbfs_nfs.on_rm_dir = cb_rm_dir
    cbfs_nfs.on_truncate = cb_truncate
    cbfs_nfs.on_unlink = cb_unlink
    cbfs_nfs.on_utime = cb_utime
    cbfs_nfs.on_write = cb_write
    cbfs_nfs.on_connected = cb_connected
    cbfs_nfs.on_disconnected = cb_disconnected
    cbfs_nfs.on_lookup = cb_lookup

# Main
def banner():
    print("CBFS Connect Copyright (c) Callback Technologies, Inc.\n\n")
    print("This demo shows how to create a virtual drive that is stored in memory using the NFS component.\n\n")


def usage():
    print("Usage: python nfs [local port or - for default] <mounting point>\n\n")
    print("Example 1 (any OS): python nfs 2049\n")
    print("Example 2 (Linux/macOS): sudo python nfs - /mnt/mynfs\n\n")
    print("'mount' command should be installed on Linux/macOS to use mounting points!\n")
    print("Automatic mounting to mounting points is supported only on Linux and macOS.\n")
    print("Also, mounting outside of your home directory may require admin rights.\n\n")


def stop_server():
    print("Stopping server...")
    cbfs_nfs.stop_listening()
    print("Server stopped")
    

def main():
    port = 2049

    banner()
    if len(sys.argv) < 2:
        usage()
        return 0

    s_port = sys.argv[1]
    if s_port != "-":
        port = int(s_port)

    if len(sys.argv) == 3 and platform.system() == 'Windows':
        print("Mounting points are not supported on Windows, only on Linux and macOS")
        return 0

    init_nfs()
    
    now = datetime.now()
    vfile = VirtualFile("test", DIR_MODE)
    vfile.creation_time = now
    vfile.last_access_time = now
    vfile.last_write_time = now
    
    drive_root.add_file(vfile)

    if len(sys.argv) == 3:
        cbfs_nfs.config("MountingPoint=" + sys.argv[2])

    cbfs_nfs.local_port = port
    cbfs_nfs.start_listening()

    print("NFS server started on port ", port)
    print("Press Ctrl+C to stop the server")

    try:
        while True:
            cbfs_nfs.do_events()
    except KeyboardInterrupt:
        stop_server()
    
    return 0


if __name__ == "__main__":
    main()




