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
import errno
import stat
import logging
from typing import Dict, Optional, Set
from threading import Lock
from io import BytesIO


_LOGGER = logging.getLogger(__name__)

PROGRAM_NAME = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}"

FALLOC_FL_KEEP_SIZE = 1

ERROR_PRIVILEGE_NOT_HELD = 1314
SECTOR_SIZE = 512

fuse = FUSE()

# option variables, used later. We pre-set them to check if they have been changed via user-passed parameters
opt_drv_path = ""
opt_mounting_point = ""


################
### Contexts ###
################

class VirtualFile(object):

    def __init__(self, name: str, mode: int = stat.S_IFREG):
        try:
            self._stream = BytesIO()
            self._name = name
            self._creation_time = datetime.now()
            self._last_access_time = datetime.now()
            self._last_write_time = datetime.now()
            self._mode = mode
            self._uid = 0
            self._gid = 0
            self._parent: Optional[VirtualFile] = None
            self._subfiles: Set[VirtualFile] = set()            
            self._open_count = 0

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
    def uid(self) -> int:
        return self._uid

    @uid.setter
    def uid(self, value: int):
        self._uid = value

    @property
    def gid(self) -> int:
        return self._gid

    @gid.setter
    def gid(self, value: int):
        self._gid = value

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
    def subfiles(self) -> Set['VirtualFile']:
        return self._subfiles

    def add_file(self, vfile: 'VirtualFile'):
        self._subfiles.add(vfile)
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
    
    def open(self):
        self._open_count += 1
        
    def close(self) -> bool:
        self._open_count -= 1 
        return bool(self._open_count == 0)
    

context_holder: Dict[int, VirtualFile] = {}
holder_lock = Lock()
holder_key_counter = 0

########################
### Helper functions ###
########################

drive_root: VirtualFile = VirtualFile('', stat.S_IFDIR)


def user_input(prompt: str) -> str:
    return input(prompt)


def calculate_dir_size(vdir: VirtualFile) -> int:
    result = 0
    for file in vdir.subfiles:
        if stat.S_ISDIR(file.mode):
            result += calculate_dir_size(file)
        else:
            result += file.size
    return result


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
            elif stat.S_ISDIR(vfile.mode):
                curr_dir = vfile
            else:
                break
        else:
            break
    return None


def find_virtual_dir(dir_name: str) -> Optional[VirtualFile]:
    result = find_virtual_file(dir_name)
    if result:
        return result if stat.S_ISDIR(result.mode) else None
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


def cb_error(params: FUSEErrorEventParams) -> None:
    print("Unhandled error %d (%s)" % (params.error_code, params.description))


def cb_stat_fs(params: FUSEStatFSEventParams) -> None:
    _LOGGER.info("cb_stat_fs")
    total_size = 1024 * 1024 * 10
    params.block_size = SECTOR_SIZE
    params.total_blocks = total_size // params.block_size
    params.free_blocks = (total_size -
                          calculate_dir_size(drive_root)) // params.block_size


