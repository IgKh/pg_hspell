CREATE EXTENSION pg_hspell;

SELECT ts_lexize('hspell', 'הרכבת');
SELECT ts_lexize('hspell', 'רק');
SELECT ts_lexize('hspell', 'איןמילהכזו');
SELECT ts_lexize('hspell', 'ישראל');

SELECT ts_lexize('hspell', 'English');
SELECT ts_lexize('hspell', 'Русский');
