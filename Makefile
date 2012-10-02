
extlib:
	cd ext && make

gem: extlib
	-mkdir lib
	cp ext/extattr.so lib/
	gem build extattr.gemspec
	gem compile extattr-0.1.gem

rdoc:
	rdoc -e UTF-8 -m README.txt README.txt LICENSE.txt ext/extattr.c

spec:
	rspec rspec/extattr.rb

clean:
	- cd ext && make clean
