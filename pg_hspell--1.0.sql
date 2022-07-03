-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_hspell" to load this file. \quit

CREATE FUNCTION dhspell_init(internal)
    RETURNS internal AS 'MODULE_PATHNAME', 'dhspell_init'
    LANGUAGE C STRICT;

CREATE FUNCTION dhspell_lexize(internal, internal, internal, internal)
    RETURNS internal AS 'MODULE_PATHNAME', 'dhspell_lexize'
    LANGUAGE C STRICT;

CREATE TEXT SEARCH TEMPLATE hspell (
    INIT = dhspell_init,
    LEXIZE = dhspell_lexize
);
COMMENT ON TEXT SEARCH TEMPLATE hspell IS 'hspell based lemmatizer';

CREATE TEXT SEARCH DICTIONARY hspell (
    TEMPLATE = hspell,
    STOPWORDS = hebrew
);
COMMENT ON TEXT SEARCH DICTIONARY hspell IS 'default hspell dictionary for hebrew language';
