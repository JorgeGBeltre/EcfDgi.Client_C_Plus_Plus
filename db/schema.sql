-- ---------------------------------------------------------------------------
-- EcfDgii.Client (C++ replica) — PostgreSQL schema.
-- This replaces the EF Core migrations used by the .NET solution. Column names
-- mirror the snake_case mappings declared in the *Configuration.cs classes.
-- Applied automatically at startup by DbInitializer (see Infrastructure/Persistence).
-- ---------------------------------------------------------------------------

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Users ----------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
    id            uuid PRIMARY KEY,
    username      varchar(100) NOT NULL,
    email         varchar(150) NOT NULL,
    password_hash text         NOT NULL,
    role          varchar(50)  NOT NULL,
    created_at    timestamptz  NOT NULL,
    created_by    varchar(100),
    updated_at    timestamptz,
    updated_by    varchar(100),
    deleted_at    timestamptz,
    deleted_by    varchar(100),
    is_deleted    boolean      NOT NULL DEFAULT false
);

CREATE UNIQUE INDEX IF NOT EXISTS uq_users_username ON users (username);
CREATE UNIQUE INDEX IF NOT EXISTS uq_users_email    ON users (email);

-- Customers ------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS customers (
    id         uuid PRIMARY KEY,
    name       varchar(200) NOT NULL,
    email      varchar(150),
    rnc        varchar(20)  NOT NULL,
    created_at timestamptz  NOT NULL,
    created_by varchar(100),
    updated_at timestamptz,
    updated_by varchar(100),
    deleted_at timestamptz,
    deleted_by varchar(100),
    is_deleted boolean      NOT NULL DEFAULT false
);

CREATE INDEX IF NOT EXISTS ix_customers_rnc ON customers (rnc);

-- e-CF documents -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS ecf_documents (
    id            uuid PRIMARY KEY,
    e_ncf         varchar(20)   NOT NULL,
    rnc_emisor    varchar(20)   NOT NULL,
    rnc_comprador varchar(20),
    track_id      varchar(100),
    state         varchar(50)   NOT NULL,
    total_amount  numeric(18,2) NOT NULL,
    itbis_amount  numeric(18,2) NOT NULL,
    security_code varchar(100),
    xml_content   text          NOT NULL,
    receipt_date  timestamptz,
    created_at    timestamptz   NOT NULL,
    created_by    varchar(100),
    updated_at    timestamptz,
    updated_by    varchar(100),
    deleted_at    timestamptz,
    deleted_by    varchar(100),
    is_deleted    boolean       NOT NULL DEFAULT false
);

CREATE UNIQUE INDEX IF NOT EXISTS uq_ecf_documents_rnc_emisor_encf
    ON ecf_documents (rnc_emisor, e_ncf);
CREATE INDEX IF NOT EXISTS ix_ecf_documents_track_id ON ecf_documents (track_id);
CREATE INDEX IF NOT EXISTS ix_ecf_documents_state    ON ecf_documents (state);
