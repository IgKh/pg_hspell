# pg_hspell

A simple and simplistic PostgreSQL extension providing a full text search dictionary template for Hebrew text based on [hspell](http://hspell.ivrix.org.il).

## Overview

The Hebrew language is traditionally considered to be difficult to perform documents retrieval tasks on. Its rich morphology means that words have a very large amount of inflections on one hand, and widespread presence of homographs leads to ambiguity. All of that means that full text search systems tend to suffer from poor recall out of the box when dealing with Hebrew texts.

`pg_hspell` is a PostgreSQL extension that tries to help with such tasks when using the database's built-in full text search subsystem. It uses the dictionary and linguistic information provided by the hspell project to provide a Postgres dictionary template which lemmatizes Hebrew words as part of a configuration pipeline.

* Only Hebrew tokens are dealt with; any token which does not contain any Hebrew letters is not recognized and will be forwarded to any following dictionary in the configuration.

* Hebrew tokens which appear in a configurable stop words list are dropped.

* For tokens which are valid entries in hspell's dictionary, all possible lemmas are emitted as different variants.

* All other tokens are emitted as-is.

## Requirements

* A GNU/Linux system

* PostgreSQL 9.6+ - earlier versions may also work, but are not officially supported anyway

* libhspell, compiled with the `--enable-linginfo` configure flag (distribution packages typically are)

## Installation

Currently `pg_hspell` has to be built from source code. Make sure you have development support files (headers etc) for PostgreSQL and hspell available.

To build and install, run:
```
$ make install
```

This will build against and install into the PostgreSQL installation determined by the first instance of `pg_config` found in the current PATH. To target a specific installation (or one not in PATH):

```
$ make install PG_CONFIG=/path/to/pg_config
```

## Usage

To load the extension into a database, execute the following SQL command as a suitably permissioned user:

```
CREATE EXTENSION pg_hspell;
```

This will place into the current schema a full text dictionary called `hspell` which is configured with a bundled list of common Hebrew stop words. To create a dictionary with a different stop word list (or none at all), do something like the following SQL command:

```
CREATE TEXT SEARCH DICTIONARY my_hspell_dict (
    TEMPLATE = hspell,
    [ STOPWORDS = my_stop_words_file ]
);
```

As with all other text search dictionary templates, the stop words list must be a file saved in the database server's `$SHAREDIR/tsearch_data` directory and having a `.stop` extension. The above example will look for `$SHAREDIR/tsearch_data/my_stop_words_file.stop`

The dictionary then needs to be integrated into a text search configuration, as described in the [PostgreSQL manual](https://www.postgresql.org/docs/current/textsearch-configuration.html). This extension does not provide any predefined configurations.

As the `hspell` directory template will only recognize tokens containing Hebrew characters, it only makes sense to set dictionaries based on it in mappings for `word`, `hword` and `hword-part` type tokens (assuming the default parser).

## Notes and Caveats

### Lemma Ambiguity

As noted above, when a token is recognized by hspell's dictionary ALL possible lemmas for it are emitted (e.g for the word `הרכבת` - `רכבת`, `הרכבה`, and `הרכיב` are all emitted). This is beneficial for recall, but can of course hurt precision. At this time, this extension does not perform any morphological disambiguation - and anyhow this requires sentence context, which PostgreSQL's full text search framework doesn't really make straightforward to use in dictionary templates.

### Stop Words

Stop word filtering is applied on input tokens themselves. Stop word ambiguity is particularly problematic with Hebrew (for examples, see this [blog post](https://code972.com/blog/2010/05/challenges-with-indexing-hebrew-texts-hebmorph-part-1-18)), so take that into account when creating a stop word list, or considering if you want to use one at all.

### A note about dotted text (Niqqud)

hspell can't take advantage of dotted text, and in fact such text even can't be represented in the ISO-8859-8 encoding which is what libhspell works with. Currently, tokens containing Niqqud therefore won't be recognized by this dictionary template!

If processing dotted text is desired, Niqqud has to be stripped prior to passing text to a `pg_hspell` dictionary. This can be achieved by creating a suitable rule set for the [unaccent](https://www.postgresql.org/docs/current/unaccent.html) module. In the future, `pg_hspell` might perform this directly.

### A note about parsing

Please note that the default text search parser included with PostgreSQL does not correctly handle corner cases specific to Hebrew where characters usually considered to be  punctuation (i.e. apostrophe and quotation mark) do not act as such when embedded into a work. Such cases are common in Hebrew computer texts in acronyms and abbreviations, which may not be tokenized as expected.

For example:
```
postgres=# select * from ts_parse('default', $$ נתב"ג $$);
 tokid | token 
-------+-------
     2 | נתב
    12 | "
     2 | ג
```

This is not something specific to `pg_hspell` or within its' scope to address. If there are specific instances that are particularly bothersome, they may be worked around with a [Thesaurus dictionary](https://www.postgresql.org/docs/current/textsearch-dictionaries.html#TEXTSEARCH-THESAURUS).

## License

`pg_hspell` itself is licensed under the Mozilla Public License 2.0. The included stop words list is based on one created by Guy Saar and extracted from https://github.com/fergiemcdowall/stopword, released under the MIT License.
