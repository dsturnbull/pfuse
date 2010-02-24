# FuseFS.rb
#
# The ruby portion of FuseFS main library
#
# Helper functions

# TODO sig handler?

require File.dirname(__FILE__) + '/../ext/fusefs_lib'

module FuseFS
  S_IFDIR = 0o0040000
  S_IFREG = 0o0100000

  @running = true

  class << self
    def run
      fd = fuse_fd

      begin
        io = IO.for_fd(fd)
      rescue Errno::EBADF
        raise "fuse is not mounted"
      end

      while @running
        IO.select([io])
        process
      end
    end

    def unmount
      system("umount #{@mountpoint}")
    end

    def exit
      @running = false
    end

    def Entries(path, entries)
      entries.map do |entry|
        @root.getattr(File.join(path, entry))
      end
    end

  end

  class Stat
    attr_accessor :st_mode
    attr_accessor :st_nlink
    attr_accessor :st_size
    attr_accessor :st_uid
    attr_accessor :st_gid
    attr_accessor :st_mtime
    attr_accessor :st_atime
    attr_accessor :st_ctime
    attr_accessor :name

    def initialize(name='undefined')
      @st_mode  = 0o755 | mode_mask
      @st_nlink = 3
      @st_size  = 0
      @st_uid   = 510
      @st_gid   = 20
      @st_mtime = Time.now.to_i
      @st_atime = Time.now.to_i
      @st_ctime = Time.now.to_i
      @name     = name
    end

    def mode_mask
      self.class == FileEntry ? S_IFREG : S_IFDIR
    end
  end

  class FileEntry < Stat
  end

  class DirEntry < Stat
    def initialize(*args)
      super(*args)
      @st_size = 4096
    end
  end

  class StatVFS
    attr_accessor :f_bsize
    attr_accessor :f_frsize
    attr_accessor :f_blocks
    attr_accessor :f_bfree
    attr_accessor :f_bavail
    attr_accessor :f_files
    attr_accessor :f_ffree
    attr_accessor :f_favail
    attr_accessor :f_fsid
    attr_accessor :f_flag
    attr_accessor :f_namemax

    def initialize
      @f_bsize   = 0
      @f_frsize  = 0
      @f_blocks  = 0
      @f_bfree   = 0
      @f_bavail  = 0
      @f_files   = 0
      @f_ffree   = 0
      @f_favail  = 0
      @f_fsid    = 0
      @f_flag    = 0
      @f_namemax = 0
    end
  end
end
