#!/usr/bin/env python3
"""初始化 config.db：创建 egw_head + egw_manifest + 业务表"""

import sqlite3, sys

SCHEMA_SQL = """
CREATE TABLE egw_head (
    schema_version INTEGER NOT NULL
);
INSERT INTO egw_head (schema_version) VALUES (1);

CREATE TABLE egw_manifest (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
INSERT INTO egw_manifest(key, value) VALUES ('southbound', 'modbus_rtu');
INSERT INTO egw_manifest(key, value) VALUES ('northbound', 'modbus_tcp');
INSERT INTO egw_manifest(key, value) VALUES ('route', '');

CREATE TABLE southbound (
    device_id INTEGER NOT NULL,
    sig_id INTEGER NOT NULL,
    func_code INTEGER NOT NULL,
    reg_addr INTEGER NOT NULL,
    reg_count INTEGER NOT NULL,
    ctype INTEGER NOT NULL,
    poll_interval_ms INTEGER NOT NULL DEFAULT 1000,
    flags INTEGER NOT NULL DEFAULT 1,
    scale REAL NOT NULL DEFAULT 1.0,
    offset REAL NOT NULL DEFAULT 0.0,
    deadband REAL NOT NULL DEFAULT 0.0,
    PRIMARY KEY (device_id, sig_id)
);

CREATE TABLE northbound (
    device_id INTEGER NOT NULL,
    sig_id INTEGER NOT NULL,
    func_code INTEGER NOT NULL,
    reg_addr INTEGER NOT NULL,
    ctype INTEGER NOT NULL,
    flags INTEGER NOT NULL DEFAULT 1,
    scale REAL NOT NULL DEFAULT 1.0,
    offset REAL NOT NULL DEFAULT 0.0,
    deadband REAL NOT NULL DEFAULT 0.0,
    PRIMARY KEY (device_id, sig_id)
);

CREATE TABLE route (
    device_id INTEGER NOT NULL,
    sig_id INTEGER NOT NULL,
    ctype INTEGER NOT NULL,
    PRIMARY KEY (device_id, sig_id)
);
INSERT INTO route(device_id, sig_id, ctype) VALUES (1, 100, 4);
INSERT INTO route(device_id, sig_id, ctype) VALUES (1, 200, 8);
INSERT INTO route(device_id, sig_id, ctype) VALUES (2, 100, 1);

INSERT INTO southbound(device_id, sig_id, func_code, reg_addr, reg_count, ctype)
    VALUES (1, 100, 3, 0, 2, 4);
INSERT INTO southbound(device_id, sig_id, func_code, reg_addr, reg_count, ctype)
    VALUES (1, 200, 3, 2, 2, 8);
INSERT INTO southbound(device_id, sig_id, func_code, reg_addr, reg_count, ctype)
    VALUES (2, 100, 1, 0, 1, 1);

INSERT INTO northbound(device_id, sig_id, func_code, reg_addr, ctype)
    VALUES (1, 100, 3, 30001, 4);
INSERT INTO northbound(device_id, sig_id, func_code, reg_addr, ctype)
    VALUES (1, 200, 3, 30003, 8);
INSERT INTO northbound(device_id, sig_id, func_code, reg_addr, ctype)
    VALUES (2, 100, 1, 10001, 1);
"""

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "config.db"
    conn = sqlite3.connect(path)
    conn.executescript(SCHEMA_SQL)
    conn.commit()
    conn.close()
    print(f"initialized {path}")

if __name__ == "__main__":
    main()
