module Inotify
  class Notifier
    attr_reader :fd, :watchers

    def initialize
      @watchers = {}
    end

    def run
      @stop = false
      process until @stop
    end

    def stop
      @stop = true
    end

    def watch(path, *flags, &block)
      wd = add_watch(path, flags)
      @watchers[wd] = { path: path, callback: block }      
      wd
    end

    def unwatch_by_path(path)
      wd, _ = @watchers.find { |wd, w| w[:path] == path }
      fail "no watcher for path #{path}" unless wd
      unwatch_by_wd(wd)
    end

    def unwatch_by_wd(wd)
      rm_watch(wd)
      @watchers.delete(wd)
    end

    def process
      read_events do |event|
        watcher = @watchers[event.wd]
        next unless watcher
        event.watched_path = watcher[:path]
        watcher[:callback].call(event)
      end
    end
  end
end
