-- ---------------------------------------------------------------
--
-- Clockwork Policy Master Reporting Database Schema
--
-- This schema definition is implemented in the policy master
-- reporting database, for consolidating and aggregating host and
-- job run results across an entire implementation.
--
-- This schema works with SQLite's DDL
--
-- ---------------------------------------------------------------


--
-- hosts - Contains all known hosts, popuated and updated
--         at policy master startup.
--
create table hosts (
  id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name            TEXT,
  last_seen_at    INTEGER
);

--
-- jobs - Every time cwa runs on a host, it creates a job record.
--
create table jobs (
  id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  host_id         INTEGER,
  started_at      INTEGER,
  ended_at        INTEGER,
  duration        INTEGER
);

--
-- resources - Every job has multiple resources attached.
--
create table resources (
  id              INTEGER NOT NULL PRIMARY KEY,
  job_id          INTEGER,
  type            TEXT,
  name            TEXT,
  sequence        INTEGER,
  compliant       INTEGER,
  fixed           INTEGER
);

--
-- actions - Each resource may have multiple actions attached.
--
create table actions (
  resource_id     INTEGER,
  summary         TEXT,
  sequence        INTEGER,
  result          INTEGER
);
