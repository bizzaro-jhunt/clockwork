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
  name            VARCHAR(100),
  last_seen_at    DATETIME
);

--
-- jobs - Every time cwa runs on a host, it creates a job record.
--
create table jobs (
  id              INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  host_id         INTEGER,
  started_at      DATETIME,
  ended_at        DATETIME,
  duration        INTEGER,
  result          VARCHAR(20)
);

--
-- resources - Every job has multiple resources attached.
--
create table resources (
  id              INTEGER NOT NULL PRIMARY KEY,
  job_id          INTEGER,
  restype         VARCHAR(30),
  name            VARCHAR(100),
  sequence        INTEGER,
  result          VARCHAR(20)
);

--
-- actions - Each resource may have multiple actions attached.
--
create table actions (
  resource_id     INTEGER,
  summary         TEXT,
  sequence        INTEGER,
  result          VARCHAR(20)
);
