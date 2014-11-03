
#Data storage specification

* All blockiverse related data are kept in a databse whose name is based on the name of the realm, indicated here as {RealmName}

* Format of sectors:
```SQL
CREATE TABLE {RealmName}.sectors (
  id INTEGER,
  x_pc BIGINT,
  y_pc BIGINT,
  z_pc BIGINT,
  PRIMARY KEY (id ASC)
);
```
    * x_pc,y_pc,z_pc is the sector's universe coordinate in parsecs
