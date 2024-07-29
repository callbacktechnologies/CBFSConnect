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


from cbfsconnect import *

# input = sys.hexversion < 0x03000000 and raw_input or input
input = input

import os
from cbfsconnect import dttm_to_ftime
from threading import Lock
import ctypes
import traceback

PROGRAM_NAME = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}"

ERROR_HANDLE_EOF = 38
ERROR_INVALID_PARAMETER = 87
ERROR_PRIVILEGE_NOT_HELD = 1314

FILE_ATTRIBUTE_DIRECTORY = 16
FILE_ATTRIBUTE_REPARSE_POINT = 1024

FILE_WRITE_ATTRIBUTES = 0x0100
FILE_SHARE_ALL = 0x1 | 0x2 | 0x4  # FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
FILE_ATTRIBUTE_NORMAL = 0x80
OPEN_EXISTING = 3

FILE_OPEN = 1
FILE_OPEN_IF = 3

SECTOR_SIZE = 512

cache: CBCache = None
disk: CBFS = None
volume_label = "Cached Folder Drive"

# option variables, used later. We pre-set them to check if they have been changed via user-passed parameters
opt_local = False
opt_network = False
opt_icon_path = ""
opt_drv_path = ""
opt_folder = ""
opt_mounting_point = ""
opt_permitted_process = ""


########################
### Helper functions ###
########################

def six_input(prompt):
    if sys.version_info[0] == 3:
        return input(prompt)
    # else:
    #    return raw_input(prompt)


def include_traling_path_delimiter_ex(path, path_delimiter):
    if path == "": return path_delimiter
    return path if (path[-1] == path_delimiter) else path + path_delimiter


################
### Contexts ###
################

context_holder = {}
holder_lock = Lock()
holder_key_counter = 0


