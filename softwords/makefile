SOURCES = softcore.fr jhlocal.fr marker.fr prefix.fr ifbrack.fr oo.fr classes.fr string.fr fileaccess.fr

softcore.c: softcore.pl  $(SOURCES)
	./softcore.pl $(SOURCES) >softcore.c
	cp softcore.c ..

clean: 
	rm *.c

