all: nodm nodm-session

nodm: nodm.c
	gcc -o $@ $< -lpam -lpam_misc -Wall

nodm-session: nodm
	ln -fs nodm nodm-session

clean:
	rm -f nodm
