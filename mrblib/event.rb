module Inotify
  class Event
    attr_reader :name, :cookie, :wd, :events
    attr_accessor :watched_path
  end
end
