FLAGS += -Wall -Wextra

hid: hid.c
	${CC} ${FLAGS} hid.c -lusb-1.0 -o hid

clean:
	rm hid
