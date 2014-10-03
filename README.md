# mruby-inotify

inotify bindings for mruby.

## Usage

```ruby
notifier = Inotify::Notifier.new

notifier.watch('/home', :all_events) do |event|
  puts event.inspect
end

notifier.run
```
