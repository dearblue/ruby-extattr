#vim: set fileencoding:utf-8

require "tmpdir"
basedir = File.join(Dir.tmpdir, "ruby-extattr.test-work")
Dir.mkdir basedir unless File.directory? basedir
filepath = File.join(basedir, "file1")

require "extattr"

extdata = "abcdefg"

Dir.chdir basedir do
    describe "file" do
        file = nil
        before(:all) do
            file = File.open(filepath, "a")
        end

        it ".extattr_list" do
            file.extattr_list.should eq([])
        end

        after(:all) do
            file.close
            file = nil
        end
    end

    describe File do
        before(:all) do
            File.open(filepath, "a") {}
        end

        it ".extattr_list" do
            File.extattr_list(filepath).should eq([])
        end

        it ".extattr_set" do
            File.extattr_set(filepath, "ext1", extdata).should nil
        end

        it ".extattr_list ((2))" do
            File.extattr_list(filepath).should eq(["ext1"])
        end

        it ".extattr_get" do
            File.extattr_get(filepath, "ext1").should eq(extdata)
        end

        it ".extattr_delete" do
            File.extattr_delete(filepath, "ext1").should nil
        end

        it ".extattr_list ((3))" do
            File.extattr_list(filepath).should eq([])
        end

        after(:all) do
            File.unlink filepath
        end
    end
end
