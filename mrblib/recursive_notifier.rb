module Inotify
  class RecursiveNotifier < Notifier
    def rwatch(path, *flags, &block)
      flags << [:create] unless flags.include?(:create) || flags.include?(:all_events)
      flags << [:delete_self] unless flags.include?(:delete_self) || flags.include?(:all_events)

      work = [path]
      until work.empty?
        path = work.shift

        begin
          watch(path, *flags) do |event|
            case
            when event.events.include?(:create) && event.events.include?(:isdir)
              rwatch(File.join(event.watched_path, event.name), *flags, &block)
            when event.events.include?(:delete_self)
              @watchers.delete(event.wd)
            end

            block.call(event)
          end
        rescue => e
          # Do not fail if entry was deleted during enumeration of its parent.
          raise unless e.to_s =~ /No such file or directory/
        end
      end
    end
  end
end
