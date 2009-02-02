pam-helper: pam-helper.c
	gcc -o $@ $< -lpam -lpam_misc -Wall

clean:
	rm pam-helper
