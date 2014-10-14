# mruby-inotify

inotify bindings for mruby.

## Usage

```ruby
notifier = Inotify::Notifier.new

notifier.watch('/home', :all_events) do |event|
  puts event.inspect
end

# .process blocks until events are available
notifier.process while true
```

Or, to watch a directory tree:

```ruby
notifier = Inotify::RecursiveNotifier.new

notifier.rwatch('/home', :all_events) do |event|
  puts event.inspect
end

notifier.process while true
```
