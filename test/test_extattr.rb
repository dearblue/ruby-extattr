#!ruby

require "test/unit"
require "fileutils"
require "tmpdir"

include FileUtils

require "extattr"

using ExtAttr

class ExtAttr::Test < Test::Unit::TestCase
  WORKDIR = Dir.mktmpdir(["", ".ruby-extattr.test-work"])
  FILEPATH1 = File.join(WORKDIR, "file1")
  FILEPATH2 = File.join(WORKDIR, "file2")

  def self.startup
    @@file = File.open(FILEPATH1, "a")
  end

  def self.shutdown
    @@file.close
    @@file = nil
    Dir.chdir "/" rescue nil
    rmtree WORKDIR, secure: true, verbose: true #, noop: true
  end

  def test_fd_extattr
    extdata = "abcdefg"

    assert_equal([], @@file.extattr_list)
    assert_nil(@@file.extattr_set("ext1", extdata))
    assert_equal(["ext1"], @@file.extattr_list())
    assert_equal("ext1", `lsextattr -qq user #{@@file.to_path}`.chomp) if RUBY_PLATFORM =~ /freebsd/
    assert_equal(extdata, @@file.extattr_get("ext1"))
    assert_equal(extdata, `getextattr -qq user ext1 #{@@file.to_path}`) if RUBY_PLATFORM =~ /freebsd/
    assert_nil(@@file.extattr_delete("ext1"))
    assert_equal([], @@file.extattr_list())
  end

  def test_extattr
    extdata = "abcdefg"
    File.open(FILEPATH2, "ab") {}

    assert_equal([], File.extattr_list(FILEPATH2))
    assert_nil(File.extattr_set(FILEPATH2, "ext1", extdata))
    assert_equal(["ext1"], File.extattr_list(FILEPATH2))
    assert_equal(extdata, File.extattr_get(FILEPATH2, "ext1"))
    assert_nil(File.extattr_delete(FILEPATH2, "ext1"))
    assert_equal([], File.extattr_list(FILEPATH2))
  end

  def test_ractor_extattr
    # Skip this test on Ruby < 3.0
    return true unless defined?(Ractor)

    # Ractor is still experimental in Ruby 3.0.x — suppress warning for this test.
    old_warning_status = Warning[:experimental]
    Warning[:experimental] = false

    extdata = "abcdefg"
    File.open(FILEPATH2, "ab") {}

    assert_equal([], File.extattr_list(FILEPATH2))
    assert_nil(File.extattr_set(FILEPATH2, "ext1", extdata))

    assert_equal(["ext1"], Ractor.new(FILEPATH2) { |path| File.extattr_list(path) }.take)
    assert_equal(extdata, Ractor.new(FILEPATH2) { |path| File.extattr_get(path, "ext1") }.take)

    assert_nil(File.extattr_delete(FILEPATH2, "ext1"))
    assert_equal([], File.extattr_list(FILEPATH2))
    Warning[:experimental] = old_warning_status
  end
end
