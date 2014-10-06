MRuby::Gem::Specification.new('mruby-inotify') do |spec|
  spec.summary = 'inotify bindings'
  spec.license = 'MIT'
  spec.authors = ['FlavourSys Technology GmbH <technology@flavoursys.com>']

  spec.add_dependency('mruby-io')
  spec.add_dependency('mruby-dir')
  spec.add_dependency('mruby-file-stat')
end
