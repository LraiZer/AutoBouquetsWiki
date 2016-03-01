# Makefile for building AutoBouquetsWiki

autobouquetswiki: autobouquetswiki.cpp
	$(CXX) -o autobouquetswiki autobouquetswiki.cpp
	$(STRIP) autobouquetswiki

