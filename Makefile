
all: gem

gem:
	gem build extattr-x86-mingw32.gemspec
	gem build extattr.gemspec

rdoc:
	rdoc -e UTF-8 -m README.txt README.txt LICENSE.txt ext/extattr.c

spec:
	rspec rspec/extattr.rb

clean:
	-cd lib/1.9.1 && make clean
	-cd lib/2.0.0 && make clean
