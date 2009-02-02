nodm: nodm.c
	gcc -o $@ $< -lpam -lpam_misc -Wall

clean:
	rm -f nodm
