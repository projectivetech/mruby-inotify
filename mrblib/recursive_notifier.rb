module Inotify
  class RecursiveNotifier < Notifier
    def rwatch(path, *flags, &block)
      flags << [:create] unless flags.include?(:create) || flags.include?(:all_events)
      flags << [:delete_self] unless flags.include?(:delete_self) || flags.include?(:all_events)

      paths = [path]
      paths.each do |path|
        paths.concat(
          Dir.entries(path).
            reject { |p| ['.', '..'].include?(p) }.
            map { |p| File.join(path, p) }.
            select { |p| File.directory?(p) }
        )
      end

      fail "directory tree has more than #{Inotify.max_user_watches} entries" if paths.size > Inotify.max_user_watches

      until paths.empty?
        path = paths.shift

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
