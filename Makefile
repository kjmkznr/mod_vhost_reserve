APXS := $(shell which apxs2 || which apxs)

mod_vhost_reserve.o: mod_vhost_reserve.c
	$(APXS) -c $(CFLAGS) mod_vhost_reserve.c

install: mod_vhost_reserve.o
	$(APXS) -i $(CFLAGS) mod_vhost_reserve.la

.PHONY: clean
clean:
	-rm -f mod_vhost_reserve.o mod_vhost_reserve.lo mod_vhost_reserve.slo mod_vhost_reserve.la

start:
	$(APACHECTL) start
restart:
	$(APACHECTL) restart
stop:
	$(APACHECTL) stop