class FileInfo:

    def __init__(self, path, name):
        self._path = path
        self._name = name
        self._size = 0
        self._attr = 0
        self._ctime = None
        self._mtime = None
        self._atime = None
        self._file_id = 0
        self._reparse_tag = 0

    @property
    def name(self):
        return self._name

    @property
    def attributes(self):
        return self._attr

    @property
    def size(self):
        return self._size

    @property
    def creation_time(self):
        return self._ctime

    @property
    def modification_time(self):
        return self._mtime

    @property
    def last_access_time(self):
        return self._atime

    @property
    def file_id(self):
        return self._file_id

    @property
    def reparse_tag(self):
        return self._reparse_tag

    def convert_time(self, atime):
        return datetime.fromtimestamp(atime, tz=timezone.utc)
        pass

    def obtain_info_from_stats(self, file_stats):
        self._attr = file_stats.st_file_attributes
        self._size = file_stats.st_size
        self._file_id = file_stats.st_ino
        if (self._attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0:
            try:
                self._reparse_tag = file_stats.st_reparse_tag
            except:
                pass

        self._ctime = self.convert_time(file_stats.st_ctime)
        self._mtime = self.convert_time(file_stats.st_mtime)
        self._atime = self.convert_time(file_stats.st_atime)

    def obtain_info(self):
        try:
            file_stats = os.lstat(self._path)
            self.obtain_info_from_stats(file_stats)
            return True
        except:
            # traceback.print_exc()
            return False


class CBFSContext:
    def __init__(self):
        self.id = 0


class EnumContext(CBFSContext):

    def __init__(self, path, mask):
        super().__init__()
        self.path = path
        self.mask = mask
        self.entries = None
        self.filenames = []
        self.cur_item = -1
        self._invalid = False

    def enumerate(self):
        try:
            # when there is a generic mask specified, most likely all items will be used and we can collect the filesystem stats about all items
            # when there's a more sophisticated mask present, it makes sense to pick just names, filter them by mask, and then retrieve filesystem stats about only the used entries
            if (self.mask == "*") or (self.mask == "*.*"):
                self.entries = os.scandir(self.path)
            else:
                for name in os.listdir(self.path):
                    if disk.file_matches_mask(self.mask, name, False):
                        self.filenames.append(name)
                self.cur_item = 0
            return True
        except:
            return False

    def file_info_from_dir_entry(self, dir_entry):
        file_stats = dir_entry.stat(follow_symlinks=False)
        finfo = FileInfo(dir_entry.path, dir_entry.name)
        finfo.obtain_info_from_stats(file_stats)
        return finfo

    def get_next_file_info(self):
        global disk
        if self.entries is not None:
            try:
                dir_entry = next(self.entries)
            except Exception as ex:
                dir_entry = None
            if dir_entry is None:
                self.entries.close()
                return None
            else:
                return self.file_info_from_dir_entry(dir_entry)
        elif self.cur_item == -1:  # list not populated
            return None
        elif self.cur_item < len(self.filenames):
            name = self.filenames[self.cur_item]
            self.cur_item += 1
            finfo = FileInfo(self.path, name)
            finfo.obtain_info()
            return finfo
        # catch-all
        return None

    @property
    def invalid(self):
        return self._invalid

    @invalid.setter
    def invalid(self, value):
        self._invalid = value

    def __del__(self):
        if self.entries is not None:
            self.entries.close()


class FileContext(CBFSContext):

    def __init__(self, name):
        super().__init__()
        self.counter_lock = Lock()
        self.counter = 0
        self.hfile = None
        self.fd = None
        self.name = name

    def increment(self):
        # with self.counter_lock:
        self.counter += 1

    def decrement(self):
        # with self.counter_lock:
        self.counter -= 1


######################
### Event handlers ###
######################

def cb_error(params: CBFSErrorEventParams):
    print("Unhandled error %d (%s)" % (params.error_code, params.description))


def cb_can_file_be_deleted(params: CBFSCanFileBeDeletedEventParams):
    params.can_be_deleted = True


def cb_close_directory_enum(params: CBFSCloseDirectoryEnumerationEventParams):
    global context_holder, holder_lock, holder_key_counter

    try:
        context = None
        if params.enumeration_context != 0:
            with holder_lock:
                try:
                    context = context_holder.get(params.enumeration_context)
                except Exception as ex:
                    pass
        if context is not None:
            # do nothing here, actually
            pass
    except Exception as ee:
        params.result_code = -1


def cb_close_file(params: CBFSCloseFileEventParams):
    global context_holder, holder_lock, holder_key_counter, cache
    context = None

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass

    if context is not None:
        with context.counter_lock:
            context.decrement()
            # maybe we need to close the underlying file
            if context.counter == 0:
                try:
                    # We close the file here because this particular call to file_close_ex is fully synchronous - it flushes and purges the cached data immediately.
                    # If flushing is postponed, then a handler of the CBCache's WriteData event would have to check for the completion or cancellation of writing, and close the file there.
                    # Also, the order of calls is important - first, the file is closed in the cache, then the backend file is closed.
                    if context.hfile is not None:
                        cache.file_close_ex(params.file_name, FLUSH_IMMEDIATE, PURGE_NONE)
                        context.hfile.close()
                except Exception as ee:
                    pass
                context.hfile = None
                context.fd = None
                # remove the context from the dictionary
                with holder_lock:
                    context_holder.pop(params.file_context)


def cb_create_file(params: CBFSCreateFileEventParams):
    global opt_folder, context_holder, holder_lock, holder_key_counter, cache

    hfile = None
    fd = None
    is_dir = (params.attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY
    try:
        try:
            if is_dir:
                if ":" in params.file_name:
                    params.result_code = 267  # ERROR_DIRECTORY
                    return

                os.mkdir(opt_folder + params.file_name)
            else:
                fd = os.open(opt_folder + params.file_name, os.O_CREAT | os.O_RDWR | os.O_EXCL | os.O_BINARY)
                hfile = os.fdopen(fd, "bx")

                attrval = ctypes.c_uint32(
                    params.attributes & 0xFFFF)  # Attributes contains creation flags as well, which we need to strip
                ctypes.windll.kernel32.SetFileAttributesW(ctypes.c_wchar_p(opt_folder + params.file_name), attrval)
        except OSError as ex:
            params.result_code = ex.winerror
            return

        # create a context object
        context = FileContext(params.file_name)
        context.hfile = hfile
        context.fd = fd
        context.increment()

        # register the context in the global dictionary
        with holder_lock:
            holder_key_counter += 1
            context_holder[holder_key_counter] = context
            context.id = holder_key_counter

        params.file_context = context.id

        # open the file in the cache
        cache.file_open_ex(params.file_name, 0, False, 0, 0, PREFETCH_NOTHING, context.id)

    except Exception as ex:
        params.result_code = -1


def cb_open_file(params: CBFSOpenFileEventParams):
    global opt_folder, context_holder, holder_lock, holder_key_counter, cache

    context = None
    try:

        # check if such file is already opened
        if params.file_context != 0:
            with holder_lock:
                try:
                    context = context_holder.get(params.file_context)
                except Exception as ex:
                    pass

        # we already have an opened backend file
        if context is not None:
            with context.counter_lock:
                context.increment()
                return

        # new file must be opened
        hfile = None
        fd = None
        is_dir = (params.attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY

        # create a context object
        try:
            if is_dir:
                # do nothing as we don't open directories on Windows
                pass
            else:
                fd = os.open(opt_folder + params.file_name, os.O_RDWR | os.O_BINARY)
                hfile = os.fdopen(fd, "br+")
        except OSError as ex:
            try:
                fd = os.open(opt_folder + params.file_name, os.O_RDONLY | os.O_BINARY)
                hfile = os.fdopen(fd, "br+")
            except OSError as ex:
                params.result_code = ex.winerror
                return

        if (params.nt_create_disposition != FILE_OPEN) and (params.nt_create_disposition != FILE_OPEN_IF):
            attrval = ctypes.c_uint32(
                params.attributes & 0xFFFF)  # Attributes contains creation flags as well, which we need to strip
            ctypes.windll.kernel32.SetFileAttributesW(ctypes.c_wchar_p(opt_folder + params.file_name), attrval)

        context = FileContext(params.file_name)
        context.hfile = hfile
        context.fd = fd
        context.increment()

        # register the context in the global dictionary
        with holder_lock:
            holder_key_counter += 1
            context_holder[holder_key_counter] = context
            context.id = holder_key_counter
            if context.counter == 1:
                file_size = os.path.getsize(opt_folder + params.file_name)
            if fd != None:
                cache.file_open(params.file_name, file_size, PREFETCH_NOTHING, context.id)

        params.file_context = context.id


    except Exception as ex:
        params.result_code = -1


def cb_enumerate_directory(params: CBFSEnumerateDirectoryEventParams):
    global opt_folder, context_holder, holder_lock, holder_key_counter, cache

    context = None

    # obtain the enumeration context object
    if params.enumeration_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.enumeration_context)
            except Exception as ex:
                pass

    try:
        if params.restart and context is not None:
            params.enumeration_context = 0
            # remove the context from the dictionary
            with holder_lock:
                context_holder.pop(params.enumeration_context)
            context = None

        params.file_found = False

        if context is None:
            context = EnumContext(opt_folder + params.directory_name, params.mask)

            # register the context in the global dictionary
            with holder_lock:
                holder_key_counter += 1
                context_holder[holder_key_counter] = context
                context.id = holder_key_counter

            params.enumeration_context = context.id

            if not context.enumerate():
                context.invalid = True

        if not context.invalid:
            file_info = context.get_next_file_info()
            if file_info is not None:
                params.file_found = True
                params.file_name = file_info.name
                params.size = file_info.size
                try:
                    fname = params.directory_name + "/" + file_info.name
                    if cache.file_exists(fname):
                        params.size = cache.file_get_size(fname)
                except Exception as ee:
                    pass

                params.attributes = file_info.attributes
                params.reparse_tag = file_info.reparse_tag
                params.file_id = file_info.file_id
                params.last_write_time = file_info.modification_time
                params.last_access_time = file_info.last_access_time
                params.creation_time = file_info.creation_time

                params.change_time = params.last_write_time

    except OSError as ex:
        params.result_code = ex.winerror
    except Exception as ex:
        params.result_code = -1
        params.file_found = False

    if params.file_found == False:
        # remove the context
        # remove the context from the dictionary
        if context is not None:
            with holder_lock:
                context_holder.pop(params.enumeration_context)
            context = None
        params.enumeration_context = 0


def cb_get_file_info(params: CBFSGetFileInfoEventParams):
    global opt_folder, cache

    try:
        file_info = FileInfo(opt_folder + params.file_name, "")  # we don't use the name in this object instance
        if file_info.obtain_info():
            params.file_exists = True
            params.size = file_info.size
            try:
                if cache.file_exists(params.file_name):
                    params.size = cache.file_get_size(params.file_name)
            except Exception as ee:
                pass
            params.attributes = file_info.attributes
            params.file_id = file_info.file_id
            params.last_write_time = file_info.modification_time
            params.last_access_time = file_info.last_access_time
            params.creation_time = file_info.creation_time
            params.change_time = params.last_write_time
            params.reparse_tag = file_info.reparse_tag
        else:
            params.file_exists = False
    except OSError as ex:
        params.result_code = ex.winerror
    except Exception as ee:
        params.result_code = -1
        params.file_exists = False


def cb_get_volume_id(params):
    params.volume_id = 0x12345678


def cb_get_volume_label(params):
    global volume_label
    try:
        params.volume_label = volume_label

    except OSError as ex:
        params.result_code = ex.winerror
    except Exception as ee:
        params.result_code = -1


def cb_get_volume_size(params: CBFSGetVolumeSizeEventParams):
    global opt_folder

    try:
        # Request the size from the system
        free_bytes_available = ctypes.c_ulonglong(0)
        total_number_of_bytes = ctypes.c_ulonglong(0)
        total_number_of_free = ctypes.c_ulonglong(0)
        ctypes.windll.kernel32.GetDiskFreeSpaceExW(ctypes.c_wchar_p(opt_folder), ctypes.pointer(free_bytes_available),
                                                   ctypes.pointer(total_number_of_bytes),
                                                   ctypes.pointer(total_number_of_free))
        # return the values
        params.available_sectors = total_number_of_free.value // SECTOR_SIZE
        params.total_sectors = total_number_of_bytes.value // SECTOR_SIZE

    except OSError as ex:
        params.result_code = ex.winerror
    except Exception as ex:
        params.result_code = -1


def cb_delete_file(params: CBFSDeleteFileEventParams):
    global opt_folder, cache

    full_name = opt_folder + params.file_name
    try:
        if os.path.isdir(full_name):
            os.rmdir(full_name)
        else:
            os.remove(full_name)
            cache.file_delete(params.file_name)
    except OSError as ex:
        params.result_code = ex.winerror
        return
    except Exception as ee:
        params.result_code = -1


def cb_read_file(params: CBFSReadFileEventParams):
    global context_holder, holder_lock, cache
    context = None

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass
        try:
            if context is not None:
                buf = bytearray(params.bytes_to_read)

                if not (cache.file_read(params.file_name, params.position, buf, 0, params.bytes_to_read)):
                    params.result_code = -1
                else:
                    params.bytes_read = params.bytes_to_read

                    if params.bytes_read > 0:
                        # this is how we get a pointer to the array
                        cbuf = c_uint8 * (params.bytes_read)
                        pbuf = cbuf.from_buffer(buf, 0)
                        # now copy the data
                        ctypes.memmove(params.buffer, pbuf, params.bytes_read)

        except OSError as ex:
            params.result_code = ex.winerror
        except Exception as ee:
            params.result_code = -1


def cb_rename_file(params: CBFSRenameOrMoveFileEventParams):
    global opt_folder, cache

    old_name = opt_folder + params.file_name
    new_name = opt_folder + params.new_file_name
    try:
        os.replace(old_name, new_name)

        if cache.file_exists(params.new_file_name):
            cache.file_delete(params.new_file_name)
        cache.file_change_id(params.file_name, params.new_file_name)

    except OSError as ex:
        params.result_code = ex.winerror
    except Exception as ee:
        params.result_code = -1


def cb_set_allocation_size(params):
    # do nothing - we don't deal with file allocation sizes
    pass


def cb_set_file_attr(params: CBFSSetFileAttributesEventParams):
    global opt_folder, context_holder, holder_lock
    context = None

    full_name = opt_folder + params.file_name
    try:
        if params.attributes != 0:  # 0 means that the attributes are not to be changed
            attrval = ctypes.c_int32(params.attributes)
            ctypes.windll.kernel32.SetFileAttributesW(ctypes.c_wchar_p(full_name), attrval)

        context = None

        if params.file_context != 0:
            with holder_lock:
                try:
                    context = context_holder.get(params.file_context)
                except Exception as ex:
                    pass

        zero_date = datetime(1970, 1, 1, 0, 0, 0)

        if context is not None:
            ctime = c_ulonglong(
                dttm_to_ftime(params.creation_time)) if params.creation_time != zero_date else c_ulonglong(0)
            atime = c_ulonglong(
                dttm_to_ftime(params.last_access_time)) if params.last_access_time != zero_date else c_ulonglong(0)
            wtime = c_ulonglong(
                dttm_to_ftime(params.last_write_time)) if params.last_write_time != zero_date else c_ulonglong(0)

            with context.counter_lock:
                hFile = ctypes.windll.kernel32.CreateFileW(ctypes.c_wchar_p(full_name), FILE_WRITE_ATTRIBUTES,
                                                           FILE_SHARE_ALL, None, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                                           None)
                if hFile != 0:
                    res = ctypes.windll.kernel32.SetFileTime(hFile, byref(ctime), byref(atime), byref(wtime))
                    ctypes.windll.kernel32.CloseHandle(hFile)

    except OSError as ex:
        traceback.print_exc()
        params.result_code = ex.winerror
    except Exception as ee:
        traceback.print_exc()
        params.result_code = -1


def cb_set_file_size(params: CBFSSetFileSizeEventParams):
    global context_holder, holder_lock, cache
    context = None

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass
        try:
            if context is not None:
                with context.counter_lock:
                    context.hfile.truncate(params.size)
                    cache.file_set_size(params.file_name, params.size, False)
        except OSError as ex:
            params.result_code = ex.winerror
        except Exception as ee:
            params.result_code = -1
    else:
        params.result_code = -1  # no context means something is wrong


def cb_set_volume_label(params: CBFSSetVolumeLabelEventParams):
    global volume_label
    volume_label = params.volume_label
    pass


def cb_write_file(params: CBFSWriteFileEventParams):
    global context_holder, holder_lock, cache
    context = None

    params.bytes_written = 0

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass
        try:
            if context is not None:
                with context.counter_lock:
                    buf = bytearray(params.bytes_to_write)
                    # this is how we get a pointer to the array
                    cbuf = c_uint8 * (params.bytes_to_write)
                    pbuf = cbuf.from_buffer(buf, 0)
                    # now copy the data
                    ctypes.memmove(pbuf, params.buffer, params.bytes_to_write)
                    # we should pass `bytes` as a buffer
                    if not (cache.file_write(params.file_name, params.position, buf, 0, params.bytes_to_write)):
                        params.result_code = -1
                    else:
                        params.bytes_written = params.bytes_to_write

        except OSError as ex:
            params.result_code = ex.winerror
        except Exception as ee:
            params.result_code = -1
    else:
        params.result_code = -1  # no context means something is wrong


############################
### Cache Event handlers ###
############################


def cb_cache_read_data(params: CBCacheReadDataEventParams):
    global context_holder, holder_lock

    params.bytes_read = 0
    if (params.flags & RWEVENT_CANCELED) == RWEVENT_CANCELED:
        return

    if params.bytes_to_read == 0:
        return

    context = None

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass

        try:
            if context is not None:
                buf = bytearray(params.bytes_to_read)

                with context.counter_lock:
                    context.hfile.seek(params.position)
                    params.bytes_read = context.hfile.readinto(buf)

                    if params.bytes_read > 0:
                        # this is how we get a pointer to the array
                        cbuf = c_uint8 * (params.bytes_read)
                        pbuf = cbuf.from_buffer(buf, 0)
                        # now copy the data
                        ctypes.memmove(params.buffer, pbuf, params.bytes_read)

            if params.bytes_read == 0:
                params.result_code = RWRESULT_FILE_FAILURE
            else:
                if params.bytes_read < params.bytes_to_read:
                    params.result_code = RWRESULT_PARTIAL

        except Exception as ex:
            params.result_code = RWRESULT_FILE_FAILURE


def cb_cache_write_data(params: CBCacheWriteDataEventParams):
    global context_holder, holder_lock

    if (params.flags & RWEVENT_CANCELED) == RWEVENT_CANCELED:
        return

    if params.bytes_to_write == 0:
        return

    context = None

    if params.file_context != 0:
        with holder_lock:
            try:
                context = context_holder.get(params.file_context)
            except Exception as ex:
                pass

        try:
            if context is not None:
                buf = bytearray(params.bytes_to_write)
                # this is how we get a pointer to the array
                cbuf = c_uint8 * (params.bytes_to_write)
                pbuf = cbuf.from_buffer(buf, 0)
                # now copy the data
                ctypes.memmove(pbuf, params.buffer, params.bytes_to_write)
                context.hfile.seek(params.position)
                params.bytes_written = context.hfile.write(buf)

            if params.bytes_written == 0:
                params.result_code = RWRESULT_FILE_FAILURE
            else:
                if params.bytes_written < params.bytes_to_write:
                    params.result_code = RWRESULT_PARTIAL

        except Exception as ex:
            params.result_code = RWRESULT_FILE_FAILURE


#################
### Main code ###
#################

def banner():
    print("CBFS Connect Copyright (c) Callback Technologies, Inc.\n\n")


def print_usage():
    print("usage: cached_folder_drive.py [-<switch 1> ... -<switch N>] <mapped folder> <mounting point>\n")
    print("<Switches>")
    print("  -lc - Mount disk for current user only")
    print("  -n - Mount disk as network volume")
    print("  -ps (pid|proc_name) - Add process, permitted to access the disk")
    print("  -drv cab_file - Install drivers from CAB file")
    print("  -i icon_path - Set overlay icon for the disk")
    print("\n")


def check_driver(module, module_name):
    global disk
    if disk is None:
        disk = CBFS()
    state = disk.get_driver_status(PROGRAM_NAME, module)
    if state == MODULE_STATUS_RUNNING:
        version = disk.get_module_version(PROGRAM_NAME, module)
        print("%s driver is installed, version: %d.%d.%d.%d\n" % (
            module_name, ((version & 0x7FFF000000000000) >> 48), ((version & 0xFFFF00000000) >> 32),
            ((version & 0xFFFF0000) >> 16), (version & 0xFFFF)))
        return True
    else:
        print("%s driver is not installed\n" % (module_name))
        return False


def install_driver():
    global opt_drv_path, disk
    if disk is None:
        disk = CBFS()
    print("Installing drivers from '%s'\n" % opt_drv_path)
    drv_reboot = False
    try:
        drv_reboot = disk.install(opt_drv_path, PROGRAM_NAME, None, MODULE_DRIVER or MODULE_HELPER_DLL,
                                  INSTALL_REMOVE_OLD_VERSIONS)
    except CBFSConnectError as e:
        if e.code == ERROR_PRIVILEGE_NOT_HELD:
            print("Installation requires administrator rights. Run the app as administrator")
        else:
            print("Drivers are not installed, error %d (%s)" % (e.code, e.message))
        return e.code
    print("Drivers installed successfully")
    if drv_reboot != 0:
        print("Reboot is required by the driver installation routine\n")
    return 0


def install_icon():
    global opt_icon_path, disk
    if disk is None:
        disk = CBFS()
    print("Installing the icon from '%s'\n" % opt_icon_path)
    reboot = False
    try:
        reboot = disk.register_icon(opt_icon_path, PROGRAM_NAME, "sample_icon")
    except CBFSConnectError as e:
        if e.code == ERROR_PRIVILEGE_NOT_HELD:
            print("Icon registration requires administrator rights. Run the app as administrator")
        else:
            print("Icon not registered, error %d (%s)" % (e.code, e.message))
        return e.code
    print("Icon registered successfully")
    if reboot:
        print("Reboot is required by the icon registration routine\n")
    return 0


def handle_parameters():
    global opt_local, opt_network, opt_icon_path, opt_drv_path, opt_folder, opt_mounting_point, opt_permitted_process

    non_dash_param = 0

    i = 1
    while i < len(sys.argv):
        cur_param = sys.argv[i]
        # we have a switch
        if non_dash_param == 0:
            if cur_param.startswith("-"):
                # icon path
                if cur_param == "-i":
                    if i + 1 < len(sys.argv):
                        opt_icon_path = convert_relative_path_to_absolute(sys.argv[i + 1])
                        if os.path.isfile(opt_icon_path):
                            i += 2
                            continue
                    print("-i switch requires the valid path to the .ico file with icons as the next parameter.")
                    return 1

                # user-local disk
                elif sys.platform == "win32" and cur_param == "-lc":
                    opt_local = True

                # network disk
                elif sys.platform == "win32" and cur_param == "-n":
                    opt_network = True

                # permitted process
                elif cur_param == "-ps":
                    if i + 1 < len(sys.argv):
                        cur_param = sys.argv[i + 1]
                        if cur_param.isnumeric():
                            opt_permitted_process = int(cur_param)
                            if opt_permitted_process <= 0:
                                opt_permitted_process = ""
                        else:
                            opt_permitted_process = cur_param
                        if opt_permitted_process != "":
                            i += 2
                            continue
                    print(
                        "Process Id or process name must follow the -ps switch. Process Id must be a positive integer, and a process name may contain an absolute path or just the name of the executable module.")
                    return 1

                # driver path
                elif cur_param == "-drv":
                    if i + 1 < len(sys.argv):
                        opt_drv_path = convert_relative_path_to_absolute(sys.argv[i + 1])
                        if os.path.isfile(opt_drv_path) and opt_drv_path.lower().endswith(".cab"):
                            i += 2
                            continue
                    print("-drv switch requires the valid path to the .cab file with drivers as the next parameter.")
                    return 1

            else:
                # we have a folder
                opt_folder = convert_relative_path_to_absolute(cur_param)
                if opt_folder is not None and (opt_folder.endswith("\\") or opt_folder.endswith("/")):
                    opt_folder = opt_folder[:-1]
                if os.path.isdir(opt_folder):
                    non_dash_param += 1
                    i += 1
                    continue
                else:
                    print("The folder '%s' doesn't seem to exist." % (opt_folder))
                    return 1

        # we have a mounting point
        else:
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


def run_mounting():
    global disk, cache, opt_local, opt_network, opt_icon_path, opt_drv_path, opt_folder, opt_mounting_point, opt_permitted_process

    # opt_folder = include_traling_path_delimiter_ex(opt_folder, "\\")

    cache = CBCache()

    cache_dir = ".\\cache"

    if os.path.exists(cache_dir):
        if not os.path.isdir(cache_dir):
            print("Cache directory exists and is a file")
            return 1
    else:
        os.mkdir(cache_dir)

    cache.set_cache_directory(cache_dir)
    cache.set_cache_file("cache.svlt")
    cache.set_reading_capabilities(RWCAP_POS_RANDOM | RWCAP_SIZE_ANY)
    cache.set_resizing_capabilities(RSZCAP_GROW_TO_ANY | RSZCAP_SHRINK_TO_ANY | RSZCAP_TRUNCATE_AT_ZERO)
    cache.set_writing_capabilities(RWCAP_POS_RANDOM | RWCAP_SIZE_ANY | RWCAP_WRITE_KEEPS_FILESIZE)

    cache.on_read_data = cb_cache_read_data
    cache.on_write_data = cb_cache_write_data
    cache.open_cache("", False)

    if disk is None:
        disk = CBFS()

    # initialize the object and the driver
    disk.initialize(PROGRAM_NAME)
    disk.on_error = cb_error
    disk.on_can_file_be_deleted = cb_can_file_be_deleted
    disk.on_close_directory_enumeration = cb_close_directory_enum
    disk.on_close_file = cb_close_file
    disk.on_create_file = cb_create_file
    disk.on_open_file = cb_open_file
    disk.on_enumerate_directory = cb_enumerate_directory
    disk.on_get_file_info = cb_get_file_info
    disk.on_get_volume_id = cb_get_volume_id
    disk.on_get_volume_label = cb_get_volume_label
    disk.on_get_volume_size = cb_get_volume_size
    disk.on_delete_file = cb_delete_file
    disk.on_read_file = cb_read_file
    disk.on_rename_or_move_file = cb_rename_file
    disk.on_set_allocation_size = cb_set_allocation_size
    disk.on_set_file_attributes = cb_set_file_attr
    disk.on_set_file_size = cb_set_file_size
    disk.on_set_volume_label = cb_set_volume_label
    disk.on_write_file = cb_write_file

    # attempt to create the storage
    try:
        disk.create_storage()
    except CBFSConnectError as e:
        print("Failed to create a storage, error %d (%s)" % (e.code, e.message))
        return e.code

    # Mount a "media"
    try:
        disk.mount_media(0)  # 30000
    except CBFSConnectError as e:
        print("Failed to mount a media, error %d (%s)" % (e.code, e.message))
        return e.code

    # Add a mounting point
    try:
        if len(opt_mounting_point) == 1 and opt_mounting_point[0].isalpha():
            opt_mounting_point = opt_mounting_point + ":"
        flags = 0
        if opt_local: flags = flags + STGMP_LOCAL
        if opt_network: flags = flags + STGMP_NETWORK
        if os.path.isdir(opt_mounting_point): flags = flags + STGMP_MOUNT_MANAGER

        disk.add_mounting_point(opt_mounting_point, flags, 0)
    except CBFSConnectError as e:
        print("Failed to add a mounting point, error %d (%s)" % (e.code, e.message))
        return e.code

    # Set the icon
    if disk.is_icon_registered("sample_icon"):
        disk.set_icon("sample_icon")

    # wait for the user to give a quit command, then attempt to delete the storage and quit
    print("Now you can use a virtual disk on the mounting point %s." % opt_mounting_point)
    while True:
        six_input("To finish, press Enter.")
        try:
            disk.remove_mounting_point(0, "", 0, 0)
        except CBFSConnectError as e:
            print("Failed to remove the mounting point, error %d (%s)" % (e.code, e.message))
            continue
        try:
            # we don't delete the media here, because this is actually optional and will be done in delete_storage
            disk.delete_storage(True)
            print("Disk unmounted")

        except CBFSConnectError as e:
            print("Failed to delete the storage, error %d (%s)" % (e.code, e.message))
            continue

        try:
            if cache.get_active():
                cache.close_cache(FLUSH_IMMEDIATE, PURGE_NONE)
        except CBFSConnectError as e:
            print("Failed to close the cache, error %d (%s)" % (e.code, e.message))

        return 0


# Program body

banner()
if len(sys.argv) < 2:
    print_usage()
    check_driver(MODULE_DRIVER, "Filesystem")
else:
    try:
        handle_parameters()
        op = False
        # check the operation
        if opt_drv_path != "":
            # install the drivers
            install_driver()
            op = True
        if opt_icon_path != "":
            # register the icon
            install_icon()
            op = True
        if opt_mounting_point != "" and opt_folder != "":
            run_mounting()
            op = True
        if not op:
            print(
                "Unrecognized combination of parameters passed. You need to either install the driver using -drv switch or specify the size of the disk and the mounting point name to create a virtual disk")
    except CBFSConnectError as e:
        print("Unhandled error %d (%s)" % (e.code, e.message))


