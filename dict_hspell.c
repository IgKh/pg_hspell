/*
 * dict_hspell.c
 *
 * Copyright (c) 2022 Igor Khanin
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// PostgreSQL server API includes
#include <postgres.h>

#include <commands/defrem.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>
#include <nodes/pg_list.h>
#include <tsearch/ts_public.h>
#include <utils/elog.h>

// hspell includes
#include <hspell.h>
#include <linginfo.h>

PG_MODULE_MAGIC;

typedef struct DictHspell
{
    struct dict_radix* dict;
    StopList	       stoplist;
}
DictHspell;

static const unsigned char MISC_STEM[] = {0xF9, 0xE5, 0xF0, 0xE5, 0xFA, 0};

static List* stem_list;

PG_FUNCTION_INFO_V1(dhspell_init);

Datum
dhspell_init(PG_FUNCTION_ARGS)
{
    List* options = (List *)PG_GETARG_POINTER(0);

    bool        stopwords_loaded = false;
    DictHspell* d;
    ListCell*   l;
    int         rc;

    d = (DictHspell*)palloc0(sizeof(DictHspell));

    rc = hspell_init(&d->dict, HSPELL_OPT_LINGUISTICS);
    if (rc) {
        ereport(ERROR,
                (errcode(ERRCODE_CONFIG_FILE_ERROR),
                 errmsg("Can't load hspell dictionary from \"%s\", rc = %d",
                        hspell_get_dictionary_path(), rc)));
    }

    foreach(l, options) {
        DefElem* defel = (DefElem *)lfirst(l);

        if (strcmp(defel->defname, "stopwords") == 0) {
            if (stopwords_loaded) {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("Multiple stopwords parameters to hspell")));
            }
            readstoplist(defGetString(defel), &d->stoplist, NULL);
            stopwords_loaded = true;
        }
        else {
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("Unrecognized hspell parameter: \"%s\"", defel->defname)));
        }
    }

    PG_RETURN_POINTER(d);
}

static bool
has_hebrew(const char* word_iso)
{
    const unsigned char* ptr;

    if (word_iso == NULL) {
        return false;
    }

    ptr = (const unsigned char*)word_iso;
    while (*ptr) {
        if (*ptr >= 0xE0 && *ptr <= 0xFA) { // Alef to Tav in ISO-8859-8
            return true;
        }
        ptr++;
    }
    return false;
}

static char*
get_hspell_token(const char* token, int token_len)
{
    char* recoded_token = NULL;

    PG_TRY();
    {
        recoded_token = pg_server_to_any(token, token_len, PG_ISO_8859_8);
    }
    PG_CATCH();
    {
        // Failure to convert token into ISO-8859-8 means that hspell
        // won't be able to handle it, so don't recognize the token.
        return NULL;
    }
    PG_END_TRY();

    if (!has_hebrew(recoded_token)) {
        // Don't recognize tokens that don't contain any Hebrew letters,
        // we want those to possibly go to another dictionary.
        if (recoded_token != token) {
            pfree(recoded_token);
        }
        return NULL;
    }
    return recoded_token;
}

static int
hspell_callback(const char* word, const char* baseword, int preflen, int prefspec)
{
    char* desc;
    char* stem;

    if (*baseword == '\0') {
        // Don't index prefix only matches
        return 0;
    }

    linginfo_lookup(baseword, &desc, &stem);

    for (int i = 0; ; i++) {
        char* stem_text = linginfo_stem2text(stem, i);
        if (!stem_text) {
            break;
        }
        if ((linginfo_desc2ps(desc, i) & prefspec) == 0) {
            continue;
        }

        if (memcmp(stem_text, MISC_STEM, sizeof(MISC_STEM)) == 0) {
            continue;
        }

        if (!list_member_ptr(stem_list, stem_text)) {
            stem_list = lappend(stem_list, stem_text);
        }
    }
    return 0;
}

PG_FUNCTION_INFO_V1(dhspell_lexize);

Datum
dhspell_lexize(PG_FUNCTION_ARGS)
{
    DictHspell* d      = (DictHspell*)PG_GETARG_POINTER(0);
    char*       in     = (char*)PG_GETARG_POINTER(1);
    int32       in_len = PG_GETARG_INT32(2);

    char*     token;
    char*     recoded_token;
    TSLexeme* res;
    size_t    i;
    ListCell* l;

    token = palloc0(in_len + 1);
    strncpy(token, in, in_len);

    recoded_token = get_hspell_token(token, in_len);
    if (recoded_token == NULL) {
        pfree(token);
        PG_RETURN_POINTER(NULL);
    }

    // Search stop words with the original token in the server encoding, but do
    // it after get_hspell_token() so we are sure to ignore non-Hebrew
    if (searchstoplist(&d->stoplist, token)) {
        res = palloc0(sizeof(TSLexeme));
    }
    else {
        stem_list = NIL;
        hspell_enum_splits(d->dict, recoded_token, hspell_callback);

        if (list_length(stem_list) == 0) {
            // Since we know the token has some Hebrew, and it is not a
            // valid word in hspell's dictionary - accept it as-is
            stem_list = lappend(stem_list, recoded_token);
        }

        res = palloc0(sizeof(TSLexeme) * (list_length(stem_list) + 1));

        i = 0;
        foreach(l, stem_list) {
            char* stem = (char *)lfirst(l);

            char* stem_recoded = pg_any_to_server(stem, strlen(stem), PG_ISO_8859_8);
            if (stem_recoded == stem) {
                stem_recoded = pstrdup(stem);
            }

            res[i].lexeme = stem_recoded;
            res[i].nvariant = i;
            i++;
        }

        list_free(stem_list);
    }

    if (recoded_token != token) {
        pfree(recoded_token);
    }
    pfree(token);

    PG_RETURN_POINTER(res);
}