def cb_get_attrs(params: FUSEGetAttrEventParams) -> None:
    _LOGGER.info("cb_get_attrs %s", params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        params.mode = vfile.mode
        params.uid = vfile.uid
        params.gid = vfile.gid
        params.size = vfile.size
        params.a_time = vfile.last_access_time
        params.c_time = vfile.creation_time
        params.m_time = vfile.last_write_time
    else:
        params.result = -errno.ENOENT

    _LOGGER.info("cb_get_attrs %s result: %s, mode: %s, uid: %s, gid: %s, size: %s, a_time: %s, c_time: %s, m_time: %s",
                 params.path,
                 params.result,
                 params.mode,
                 params.uid,
                 params.gid,
                 params.size,
                 params.a_time,
                 params.c_time,
                 params.m_time,
                 )


def cb_read_dir(params: FUSEReadDirEventParams) -> None:
    _LOGGER.info("cb_read_dir %s", params.path)
    vdir = find_virtual_dir(params.path)
    if vdir:
        if vdir != drive_root:
            fuse.fill_dir(
                filler_context=params.filler_context,
                name=".",
                ino=0,
                link_count=2,
                mode=vdir.mode,
                uid=vdir.uid,
                gid=vdir.gid,
                size=vdir.size,
                a_time=vdir.last_access_time,
                c_time=vdir.creation_time,
                m_time=vdir.last_write_time,
            )
            fuse.fill_dir(
                filler_context=params.filler_context,
                name="..",
                ino=0,
                link_count=2,
                mode=vdir.mode,
                uid=vdir.uid,
                gid=vdir.gid,
                size=vdir.size,
                a_time=vdir.last_access_time,
                c_time=vdir.creation_time,
                m_time=vdir.last_write_time,
            )
        for vfile in vdir.subfiles:
            fuse.fill_dir(
                filler_context=params.filler_context,
                name=vfile.name,
                ino=0,
                link_count=2 if stat.S_ISDIR(vfile.mode) else 1,
                mode=vfile.mode,
                uid=vfile.uid,
                gid=vfile.gid,
                size=vfile.size,
                a_time=vfile.last_access_time,
                c_time=vfile.creation_time,
                m_time=vfile.last_write_time,
            )
    else:
        params.result = -errno.ENOENT


def cb_create(params: FUSECreateEventParams) -> None:
    _LOGGER.info("cb_create %s mode: %s", params.path, params.mode)
    global holder_lock, context_holder, holder_key_counter
    vfile = find_virtual_file(params.path)
    if vfile:
        params.result = -errno.EEXIST
        vfile = None
    else:
        vdir = get_parent_virtual_dir(params.path)
        if not vdir:
            params.result = -errno.ENOENT
        else:
            file_name = get_file_name(params.path)
            vfile = VirtualFile(file_name, params.mode)
            vdir.add_file(vfile)
    if vfile:
        with holder_lock:
            holder_key_counter += 1
            context_holder[holder_key_counter] = vfile
            params.file_context = holder_key_counter
        vfile.open()        
    _LOGGER.info("cb_create %s, result: %s, context: %s",
                 params.path, params.result, params.file_context
                 )


def cb_unlink(params: FUSEUnlinkEventParams) -> None:
    _LOGGER.info("cb_unlink %s", params.path)
    vfile = find_virtual_file(params.path)
    if not vfile:
        params.result = -errno.ENOENT
    else:
        vfile.remove()


def cb_mk_dir(params: FUSEMkDirEventParams) -> None:
    _LOGGER.info("cb_mk_dir %s", params.path)
    vdir = find_virtual_dir(params.path)
    if vdir:
        params.result = -errno.EEXIST
    else:
        vdir = get_parent_virtual_dir(params.path)
        if not vdir:
            params.result = -errno.ENOENT
        else:
            file_name = get_file_name(params.path)
            vdir.add_file(VirtualFile(file_name, params.mode))


def cb_rm_dir(params: FUSERmDirEventParams) -> None:
    _LOGGER.info("cb_rm_dir %s", params.path)
    vdir = find_virtual_dir(params.path)
    if not vdir:
        params.result = -errno.ENOENT
    else:
        vdir.remove()


def cb_chmod(params: FUSEChmodEventParams) -> None:
    _LOGGER.info("cb_chmod %s", params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        vfile.mode = params.mode
    else:
        params.result = -errno.ENOENT


def cb_chown(params: FUSEChownEventParams) -> None:
    _LOGGER.info("cb_chown %s", params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        vfile.uid = params.uid
        vfile.gid = params.gid
    else:
        params.result = -errno.ENOENT


def cb_utime(params: FUSEUTimeEventParams) -> None:
    _LOGGER.info("cb_utime %s", params.path)
    vfile = find_virtual_file(params.path)
    if vfile:
        vfile.last_access_time = params.a_time
        vfile.last_write_time = params.m_time
    else:
        params.result = -errno.ENOENT


def cb_truncate(params: FUSETruncateEventParams) -> None:
    _LOGGER.info("cb_truncate %s", params.path)
    vfile = find_virtual_file(params.path)
    if not vfile:
        params.result = -errno.ENOENT
    else:
        vfile.size = params.size


def cb_rename(params: FUSERenameEventParams) -> None:
    _LOGGER.info("cb_rename %s", params.old_path)
    vfile = find_virtual_file(params.new_path)
    if vfile:
        params.result = -errno.EEXIST
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
                params.result = -errno.ENOENT
        else:
            params.result = -errno.ENOENT


def cb_open(params: FUSEOpenEventParams) -> None:
    _LOGGER.info("cb_open %s", params.path)
    global holder_lock, context_holder, holder_key_counter
    
    vfile = None

    if params.file_context > 0:
        with holder_lock:
            vfile = context_holder.get(params.file_context)
    
    if vfile is None: 
        # The file is being opened anew
        vfile = find_virtual_file(params.path)
    
        if vfile is None:
            params.result = -errno.ENOENT
        else:
            with holder_lock:
                holder_key_counter += 1
                context_holder[holder_key_counter] = vfile
                params.file_context = holder_key_counter

    # if the file has been opened, increment the open counter
    if vfile:
        vfile.open()
    
    _LOGGER.info("cb_open %s, result: %s, context: %s", params.path, params.result, params.file_context)
        

def cb_release(params: FUSEReleaseEventParams) -> None:
    _LOGGER.info("cb_release %s, context: %s", params.path, params.file_context)
    global holder_lock, context_holder
    
    if params.file_context:        
        vfile = None
        with holder_lock:
            vfile = context_holder.get(params.file_context)
    
        if vfile: 
            if vfile.close(): # open_count == 0
                with holder_lock:
                    context_holder.pop(params.file_context, None)
                # params.file_context = 0
        else:
            params.result = -errno.EBADF
    else:
        params.result = -errno.EBADF


def cb_f_allocate(params: FUSEFAllocateEventParams) -> None:
    global holder_lock, context_holder

    if (params.mode & (~FALLOC_FL_KEEP_SIZE)) != 0:
        # if we detect unsupported flags in Mode, we deny the request
        params.result = -errno.EOPNOTSUPP
        return

    vfile: Optional[VirtualFile] = None
    if params.file_context > 0:
        with holder_lock:
            vfile = context_holder.get(params.file_context)
    else:
        vfile = find_virtual_file(params.path)
    if vfile:
        # in this sample, allocation size is not maintained so we don't change it; however, fallocate is used on non-Windows systems to increase file size as well
        # so we do this here
        if (params.mode & FALLOC_FL_KEEP_SIZE) != FALLOC_FL_KEEP_SIZE:
            new_size = params.offset + params.length
            if new_size > vfile.size:
                vfile.size = new_size
    else:
        params.result = -errno.EBADF
    _LOGGER.info("cb_f_allocate %s result: %s", params.path, params.result)


def cb_read(params: FUSEReadEventParams) -> None:
    _LOGGER.info("cb_read %s, context: %s", params.path, params.file_context)
    global holder_lock, context_holder
    vfile: Optional[VirtualFile] = None
    if params.file_context > 0:
        with holder_lock:
            vfile = context_holder.get(params.file_context)
    else:
        vfile = find_virtual_file(params.path)
    if vfile:
        buffer = vfile.read(params.offset, params.size)
        # this is how we get a pointer to the array
        cbuf = c_uint8 * len(buffer)
        pbuf = cbuf.from_buffer_copy(buffer, 0)
        # now copy the data
        ctypes.memmove(params.buffer, pbuf, len(buffer))
        params.result = len(buffer)
    else:
        params.result = -errno.EBADF
    _LOGGER.info("cb_read %s result: %s", params.path, params.result)


def cb_write(params: FUSEWriteEventParams) -> None:
    _LOGGER.info("cb_write %s, context: %s", params.path, params.file_context)
    global holder_lock, context_holder
    vfile: Optional[VirtualFile] = None
    if params.file_context > 0:
        with holder_lock:
            vfile = context_holder.get(params.file_context)
    else:
        vfile = find_virtual_file(params.path)
    if vfile:
        # this is how we get a pointer to the array
        buffer = bytearray(params.size)
        cbuf = c_uint8 * params.size
        pbuf = cbuf.from_buffer(buffer, 0)
        # now copy the data
        ctypes.memmove(pbuf, params.buffer, params.size)
        count = vfile.write(buffer, params.offset, params.size)
        params.result = count
    else:
        params.result = -errno.EBADF
    _LOGGER.info("cb_write %s result: %s", params.path, params.result)


#################
### Main code ###
#################

def banner() -> None:
    print("CBFS Connect Copyright (c) Callback Technologies, Inc.\n\n")


def print_usage() -> None:
    if sys.platform == "win32":
        print("usage: fusememdrive.py [-<switch 1> ... -<switch N>] <mounting point>\n")
        print("<Switches>")
        print("  -drv cab_file - Install drivers from CAB file")
    else:
        print("usage: fusememdrive.py <mounting point>\n")
    print("\n")


def check_driver(module: int, module_name: str) -> bool:
    global fuse

    state = fuse.get_driver_status(PROGRAM_NAME)
    if state == MODULE_STATUS_RUNNING:
        version = fuse.get_driver_version(PROGRAM_NAME)
        print("%s driver is installed, version: %d.%d.%d.%d\n" % (
            module_name, ((version & 0x7FFF000000000000) >>
                          48), ((version & 0xFFFF00000000) >> 32),
            ((version & 0xFFFF0000) >> 16), (version & 0xFFFF)))
        return True
    else:
        print("%s driver is not installed\n" % module_name)
        return False


def handle_parameters() -> int:
    global opt_drv_path, opt_mounting_point

    i = 1
    while i < len(sys.argv):
        cur_param = sys.argv[i]
        # we have a switch
        if cur_param.startswith("-"):
            # driver path
            if cur_param == "-drv" and sys.platform == "win32":
                if i + 1 < len(sys.argv):
                    opt_drv_path = convert_relative_path_to_absolute(sys.argv[i + 1])
                    if os.path.isfile(opt_drv_path) and opt_drv_path.endswith(".cab"):
                        i += 2
                        continue
                print(
                    "-drv switch requires the valid path to the .cab file with drivers as the next parameter.")
                return 1
            else:
                print("Invalid parameter '%s'" % cur_param)
                return 1

        else:
            # we have a mounting point
            if len(cur_param) == 1:
                cur_param += ":"
            opt_mounting_point = convert_relative_path_to_absolute(cur_param, True)
            if opt_mounting_point is None or opt_mounting_point == "":
                print("Error: Invalid Mounting Point Path")
                return 1
            break

        i += 1  # end of loop
    return 0


def is_drive_letter(path_str):
    if not path_str:
        return False

    c = path_str[0]
    if c.isalpha() and len(path_str) == 2 and path_str[1] == ':':
        return True
    else:
        return False


def convert_relative_path_to_absolute(path, accept_mounting_point=False):
    if path and path.strip():
        res = path

        # Linux-specific case of using a home directory
        if path == "~" or path.startswith("~/"):
            home_dir = os.path.expanduser("~")
            if path == "~" or path == "~/":
                return home_dir
            else:
                return os.path.join(home_dir, path[1:])

        semicolon_count = path.count(';')
        is_network_mounting_point = semicolon_count == 2
        remained_path = ""

        if is_network_mounting_point:
            if not accept_mounting_point:
                print(f"The path '{path}' format cannot be equal to the Network Mounting Point")
                return ""
            pos = path.find(';')
            if pos != len(path) - 1:
                res = path[:pos]
                if not res:
                    return path
                remained_path = path[pos:]

        if is_drive_letter(res):
            if not accept_mounting_point:
                print(f"The path '{res}' format cannot be equal to the Drive Letter")
                return ""
            return path

        if not os.path.isabs(res):
            res = os.path.abspath(res)
            return res + remained_path

    return path


def install_driver() -> int:
    global opt_drv_path, fuse
    print("Installing drivers from '%s'\n" % opt_drv_path)
    drv_reboot = False
    try:
        drv_reboot = fuse.install(
            opt_drv_path, PROGRAM_NAME, None, MODULE_DRIVER or MODULE_HELPER_DLL)
    except CBFSConnectError as e:
        if e.code == ERROR_PRIVILEGE_NOT_HELD:
            print(
                "Installation requires administrator rights. Run the app as administrator")
        else:
            print("Drivers are not installed, error %d (%s)" %
                  (e.code, e.message))
        return e.code
    print("Drivers installed successfully")
    if drv_reboot != 0:
        print("Reboot is required by the driver installation routine\n")
    return 0


def run_mounting() -> int:
    global fuse, opt_drv_path, opt_mounting_point

    try:
        if sys.platform == "win32":
            fuse.config("SectorSize=" + str(SECTOR_SIZE))
        # initialize the object and the driver
        fuse.on_error = cb_error
        fuse.on_stat_fs = cb_stat_fs
        fuse.on_get_attr = cb_get_attrs
        fuse.on_read_dir = cb_read_dir
        fuse.on_mk_dir = cb_mk_dir
        fuse.on_rm_dir = cb_rm_dir
        fuse.on_create = cb_create
        fuse.on_unlink = cb_unlink
        fuse.on_chmod = cb_chmod
        fuse.on_chown = cb_chown
        fuse.on_u_time = cb_utime
        fuse.on_rename = cb_rename
        fuse.on_open = cb_open
        fuse.on_release = cb_release
        fuse.on_f_allocate = cb_f_allocate
        fuse.on_read = cb_read
        fuse.on_write = cb_write
        fuse.on_truncate = cb_truncate

        fuse.serialize_events = True

        fuse.initialize(PROGRAM_NAME)
        # attempt to mount

        drive_root.add_file(VirtualFile("sample dir", stat.S_IFDIR))

        fuse.mount(opt_mounting_point)
    except CBFSConnectError as e:
        print("Failed to mount, error %d (%s)" % (e.code, e.message))
        return e.code

    # wait for the user to give a quit command, then attempt to delete the storage and quit
    print("Now you can use a virtual disk on the mounting point %s." %
          opt_mounting_point)
    while True:
        user_input("To finish, press Enter.")
        try:
            fuse.unmount()
        except CBFSConnectError as e:
            print("Failed to unmount, error %d (%s)" % (e.code, e.message))
            continue
        break


def main() -> None:
    try:
        banner()
        if len(sys.argv) < 2:
            print_usage()
            check_driver(MODULE_DRIVER, "CBFSConnect")
        else:
            try:
                handle_parameters()
                op = False
                # check the operation
                if opt_drv_path != "":
                    # install the drivers
                    install_driver()
                    op = True
                if opt_mounting_point != "":
                    run_mounting()
                    op = True
                if not op:
                    print(
                        "Unrecognized combination of parameters passed. You need to either install the driver using -drv switch the mounting point to create a virtual disk")
            except CBFSConnectError as e:
                print("Unhandled error %d (%s)" % (e.code, e.message))
    except Exception as ex:
        print("Exception - %s" % ex)


# Program body
if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main()


