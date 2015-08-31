mod_vhost_reserve
=================

mod_vhost_reserve is a module that limits the slot of vhosts in order to reserve a part of the MaxClients.

Install
-------

How to use
----------

* modules.d/mod_vhost_reserve.conf

```apache
ExtendedStatus On
ReserveSlots 100
```

* vhosts.d/10_example.conf

```apache
<VirtualHost *>
  DocumentRoot /path/to/doc/root
  ServerName example.jp

  VhostLimitEnabled On
</VirtualHost>
```

License
-------
Apache License Version 2.0
